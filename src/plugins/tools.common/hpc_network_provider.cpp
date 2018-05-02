/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 * 
 * -=- Robust Distributed System Nucleus (rDSN) -=- 
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Description:
 *     Network atop of io looper.
 *
 * Revision history:
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# if !defined(_WIN32)

# include "hpc_network_provider.h"
# include "io_looper.h"
# include <netinet/tcp.h>
# include <sys/un.h>
# include <dsn/tool-api/view_point.h>

# ifdef __TITLE__
# undef __TITLE__
# endif
# define __TITLE__ "network.provider.hpc"

namespace dsn
{
    namespace tools
    {
        int      connect_socket(socket_t s, uint32_t ip, uint16_t port, bool is_ipc)
        {
            struct sockaddr* addr = nullptr;
            
            if (!is_ipc) {
                struct sockaddr_in tcp_addr;
                tcp_addr.sin_family = AF_INET;
                tcp_addr.sin_addr.s_addr = htonl(ip);
                tcp_addr.sin_port = htons(port);
                addr = (struct sockaddr*)&tcp_addr;

                int ret = ::connect(s, addr, (int)sizeof(tcp_addr));
                int err = errno;

                rpc_address remote(ip, port);
                dinfo("(s = %d) call connect to %s, return %d, err = %s",
                    s,
                    remote.to_string(),
                    ret,
                    strerror(err)
                    );
                errno = err;
                return ret;
            } else {
                struct sockaddr_un unix_addr;
                memset(&unix_addr, 0, sizeof(unix_addr));
                unix_addr.sun_family = AF_UNIX;
                snprintf(unix_addr.sun_path, sizeof(unix_addr.sun_path), "/tmp/rdsn-ipc-%d", (int)port);
                addr = (struct sockaddr*)&unix_addr;
                
                int ret;
                int err;
                for (int i = 0; i < 20*10; i++) {
                    ret = ::connect(s, addr, (int)sizeof(unix_addr));
                    err = errno;
                    if (errno == ENOENT)
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    else
                        break;
                }
                
                if (ret == -1) {
                    derror("(s = %d) call connect to %s, return %d, err = %s",
                        s,
                        unix_addr.sun_path,
                        ret,
                        strerror(err)
                        );
                } else {
                    dinfo("(s = %d) call connect to %s, return %d, err = %s",
                        s,
                        unix_addr.sun_path,
                        ret,
                        strerror(err)
                        );
                }
                
                errno = err;
                return ret;
            }
        }

        socket_t create_socket(int port, bool is_async, bool is_ipc, bool bind_addr)
        {
            socket_t s = -1;
            if (!is_ipc)
                s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            else  {
                s = socket(AF_UNIX, SOCK_STREAM, 0);
            }
            if (s == -1)
            {
                dwarn("socket failed, err = %s", strerror(errno));
                return -1;
            }

            int flags = fcntl(s, F_GETFL, 0);
            dassert (flags != -1, "fcntl failed, err = %s, fd = %d", strerror(errno), s);

            if (is_async && !(flags & O_NONBLOCK))
            {
                flags |= O_NONBLOCK;
                flags = fcntl(s, F_SETFL, flags);
                dassert(flags != -1, "fcntl failed, err = %s, fd = %d", strerror(errno), s);
            }

            int reuse = 1;
            if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(int)) == -1)
            {
                dwarn("setsockopt SO_REUSEADDR failed, err = %s", strerror(errno));
            }

            if (!is_ipc) 
            {
                int nodelay = 1;
                if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(int)) != 0)
                {
                    dwarn("setsockopt TCP_NODELAY failed, err = %s", strerror(errno));
                }

                int keepalive = 1;
                if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char*)&keepalive, sizeof(keepalive)) != 0)
                {
                    dwarn("setsockopt SO_KEEPALIVE failed, err = %s", strerror(errno));
                }
                
# ifdef __APPLE__
                int optval = 1;
                setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, (void *)&optval, sizeof(int));
