# include "hpc_network_provider.h"

namespace dsn 
{
    namespace tools
    {
        hpc_sync_client_session::hpc_sync_client_session(
                socket_t sock,
                net_header_format hdr_format,
                network& net,
                ::dsn::rpc_address remote_addr,
                bool is_ipc_socket
            ): rpc_session(net, nullptr, remote_addr, hdr_format, true, false),
             _reader(net.message_buffer_block_size()),
             _is_ipc_socket(is_ipc_socket)
        {
            _connect_state = SS_DISCONNECTED;
            _socket = sock;
            
            memset((void*)&_peer_addr, 0, sizeof(_peer_addr));
            _peer_addr.sin_family = AF_INET;
            _peer_addr.sin_addr.s_addr = INADDR_ANY;
            _peer_addr.sin_port = 0;
        }

        hpc_sync_client_session::~hpc_sync_client_session()
        {

        }

        void hpc_sync_client_session::connect()
        {
            if (SS_DISCONNECTED != _connect_state)
                return;

            _connect_state = SS_CONNECTING;
            _peer_addr.sin_family = AF_INET;
            _peer_addr.sin_addr.s_addr = htonl(remote_address().ip());
            _peer_addr.sin_port = htons(remote_address().port());
            
            errno = 0;
            int rt = connect_socket(_socket, remote_address().ip(), remote_address().port(),
                _is_ipc_socket);
            int err = errno;
            if (rt == -1)
            {
                dwarn("(s = %d) connect to %s failed, err = %s", _socket, remote_address().to_string(), strerror(err));
                on_failure();
            }
            else 
            {
                _connect_state = SS_CONNECTED;
            }
        }

        void hpc_sync_client_session::on_failure() 
        {
            if (SS_DISCONNECTED == _connect_state)
                return;

            _connect_state = SS_DISCONNECTED;
            close();
            on_disconnected();
        }

        void hpc_sync_client_session::close()
        {
            if (-1 != _socket)
            {
                shutdown(_socket, SHUT_RDWR);
                auto err = ::close(_socket);
                if (err == 0)
                    dinfo("(s = %d) close socket %p, ref count = %d", _socket, this, get_count());
                else
                    derror("(s = %d) close socket %p failed, ref count = %d, return = %d, err = %d", _socket, this, get_count(), err, errno);
                _socket = -1;
            }
        }

        void hpc_sync_client_session::disconnect_and_release()
        {
            int fd = _socket;
            int count = (int)get_count();
            on_failure();
            release_ref(); // added in channel

            dinfo("(s = %d) disconnect_and_release is done, ref_count = %d", fd, count - 1);
        }

        void hpc_sync_client_session::send_message(message_ex* msg)
        {
            dassert(_parser, "parser should not be null when send");
            _parser->prepare_on_send(msg);

            auto lcount = _parser->get_buffer_count_on_send(msg);
            send_buf* sbuffers = (send_buf*)alloca(sizeof(send_buf) * lcount);
            int send_buf_start_index = 0;
        
            // fill send buffers
            auto rcount = _parser->get_buffers_on_send(msg, sbuffers);
            dassert(lcount >= rcount, "");

            while (true) 
            {
                int buffer_count = rcount - send_buf_start_index;
                if (buffer_count > net().max_buffer_block_count_per_send())
                {
                    buffer_count = net().max_buffer_block_count_per_send();
                }

                int byte_sent = 0;
# ifdef _WIN32 
                int rt = WSASend(
                    _socket,
                    (LPWSABUF)&sbuffers[send_buf_start_index],
                    (DWORD)buffer_count,
                    (DWORD*)&byte_sent,
                    0,
                    NULL,
                    NULL
                );

                if (SOCKET_ERROR == rt)
                {
                    dwarn("WSASend to %s failed, err = %d", remote_address().to_string(), ::WSAGetLastError());
                    on_failure();
                    return;
                }
# else 
                struct msghdr hdr;
                memset((void*)&hdr, 0, sizeof(hdr));
                hdr.msg_name = (void*)&_peer_addr;
                hdr.msg_namelen = (socklen_t)sizeof(_peer_addr);
                hdr.msg_iov = (struct iovec*)&sbuffers[send_buf_start_index];
                hdr.msg_iovlen = (size_t)buffer_count;

                errno = 0;
                byte_sent = sendmsg(_socket, &hdr, 0);
                if (byte_sent < 0)
                {
                    derror("(s = %d) sendmsg to %s failed, err = %s", _socket, remote_address().to_string(), strerror(errno));
                    on_failure();
                    return;
                }
# endif 

                dinfo("(s = %d) call sendmsg to %s ok, buffer_count = %d, sent bytes = %d",
                    _socket,
                    remote_address().to_string(),
                    buffer_count,
                    byte_sent
                    );
                
                int len = (int)byte_sent;
                int buf_i = send_buf_start_index;
                while (len > 0)
                {
                    auto& buf = sbuffers[buf_i];
                    if (len >= (int)buf.sz)
                    {
                        buf_i++;
                        len -= (int)buf.sz;
                    }
                    else
                    {
                        buf.buf = (char*)buf.buf + len;
                        buf.sz -= len;
                        break;
                    }
                }
                send_buf_start_index = buf_i;

                // message completed, continue next message
                if (send_buf_start_index == rcount)
                {
                    dassert (len == 0, "buffer must be sent completely");
                    return;
                }
            }
        }

        message_ex* hpc_sync_client_session::recv_message()
        {
            int read_next = 1024;
            while (true)
            {
                char* ptr = _reader.read_buffer_ptr(read_next);
                int remaining = _reader.read_buffer_capacity();

                errno = 0;
                int length = recv(_socket, ptr, remaining, 0);
                int err = errno;
                dinfo("(s = %d) call recv on %s, return %d, err = %s",
                    _socket,
                    remote_address().to_string(),
                    length,
                    strerror(err)
                    );

                if (length > 0)
                {
                    _reader.mark_read(length);

                    if (!_parser)
                    {
                        read_next = prepare_parser(_reader);
                    }

                    if (_parser)
                    {
                        message_ex* msg = _parser->get_message_on_receive(&_reader, read_next);
                        if (msg != nullptr) 
                        {
                            msg->from_address = remote_address();
                            msg->to_address = net().address();

                            // TODO: injection for rpc response enqueue without rpc response task

                            ddebug("(s = %d) recv rpc message, id = %d, trace_id = %016" PRIx64,
                                _socket,
                                msg->header->id,
                                msg->header->trace_id
                                );
                            return msg;
                        }
                    }

                    if (read_next == -1)
                    {
                        derror("(s = %d) recv failed on %s, parse failed", 
                            _socket, remote_address().to_string());
                        on_failure();
                        return nullptr;
                    }
                }
                else
                {
                    derror("(s = %d) recv failed on %s, err = %s", 
                            _socket, remote_address().to_string(), strerror(err));
                    on_failure();
                    return nullptr;
                }
            }
        }
    }
}