# endif
            }

            if (bind_addr) 
            {
                struct sockaddr* addr = nullptr;
                
                if (!is_ipc) {
                    struct sockaddr_in tcp_addr;
                    tcp_addr.sin_family = AF_INET;
                    tcp_addr.sin_addr.s_addr = INADDR_ANY;
                    tcp_addr.sin_port = htons((uint16_t)port);
                    addr = (struct sockaddr*)&tcp_addr;

                    if (bind(s, addr, sizeof(tcp_addr)) != 0)
                    {
                        derror("bind failed, port = %d, err = %s", (int)port, strerror(errno));
                        ::close(s);
                        return -1;
                    }
                } else {
                    struct sockaddr_un unix_addr;
                    addr = (struct sockaddr*)&unix_addr;
                    memset(&unix_addr, 0, sizeof(unix_addr));
                    
                    unix_addr.sun_family = AF_UNIX;
                    snprintf(unix_addr.sun_path, sizeof(unix_addr.sun_path), "/tmp/rdsn-ipc-%d", (int)port);
                    unlink(unix_addr.sun_path);

                    if (bind(s, addr, sizeof(unix_addr)) != 0)
                    {
                        derror("bind failed, ipc = %s, err = %s", unix_addr.sun_path, strerror(errno));
                        ::close(s);
                        return -1;
                    }
                }
            }
            
            dinfo("(s = %d) create socket succeeded", s);
            return s;
        }

        int get_sock_port(int fd)
        {
            struct sockaddr_in addr;
            memset((void*)&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = 0;

            socklen_t addr_len = (socklen_t)sizeof(addr);
            if (getsockname(fd, (struct sockaddr*)&addr, &addr_len) == -1)
            {
                dassert(false, "getsockname failed, err = %s", strerror(errno));
            }
            return (int)htons((uint16_t)addr.sin_port);
        }

        static int get_socket_recv_buffer_bytes(socket_t s) 
        {
            int buflen = 0;
            socklen_t i = sizeof(buflen);
            if (getsockopt(s, SOL_SOCKET, SO_RCVBUF, (char*)&buflen, &i) < 0)
            {
                derror("getsockopt SO_RCVBUF failed, err = %s", strerror(errno));
                buflen = 1*1024*1024*1024;
            }
            else {
                dinfo ("getsockopt SO_RCVBUF success, ret = %d", buflen);
            }
            return buflen;
        }

        hpc_network_provider::hpc_network_provider(rpc_engine* srv, network* inner_provider)
            : network(srv, inner_provider)
        {
            _listen_fd = -1;
            _listen_fd_ipc = -1;
            _looper = nullptr;
            _max_buffer_block_count_per_send = 1; // TODO: after fixing we can increase it
        }

        void __hpc_accept_handler(event_loop* lp, int fd, void* ctx, int events)
        {
            auto provider = (hpc_network_provider*)ctx;
            provider->do_accept(false);
        }

        
        void __hpc_accept_ipc_handler(event_loop* lp, int fd, void* ctx, int events)
        {
            auto provider = (hpc_network_provider*)ctx;
            provider->do_accept(true);
        }

        error_code hpc_network_provider::start(net_channel channel, int port, bool client_only)
        {
            if (_listen_fd != -1)
                return ERR_SERVICE_ALREADY_RUNNING;

            _looper = get_io_looper(node(), false);
            
            if (!client_only)
            {
                ddebug("try to listen on port %d", (int)port);
                _listen_fd = create_socket(port, true, false, true);
                if (_listen_fd == -1)
                {
                    dassert(false, "cannot create listen socket");
                }

                if (listen(_listen_fd, SOMAXCONN) != 0)
                {
                    dwarn("listen failed, err = %s", strerror(errno));
                    return ERR_NETWORK_START_FAILED;
                }

                auto lport = get_sock_port(_listen_fd);
                _address.assign_ipv4(get_local_ipv4(), lport);

                // bind for accept
                _looper->bind_io_handle(
                    _listen_fd, 
                    __hpc_accept_handler,
                    this,
                    FD_READABLE
                    );

                if (_enable_ipc_if_possible) {
                    ddebug("try to listen on unix ipc port /tmp/rdsn-ipc-%d", (int)port);
                    _listen_fd_ipc = create_socket(port, true, true, true);
                    if (_listen_fd_ipc == -1)
                    {
                        dassert(false, "cannot create listen socket");
                    }

                    if (listen(_listen_fd_ipc, SOMAXCONN) != 0)
                    {
                        dwarn("listen failed, err = %s", strerror(errno));
                        return ERR_NETWORK_START_FAILED;
                    }

                    // bind for accept
                    _looper->bind_io_handle(
                        _listen_fd_ipc, 
                        __hpc_accept_ipc_handler,
                        this,
                        FD_READABLE
                        );
                }
            } else {
                _address.assign_ipv4(get_local_ipv4(), port);
            }
            return ERR_OK;
        }

        rpc_session* hpc_network_provider::create_client_session(
            net_header_format fmt, 
            ::dsn::rpc_address server_addr,
            bool is_async
            )
        {
            bool is_ipc = _enable_ipc_if_possible && 
                (server_addr.ip() == dsn_primary_address().u.v4.ip 
                || server_addr.ip() == 0x7f000001
                );
            auto sock = create_socket(0, is_async,
                is_ipc,
                false
                );
            dassert(sock != -1, "create client tcp socket failed!");
            if (is_async)
                return new hpc_rpc_session(sock, fmt, *this, server_addr, true, is_ipc);
            else 
                return new hpc_sync_client_session(sock, fmt, *this, server_addr, is_ipc);
        }

        void hpc_network_provider::do_accept(bool ipc)
        {
            while (true)
            {
                socket_t s;
                ::dsn::rpc_address client_addr;

                if (ipc) 
                {
                    s = ::accept(_listen_fd_ipc, NULL, NULL);
                } else {
                    struct sockaddr_in addr;
                    socklen_t addr_len = (socklen_t)sizeof(addr);
                    s = ::accept(_listen_fd, (struct sockaddr*)&addr, &addr_len);
                    client_addr = ::dsn::rpc_address(ntohl(addr.sin_addr.s_addr), ntohs(addr.sin_port));
                }
                if (s != -1)
                {
                    if (!ipc) {
                        int no_delay = 1;
                        if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&no_delay, sizeof(no_delay)) != 0) {
                            derror("set no_delay failed, err = %s", strerror(errno));
                        }
                    }

#ifdef _WIN32
                    u_long flag = 1;
                    int ret = ioctlsocket(fd, FIONBIO, &flag);
#else
                    int ret = fcntl(s, F_SETFL, O_NONBLOCK | fcntl(s, F_GETFL));
#endif
                    if (ret == -1) {
                        derror("set no_block failed, err = %s", strerror(errno));
                    }

                    auto rs = new hpc_rpc_session(s, NET_HDR_INVALID, *this, client_addr, false, ipc);
                    auto err = rs->bind_looper();
                    if (ERR_OK != err && ERR_IO_PENDING != err)
                    {
                        derror("accept failed when bind to io looper for address %s", client_addr.to_string());
                        rs->on_failure();
                        break;
                    }
                }
                else
                {
                    if (errno != EAGAIN && errno != EWOULDBLOCK)
                    {
                        derror("accept failed, err = %s", strerror(errno));
                    }                    
                    break;
                }
            }
        }

        //////////////////////////////////////////////////////////////////
        //
        // hpc rpc session 
        //

        DEFINE_TASK_CODE_RPC(RPC_NETWORK_PING, TASK_PRIORITY_COMMON, THREAD_POOL_IO)

        bool hpc_rpc_session::check_health()
        {
            if (_socket == -1)
                return false;
            else
            {
                dassert(!is_client(), "only server session needs health check");
                
                // not one message is received yet
                if (!parser())
                    return true;

                auto req = message_ex::create_request(RPC_NETWORK_PING);
                req->from_address = net().address();
                req->to_address = remote_address();
                req->u.client.call = nullptr;
                req->header->client.timeout_ms = 0;

                auto resp = req->create_response();
                delete req;
                send_message(resp);
                
                return true;
            }
        }

        void __local_health_timer_cb(event_loop* lp, void* data, void* data2)
        {
            auto this_ = (hpc_rpc_session*)data;
            if (this_->check_health())
            {
                this_->_looper->add_timer(
                    "__local_health_timer_cb",
                    __local_health_timer_cb,
                    data,
                    nullptr,
                    1000 // check for every 5 seconds
                );
            }
            else
            {
                this_->on_failure();
                this_->release_ref(); // added in ctor
            }
        }

        void __hpc_read_write_handler(event_loop* lp, int fd, void* ctx, int events)
        {
            auto s = (hpc_rpc_session*)ctx;
            s->add_ref();

            if (events & FD_READABLE)
                s->start_read_next(256);
            
            if (SS_CONNECTED == s->_connect_state && (events & FD_WRITABLE))
                s->on_send_ready();

            s->release_ref();
        }

        void __hpc_connected_handler(event_loop* lp, int fd, void* ctx, int events)
        {
            auto s = (hpc_rpc_session*)ctx;
            if (SS_CONNECTING != s->_connect_state)
                return;
            s->on_connect_events_ready(events);
        }

        hpc_rpc_session::hpc_rpc_session(
            socket_t sock,
            net_header_format hdr_format,
            network& net,
            ::dsn::rpc_address remote_addr,
            bool is_client,
            bool is_ipc
            )
            : rpc_session(net, nullptr, remote_addr, hdr_format, is_client, true),
             _reader(net.message_buffer_block_size()),
             _is_ipc_socket(is_ipc)
        {
            dassert(sock != -1, "invalid given socket handle");

            _message_count = 0;
            _connect_state = is_client ? SS_DISCONNECTED : SS_CONNECTED;
            _message_sent = 0;
            _delay_server_receive_ms = 0;

            _socket = sock;
            _sending = false;
            _sending_buffer_start_index = 0;
            
            memset((void*)&_peer_addr, 0, sizeof(_peer_addr));
            _peer_addr.sin_family = AF_INET;
            _peer_addr.sin_addr.s_addr = INADDR_ANY;
            _peer_addr.sin_port = 0;
            _looper = get_io_looper(net.node(), is_client);

            if (is_client) _matcher = _looper->matcher();

            if (is_client)
            {
                // default to a very large value
                _socket_recv_buffer_bytes = 1024*1024*1024;
            }
            else
            {
                socklen_t addr_len = (socklen_t)sizeof(_peer_addr);
                if (getpeername(_socket, (struct sockaddr*)&_peer_addr, &addr_len) == -1)
                {
                    derror("(server) getpeername failed, err = %s", strerror(errno));
                }

                _socket_recv_buffer_bytes = get_socket_recv_buffer_bytes(_socket);
            }

            // TODO: client initiated health check
            /*if (!is_client)
            {
                this->add_ref(); // released in health timer callback
                _looper->add_timer(
                    "__local_health_timer_cb",
                    __local_health_timer_cb,
                    this,
                    nullptr,
                    1000 // check for every 5 seconds
                );
            }
            */
            
            _safe_ptr = 0;
            save_to_safe_mgr();
        }

        void hpc_rpc_session::local_connect()
        {
            if (SS_DISCONNECTED != _connect_state)
                return;

            _connect_state = SS_CONNECTING;

            if (-1 == _socket)
            {
                _socket = create_socket(0, true, 
                    _is_ipc_socket,
                    false);
                dassert(_socket != -1, "create client tcp socket failed!");
            }

            errno = 0;
            int rt = connect_socket(_socket, remote_address().ip(), remote_address().port(), _is_ipc_socket);
            int err = errno;
            if (rt == -1 && err != EINPROGRESS)
            {
                dwarn("(s = %d) connect to %s failed, err = %s", _socket, remote_address().to_string(), strerror(err));
                on_failure();
                return;
            }

            // bind for connect
            auto lerr = _looper->bind_io_handle(
                _socket, 
                __hpc_connected_handler,
                this,
                FD_WRITABLE
            );

            if (lerr != ERR_OK && lerr != ERR_IO_PENDING)
            {
                dwarn("(s = %d) connect to %s failed, err = %s", _socket, remote_address().to_string(), lerr.to_string());
                on_failure();
                return;
            }
        }
        
        void hpc_rpc_session::on_connect_events_ready(int mask)
        {
            _looper->unbind_io_handle(_socket, FD_WRITABLE);

            socklen_t addr_len = (socklen_t)sizeof(_peer_addr);
            if (getpeername(_socket, (struct sockaddr*)&_peer_addr, &addr_len) == -1)
            {
                derror("(s = %d) (client) getpeername failed, err = %s",
                    _socket, strerror(errno));
                on_failure();
                return;
            }

            dinfo("(s = %d) client session %s connected",
                _socket,
                remote_address().to_string()
                );
            _socket_recv_buffer_bytes = get_socket_recv_buffer_bytes(_socket);
            _connect_state = SS_CONNECTED;

            if (ERR_OK != bind_looper())
            {
                derror("(s = %d) (client) bind looper failed", _socket);
                on_failure();
                return;
            }

            // start first round send
            on_send_ready();
        }

        void hpc_rpc_session::do_write()
        {
            // this should never happen as we don't
            // send empty packets?
            /*
            if (!_sending_buffers.size())
            {
                _sending = false;
                return ;
            }
            */

            const int flags =
# ifdef __APPLE__
                SO_NOSIGPIPE
# else
                MSG_NOSIGNAL
# endif
                ;

            static_assert (sizeof(send_buf) == sizeof(struct iovec), 
                "make sure they are compatible");

            dbg_dassert(_sending, "");

            // prepare send buffer, make sure header is already in the buffer
            while (true)
            {
                INSTALL_VIEW_POINT("do_write");
                int buffer_count = (int)_sending_buffers.size() - _sending_buffer_start_index;
                struct msghdr hdr;
                memset((void*)&hdr, 0, sizeof(hdr));
                hdr.msg_name = (void*)&_peer_addr;
                hdr.msg_namelen = (socklen_t)sizeof(_peer_addr);
                hdr.msg_iov = (struct iovec*)&_sending_buffers[_sending_buffer_start_index];
                hdr.msg_iovlen = (size_t)buffer_count;

                errno = 0;
                int sz = sendmsg(_socket, &hdr, flags);
                int err = errno;
                dinfo("(s = %d) call sendmsg to %s, buffer_count = %d, return %d, err = %s",
                    _socket,
                    remote_address().to_string(),
                    buffer_count,
                    sz,
                    strerror(err)
                    );

                if (sz < 0)
                {
                    if (err != EAGAIN && err != EWOULDBLOCK)
                    {
                        derror("(s = %d) sendmsg to %s failed, err = %s", _socket, remote_address().to_string(), strerror(err));
                        on_failure();                        
                    }
                    else
                    {
                        // wait for epoll_wait notification
                    }
                    return;
                }
                else
                {
                    int len = (int)sz;
                    int buf_i = _sending_buffer_start_index;
                    while (len > 0)
                    {
                        auto& buf = _sending_buffers[buf_i];
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
                    _sending_buffer_start_index = buf_i;

                    // message completed, continue next message
                    if (_sending_buffer_start_index == (int)_sending_buffers.size())
                    {
                        dassert(len == 0, "buffer must be sent completely");

                        // try next msg recursively
                        on_send_completed();
                        return;
                    }
                    
                    //
                    // this is not reliable, let's sendmsg until we get EAGAIN
                    //
                    //else if (err == EAGAIN)
                    //    return;
                    else
                    {
                        // try next while(true) loop to continue sending current msg
                    }
                }
            }
        }


        void hpc_rpc_session::close()
        {
            if (-1 != _socket)
            {
                _looper->unbind_io_handle(_socket, FD_READABLE | FD_WRITABLE);
                shutdown(_socket, SHUT_RDWR);
                auto err = ::close(_socket);
                if (err == 0)
                    dinfo("(s = %d) close socket %p, ref count = %d", _socket, this, get_count());
                else
                    derror("(s = %d) close socket %p failed, ref count = %d, return = %d, err = %d", _socket, this, get_count(), err, errno);
                _socket = -1;
            }
        }

        error_code hpc_rpc_session::bind_looper()
        {
            init_thread_checker(_looper->native_tid());

            // bind for send/recv
            auto err = _looper->bind_io_handle(
                _socket, 
                __hpc_read_write_handler,
                this,
                FD_READABLE | FD_WRITABLE
                );
            return err;
        }

        void hpc_rpc_session::do_read(int read_next)
        {
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

                        while (msg != nullptr)
                        {
                            if (msg->header->context.u.is_request)
                                this->on_recv_request(msg, 0);
                            else
                            {
                                if (!is_client())
                                {
                                    derror("(s = %d) response is not expected, remote address %s",
                                           _socket, remote_address().to_string());
                                    delete msg;
                                    on_failure();
                                    break;
                                }
                                this->on_recv_response(msg->header->id, msg, 0);
                            }

                            msg = _parser->get_message_on_receive(&_reader, read_next);
                        }
                    }

                    if (read_next == -1)
                    {
                        derror("(s = %d) recv failed on %s, parse failed", 
                            _socket, remote_address().to_string());
                        on_failure();
                        break;
                    }
                    
                    // TODO: is it reliable this way?
                    if (length < remaining && length < _socket_recv_buffer_bytes)
                        break;
                }
                else
                {
                    if (err != EAGAIN && err != EWOULDBLOCK)
                    {
                        if (0 == length)
                            derror("(s = %d) recv returns 0, remote peer %s is closed", 
                                _socket, remote_address().to_string());
                        else
                            derror("(s = %d) recv failed on %s, err = %s", 
                                _socket, remote_address().to_string(), strerror(err));
                        on_failure();
                    }
                    else
                    {
                        // wait for another round of epoll/kevent later
                    }
                    break;
                }
            }
        }

        

        
    }
}

# endif 
