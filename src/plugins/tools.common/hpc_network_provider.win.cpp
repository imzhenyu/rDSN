/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 * 
 * -=- Robust Distributed System Nucleus (rdsn) -=- 
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
 *     What is this file about?
 *
 * Revision history:
 *     xxxx-xx-xx, author, first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */


# include "hpc_network_provider.h"
# ifdef _WIN32
# include <MSWSock.h>
# endif
# include "io_looper.h"

# ifdef __TITLE__
# undef __TITLE__
# endif
# define __TITLE__ "network.provider.hpc"

#include <dsn/tool-api/view_point.h>

namespace dsn
{
    namespace tools
    {
        static void on_send_message_error(message_ex* msg)
        {
            dwarn("send msg failed %s => %s, trace_id = %16" PRIx64 " as rpc session is gone",
                msg->from_address.to_string(),
                msg->to_address.to_string(),
                msg->header->trace_id
            );

            if (msg->header->context.u.is_request &&
                msg->u.client.call)
            {
                msg->u.client.call->enqueue(dsn_rpc_error_t::RPC_ERR_TIMEOUT, nullptr);
            }

            delete msg;
        }

        static void safe_send_message(message_ex* msg, io_looper* looper)
        {
            INSTALL_VIEW_POINT("safe_send_message " + std::to_string(msg->header->trace_id));
            uint64_t safe_ptr = msg->io_session_secret;
            safe_ptr &= (~(0x3FFULL << 54));

            dbg_dassert(looper->is_local(), "must happen in looper thread");
            hpc_rpc_session* s = (hpc_rpc_session*)looper->safe_mgr()->get(safe_ptr);
            if (s)
                s->local_send(msg);
            else
            {
                on_send_message_error(msg);
            }
        }

        static void remote_send_message(event_loop*, void* data, void* data2)
        {
            auto looper = (io_looper*)data;
            auto msg = (message_ex*)data2;

            INSTALL_VIEW_POINT("remote_send_message " + std::to_string(msg->header->trace_id));
            dbg_dassert(looper->is_local(), "must be in local io looper thread");
            dbg_dassert(msg->io_session_secret, "io_session_secret not set");
            safe_send_message(msg, looper);
        }

        struct safe_context
        {
            network* net;
            hpc_rpc_session* s;
            rpc_address remote_addr;
            bool is_client;
        };

        static void remote_send_message_without_safe_ptr(event_loop*, void* data, void* data2)
        {
            auto ctx = (safe_context*)data;
            auto msg = (message_ex*)data2;
            if (ctx->is_client)
            {
                if (ctx->net->is_client_session_exist(ctx->s, ctx->remote_addr))
                {
                    ctx->s->local_send(msg);
                    delete ctx;
                    return;
                }
            }
            else
            {
                if (ctx->net->is_server_session_exist(ctx->s, ctx->remote_addr))
                {
                    ctx->s->local_send(msg);
                    delete ctx;
                    return;
                }
            }

            delete ctx;

            on_send_message_error(msg);
        }

        void hpc_network_provider::send_reply_message(message_ex* msg)
        {
            INSTALL_VIEW_POINT("send_reply_message " + std::to_string(msg->header->trace_id));
            dbg_dassert(msg->io_session_secret, "io_session_secret not set");

            uint64_t safe_ptr = msg->io_session_secret;
            int looper_index = (safe_ptr >> 54);
            io_looper* looper = get_io_looper(task::get_current_node(), looper_index);
            dassert (looper, "cannot find correspondent looper with index %d", looper_index);
            
            if (looper->is_local())
            {
                safe_send_message(msg, looper);
            }
            else
            {
                looper->add_lpc("remote_send_message", remote_send_message, looper, msg);
            }
        }

        hpc_rpc_session::~hpc_rpc_session() 
        {
            dassert(_looper->is_local(), 
                "rpc sessions can only destructed in bound looper thread");

            while (!_messages.is_alone())
            {
                auto msg = CONTAINING_RECORD(_messages.next(), message_ex, dl);
                msg->dl.remove();

                dinfo("msg %d deleted on rpc session deconstructed, trace_id = %016" PRIx64,
                    msg->header->id,
                    msg->header->trace_id
                    );
                delete msg;
            }
            
            _looper->safe_mgr()->destroy(get_safe_ptr(), this);
        }

        void __safe_save_rpc_session_to_safe_mgr(event_loop* lp, void*, void* data2)
        {
            safe_context* ctx = (safe_context*)data2;
            if (ctx->is_client)
            {
                // always distructed in looper thread so it is ok
                if (ctx->net->is_client_session_exist(ctx->s, ctx->remote_addr))
                {
                    ctx->s->save_to_safe_mgr();
                }
            }
            else
            {
                // always distructed in looper thread so it is ok
                if (ctx->net->is_server_session_exist(ctx->s, ctx->remote_addr))
                {
                    ctx->s->save_to_safe_mgr();
                }
            }

            delete ctx;
        }

        void hpc_rpc_session::save_to_safe_mgr()
        {
            if (_looper->is_local())
            {
                if (_safe_ptr == 0)
                {
                    // use high 10 bits to save the looper index 
                    _safe_ptr = _looper->safe_mgr()->save(this) | (((uint64_t)_looper->index()) << 54);
                }
            }
            else
            {
                auto ctx = new safe_context;
                ctx->is_client = is_client();
                ctx->net = &net();
                ctx->s = this;
                ctx->remote_addr = remote_address();

                _looper->add_lpc(
                    "__safe_save_rpc_session_to_safe_mgr",
                    __safe_save_rpc_session_to_safe_mgr,
                    _looper,
                    ctx
                );
            }
        }

        static void safe_remote_connect(event_loop*, void*, void* data2)
        {
            safe_context* ctx = (safe_context*)data2;
            if (ctx->is_client)
            {
                // always distructed in looper thread so it is ok
                if (ctx->net->is_client_session_exist(ctx->s, ctx->remote_addr))
                {
                    ctx->s->local_connect();
                }
            }
            else
            {
                dassert(false, "invalid execution flow");
            }

            delete ctx;
        }

        void hpc_rpc_session::connect()
        {
            if (SS_DISCONNECTED != _connect_state)
                return;

            if (_looper->is_local())
                local_connect();
            else
            {
                auto ctx = new safe_context;
                ctx->is_client = is_client();
                ctx->net = &net();
                ctx->s = this;
                ctx->remote_addr = remote_address();

                _looper->add_lpc("safe_remote_connect", safe_remote_connect, _looper, ctx);
            }
        }

        void remote_disconnect(event_loop*, void* data, void*)
        {
            ((hpc_rpc_session*)data)->disconnect_and_release();
        }

        void hpc_rpc_session::disconnect_and_release()
        {
            if (_looper->is_local())
            {
                int fd = _socket;
                int count = (int)get_count();
                on_failure();
                release_ref(); // added in channel

                dinfo("(s = %d) disconnect_and_release is done, ref_count = %d", fd, count - 1);
            }
            else
            {
                // this is passed safely as ref counter is released in the callback
                _looper->add_lpc("remote_disconnect", remote_disconnect, this);
            }
        }

        void hpc_rpc_session::on_failure()
        {
            if (SS_DISCONNECTED == _connect_state)
                return;

            _connect_state = SS_DISCONNECTED;
            close();
            on_disconnected();

            // clean up sending queue 
            _sending = false;
            _sending_msgs.clear();
            _sending_buffers.clear();

            _sending_buffer_start_index = 0;
        }

        static void __delayed_rpc_session_read_next__(event_loop*, void* ctx, void*)
        {
            auto s = (hpc_rpc_session*)ctx;
            s->start_read_next(256);
            s->release_ref(); // added in start_read_next
        }

        void hpc_rpc_session::start_read_next(int read_next)
        {
            if (_connect_state != SS_CONNECTED)
                return;

            // server only
            if (!is_client())
            {
                int delay_ms = _delay_server_receive_ms;

                // delayed read
                if (delay_ms > 0)
                {
                    _delay_server_receive_ms = 0;
                    this->add_ref(); // released in __delayed_rpc_session_read_next__
                    _looper->add_timer(
                        "__delayed_rpc_session_read_next__",
                        __delayed_rpc_session_read_next__,
                        this,
                        nullptr,
                        delay_ms
                    );
                }
                else
                {
                    do_read(read_next);
                }
            }
            else
            {
                do_read(read_next);
            }
        }


        void hpc_rpc_session::send_message(message_ex* msg)
        {
            if (_looper->is_local())
            {
                local_send(msg);
            }
            else if (0 == _safe_ptr)
            {
                auto ctx = new safe_context;
                ctx->net = &net();
                ctx->s = this;
                ctx->is_client = is_client();
                ctx->remote_addr = remote_address();

                INSTALL_VIEW_POINT("send_message.add_lpc " + std::to_string(msg->header->trace_id));
                _looper->add_lpc("remote_send_message_without_safe_ptr", remote_send_message_without_safe_ptr, ctx, msg);
            }
            else
            {
                INSTALL_VIEW_POINT("send_message.safe_ptr.add_lpc " + std::to_string(msg->header->trace_id));
                msg->io_session_secret = get_secret();
                dbg_dassert(msg->io_session_secret, "io_session_secret not set");
                _looper->add_lpc("remote_send_message", remote_send_message, _looper, msg);
            }
        }

        static void local_rpc_timer_cb(event_loop*, void* data, void* data2)
        {
# ifndef NDEBUG
            ((hpc_rpc_session*)data)->check_thread_access();
# endif
            ((hpc_rpc_session*)data)->on_recv_response((uint32_t)(uintptr_t)data2, nullptr, 0);
            ((hpc_rpc_session*)data)->release_ref(); // added in timer
        }

        void hpc_rpc_session::local_send(message_ex* msg)
        {
            INSTALL_VIEW_POINT("local_send " + std::to_string(msg->header->trace_id));
# ifndef NDEBUG
            check_thread_access();
# endif
            dassert(_parser, "parser should not be null when send");
            _parser->prepare_on_send(msg);

            if (msg->header->context.u.is_request &&
                msg->u.client.call)
            {
                dbg_dassert(msg->header->client.timeout_ms > 0, "timeout not present");
                _matcher->on_call(msg, msg->u.client.call);
                msg->u.client.call = nullptr;

                this->add_ref(); // released in timer callback
                _looper->add_timer(
                    "local_rpc_timer_cb",
                    local_rpc_timer_cb,
                    this,
                    (void*)(uintptr_t)msg->header->id,
                    msg->header->client.timeout_ms
                );
            }

            msg->dl.insert_before(&_messages);
            ++_message_count;

            if (SS_CONNECTED == _connect_state && !_sending)
            {
                prepare_send_buffers();
                do_write();
            }
        }

        bool hpc_rpc_session::prepare_send_buffers()
        {
            auto n = _messages.next();
            int bcount = 0;

            dbg_dassert(!_sending, "");
            dbg_dassert(0 == _sending_buffers.size(), "");
            dbg_dassert(0 == _sending_msgs.size(), "");
            _sending_buffers.reserve(64);
            _sending = true;

            while (n != &_messages)
            {
                auto lmsg = CONTAINING_RECORD(n, message_ex, dl);
                auto lcount = _parser->get_buffer_count_on_send(lmsg);
                if (bcount > 0 && bcount + lcount > 64)
                {
                    break;
                }

                INSTALL_VIEW_POINT("prepare_send_buffers " + std::to_string(lmsg->header->trace_id));
                _sending_buffers.resize(bcount + lcount);
                auto rcount = _parser->get_buffers_on_send(lmsg, &_sending_buffers[bcount]);
                dassert(lcount >= rcount, "");
                if (lcount != rcount)
                    _sending_buffers.resize(bcount + rcount);
                bcount += rcount;
                _sending_msgs.push_back(lmsg);

                n = n->next();

                // TODO: why?
                /*
                if (!rcount && !lmsg->header->body_length)
                {
                    lmsg->dl.remove();
                    delete lmsg;
                }
                else
                {
                    if (lmsg->header->body_length == 0)
                        _sending_msgs.push_back(lmsg);
                }
                */
            }

            // added in send_message
            return _sending_msgs.size() > 0;
        }

        void hpc_rpc_session::on_send_ready()
        {
            if (_sending)
            {
                do_write();
            }
            else if (!_messages.is_alone())
            {
                prepare_send_buffers();
                do_write();
            }
            else
            {
                return;
            }
        }

        void hpc_rpc_session::on_send_completed()
        {
            dassert(_sending, "");

            for (auto& msg : _sending_msgs)
            {
                INSTALL_VIEW_POINT("on_send_completed " + std::to_string(msg->header->trace_id));
                msg->dl.remove();
                
                dinfo("msg %d sent, trace_id = %016" PRIx64,
                    msg->header->id,
                    msg->header->trace_id
                    );
                delete msg;
            }

            _sending = false;
            _sending_buffers.clear();
            _sending_msgs.clear();

            _sending_buffer_start_index = 0;
            ++_message_sent;

            if (!_messages.is_alone())
            {
                prepare_send_buffers();
                do_write();
            }
        }

# ifdef _WIN32

        socket_t create_socket(sockaddr_in* addr, bool is_async)
        {
            socket_t s = INVALID_SOCKET;
            if ((s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, 
                        is_async ? WSA_FLAG_OVERLAPPED : 0)) == INVALID_SOCKET)
            {
                dwarn("WSASocket failed, err = %d", ::GetLastError());
                return INVALID_SOCKET;
            }

            int reuse = 1;
            if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(int)) == -1)
            {
                dwarn("setsockopt SO_REUSEADDR failed, err = %s", strerror(errno));
            }

            int nodelay = 1;
            if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(int)) != 0)
            {
                dwarn("setsockopt TCP_NODELAY failed, err = %d", ::GetLastError());
            }

            int isopt = 1;
            if (setsockopt(s, SOL_SOCKET, SO_DONTLINGER, (char*)&isopt, sizeof(int)) != 0)
            {
                dwarn("setsockopt SO_DONTLINGER failed, err = %d", ::GetLastError());
            }
            
            /*
            int buflen = 8 * 1024 * 1024;
            if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char*)&buflen, sizeof(buflen)) != 0)
            {
                dwarn("setsockopt SO_SNDBUF failed, err = %d", ::GetLastError());
            }

            buflen = 8*1024*1024;
            if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char*)&buflen, sizeof(buflen)) != 0)
            {
                dwarn("setsockopt SO_RCVBUF failed, err = %d", ::GetLastError());
            }
            */
            int keepalive = 1;
            if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char*)&keepalive, sizeof(keepalive)) != 0)
            {
                dwarn("setsockopt SO_KEEPALIVE failed, err = %d", ::GetLastError());
            }
            
            if (addr != 0)
            {
                if (bind(s, (struct sockaddr*)addr, sizeof(*addr)) != 0)
                {
                    derror("bind failed, err = %d", ::GetLastError());
                    closesocket(s);
                    return INVALID_SOCKET;
                }
            }

            return s;
        }

        static LPFN_ACCEPTEX s_lpfnAcceptEx = NULL;
        static LPFN_CONNECTEX s_lpfnConnectEx = NULL;
        static LPFN_GETACCEPTEXSOCKADDRS s_lpfnGetAcceptExSockaddrs = NULL;

        static void load_socket_functions()
        {
            if (s_lpfnGetAcceptExSockaddrs != NULL)
                return;

            socket_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (s == INVALID_SOCKET)
            {
                dassert(false, "create Socket Failed, err = %d", ::GetLastError());
            }

            GUID GuidAcceptEx = WSAID_ACCEPTEX;
            GUID GuidConnectEx = WSAID_CONNECTEX;
            GUID GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
            DWORD dwBytes;

            // Load the AcceptEx function into memory using WSAIoctl.
            // The WSAIoctl function is an extension of the ioctlsocket()
            // function that can use overlapped I/O. The function's 3rd
            // through 6th parameters are input and output buffers where
            // we pass the pointer to our AcceptEx function. This is used
            // so that we can call the AcceptEx function directly, rather
            // than refer to the Mswsock.lib library.
            int rt = WSAIoctl(s,
                SIO_GET_EXTENSION_FUNCTION_POINTER,
                &GuidAcceptEx,
                sizeof(GuidAcceptEx),
                &s_lpfnAcceptEx,
                sizeof(s_lpfnAcceptEx),
                &dwBytes,
                NULL,
                NULL);
            if (rt == SOCKET_ERROR)
            {
                dwarn("WSAIoctl for AcceptEx failed, err = %d", ::WSAGetLastError());
                closesocket(s);
                return;
            }


            rt = WSAIoctl(s,
                SIO_GET_EXTENSION_FUNCTION_POINTER,
                &GuidConnectEx,
                sizeof(GuidConnectEx),
                &s_lpfnConnectEx,
                sizeof(s_lpfnConnectEx),
                &dwBytes,
                NULL,
                NULL);
            if (rt == SOCKET_ERROR)
            {
                dwarn("WSAIoctl for ConnectEx failed, err = %d", ::WSAGetLastError());
                closesocket(s);
                return;
            }

            rt = WSAIoctl(s,
                SIO_GET_EXTENSION_FUNCTION_POINTER,
                &GuidGetAcceptExSockaddrs,
                sizeof(GuidGetAcceptExSockaddrs),
                &s_lpfnGetAcceptExSockaddrs,
                sizeof(s_lpfnGetAcceptExSockaddrs),
                &dwBytes,
                NULL,
                NULL);
            if (rt == SOCKET_ERROR)
            {
                dwarn("WSAIoctl for GetAcceptExSockaddrs failed, err = %d", ::WSAGetLastError());
                closesocket(s);
                return;
            }

            closesocket(s);
        }

        hpc_network_provider::hpc_network_provider(rpc_engine* srv, network* inner_provider)
            : network(srv, inner_provider)
        {
            load_socket_functions();
            _listen_fd = INVALID_SOCKET;
            _looper = nullptr;
            _max_buffer_block_count_per_send = 64;
        }
        
        error_code hpc_network_provider::start(net_channel channel, int port, bool client_only)
        {
            if (_listen_fd != INVALID_SOCKET)
                return ERR_SERVICE_ALREADY_RUNNING;
            
            _looper = get_io_looper(node(), false);
            _address.assign_ipv4(get_local_ipv4(), port);

            if (!client_only)
            {
                struct sockaddr_in addr;
                addr.sin_family = AF_INET;
                addr.sin_addr.s_addr = INADDR_ANY;
                addr.sin_port = htons(port);

                _listen_fd = create_socket(&addr);
                if (_listen_fd == INVALID_SOCKET)
                {
                    dassert(false, "");
                }

                int forcereuse = 1;
                if (setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, 
                    (char*)&forcereuse, sizeof(forcereuse)) != 0)
                {
                    dwarn("setsockopt SO_REUSEDADDR failed, err = %d", ::GetLastError());
                }

                _looper->bind_io_handle((int)_listen_fd, &_accept_event.callback, 0);

                if (listen(_listen_fd, SOMAXCONN) != 0)
                {
                    dwarn("listen failed, err = %d", ::GetLastError());
                    return ERR_NETWORK_START_FAILED;
                }
                
                do_accept();
            }
            
            return ERR_OK;
        }

        rpc_session* hpc_network_provider::create_client_session(
            net_header_format fmt,
            ::dsn::rpc_address server_addr,
            bool is_async
            )
        {
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = 0;

            auto sock = create_socket(&addr, is_async);
            dassert(sock != -1, "create client tcp socket failed!");
            hpc_rpc_session * rs;
            if (is_async)
                rs = new hpc_rpc_session(sock, fmt, *this, server_addr, true);
            else
                rs = new hpc_sync_client_session(sock, fmt, *this, server_addr);
            auto err = rs->bind_looper();
            dassert(ERR_OK == err, "(s = %d) (client) bind looper failed for %s", sock, server_addr.to_string());
            return rs;
        }

        void hpc_network_provider::do_accept()
        {
            socket_t s = create_socket(nullptr);
            dassert(s != INVALID_SOCKET, "cannot create socket for accept");

            _accept_sock = s;            
            _accept_event.callback = [this](int err, uint32_t size, LPOVERLAPPED lolp)
            {
                //dinfo("accept completed, err = %d, size = %u", err, size);
                dassert(&_accept_event.olp == lolp, "must be this exact overlap");
                if (err == ERROR_SUCCESS)
                {
                    setsockopt(_accept_sock,
                        SOL_SOCKET,
                        SO_UPDATE_ACCEPT_CONTEXT,
                        (char *)&_listen_fd,
                        sizeof(_listen_fd)
                        );

                    struct sockaddr_in addr;
                    memset((void*)&addr, 0, sizeof(addr));

                    addr.sin_family = AF_INET;
                    addr.sin_addr.s_addr = INADDR_ANY;
                    addr.sin_port = 0;

                    int addr_len = sizeof(addr);
                    if (getpeername(_accept_sock, (struct sockaddr*)&addr, &addr_len)
                        == SOCKET_ERROR)
                    {
                        dassert(false, "getpeername failed, err = %d", ::WSAGetLastError());
                    }

                    ::dsn::rpc_address client_addr(ntohl(addr.sin_addr.s_addr), ntohs(addr.sin_port));
                    auto rs = new hpc_rpc_session(_accept_sock, NET_HDR_INVALID, *this, client_addr, false);
                    auto err = rs->bind_looper();
                    if (ERR_OK != err && ERR_IO_PENDING != err)
                    {
                        derror("accept failed when bind to io looper for address %s", client_addr.to_string());
                        rs->on_failure();
                    }
                    else
                    {
                        rs->start_read_next(256);
                    }   
                }
                else
                {
                    closesocket(_accept_sock);
                }

                do_accept();
            };

            memset(&_accept_event.olp, 0, sizeof(_accept_event.olp));

            DWORD bytes;
            BOOL rt = s_lpfnAcceptEx(
                _listen_fd, s,
                _accept_buffer,
                0,
                (sizeof(struct sockaddr_in) + 16),
                (sizeof(struct sockaddr_in) + 16),
                &bytes,
                &_accept_event.olp
                );

            if (!rt && (WSAGetLastError() != ERROR_IO_PENDING))
            {
                dassert(false, "AcceptEx failed, err = %d", ::WSAGetLastError());
                closesocket(s);
            }
        }
                
        io_loop_callback s_ready_event = 
        void s_ready_event(int err, uint32_t length, LPOVERLAPPED lolp)
            {
                auto evt = CONTAINING_RECORD(lolp, hpc_network_provider::ready_event, olp);
                evt->callback(err, length, lolp);
            };
        
        //////////////////////////////////////////////////////////////////
        //
        // hpc rpc session 
        //
        hpc_rpc_session::hpc_rpc_session(
            socket_t sock,
            net_header_format hdr_format,
            network& net,
            ::dsn::rpc_address remote_addr,
            bool is_client
        )
            : rpc_session(net, nullptr, remote_addr, hdr_format, is_client, true),
            _reader(net.message_buffer_block_size())
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

            _read_event.callback = [this](int err, uint32_t length, LPOVERLAPPED lolp)
            {
# ifndef NDEBUG
                check_thread_access();
# endif
                //dinfo("WSARecv completed, err = %d, size = %u", err, length);
                dassert((LPOVERLAPPED)lolp == &_read_event.olp, "must be exact this overlapped");
                if (err != ERROR_SUCCESS)
                {
                    dwarn("WSARecv from %s failed, err = %d", remote_address().to_string(), err);
                    on_failure();
                }
                else
                {
                    _reader.mark_read(length);

                    int read_next = -1;

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
                                this->on_recv_response(msg->header->id, msg, 0);

                            msg = _parser->get_message_on_receive(&_reader, read_next);
                        }
                    }

                    if (read_next == -1)
                    {
                        derror("(s = %d) recv failed on %s, parse failed",
                            _socket, remote_address().to_string());
                        on_failure();
                    }
                    else
                    {
                        dassert(read_next != 0, "cannot read 0 bytes next");
                        start_read_next(read_next);
                    }
                }

                release_ref(); // added when initiate a read op
            };

            _write_event.callback = [this](int err, uint32_t length, LPOVERLAPPED lolp)
            {
# ifndef NDEBUG
                check_thread_access();
# endif
                dassert((LPOVERLAPPED)lolp == &_write_event.olp, "must be exact this overlapped");
                if (err != ERROR_SUCCESS)
                {
                    dwarn("WSASend from %s failed, err = %d", remote_address().to_string(), err);
                    on_failure();
                }
                else if (!is_disconnected())
                {
                    int len = (int)length;
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
                            buf.sz -= (uint32_t)len;
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
                    }
                    else
                        on_send_ready();
                }

                release_ref(); // added when initiate a write op
            };

            if (is_client)
            {
                _connect_event.callback = [this](int err, uint32_t io_size, LPOVERLAPPED lpolp)
                {
# ifndef NDEBUG
                    check_thread_access();
# endif
                    //dinfo("ConnectEx completed, err = %d, size = %u", err, io_size);
                    if (err != ERROR_SUCCESS)
                    {
                        dwarn("ConnectEx to %s failed, err = %d", remote_address().to_string(), err);
                        this->on_failure();
                    }
                    else
                    {
                        dinfo("client session %s connected",
                            remote_address().to_string()
                        );

                        _connect_state = SS_CONNECTED;

                        if (!_messages.is_alone())
                        {
                            prepare_send_buffers();
                            do_write();
                        }
                        start_read_next(256);
                    }

                    this->release_ref(); // added before ConnectEx
                };
            }
            else
            {
                int addr_len = (int)sizeof(_peer_addr);
                if (getpeername(_socket, (struct sockaddr*)&_peer_addr, &addr_len) == -1)
                {
                    derror("(server) getpeername failed, err = %s", strerror(errno));
                }
            }

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
                struct sockaddr_in taddr;
                taddr.sin_family = AF_INET;
                taddr.sin_addr.s_addr = INADDR_ANY;
                taddr.sin_port = 0;

                _socket = create_socket(&taddr);
                dassert(_socket != -1, "create client tcp socket failed!");
                auto err = bind_looper();
                dassert(ERR_OK == err, "(s = %d) (client) bind looper failed for %s during reconnect", 
                    _socket, remote_address().to_string());
            }

            memset(&_connect_event.olp, 0, sizeof(_connect_event.olp));

            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(remote_address().ip());
            addr.sin_port = htons(remote_address().port());

            this->add_ref(); // released in _connect_event.callback
            BOOL rt = s_lpfnConnectEx(
                _socket,
                (struct sockaddr*)&addr,
                (int)sizeof(addr),
                0,
                0,
                0,
                &_connect_event.olp
            );

            if (!rt && (WSAGetLastError() != ERROR_IO_PENDING))
            {
                dwarn("ConnectEx to %s failed, err = %d", remote_address().to_string(), ::WSAGetLastError());
                this->release_ref(); // added before connectEx
                on_failure();
            }
        }

        void hpc_rpc_session::do_write()
        {
            add_ref(); // released in write callback

            memset(&_write_event.olp, 0, sizeof(_write_event.olp));

            dassert(_sending, "");

            int buffer_count = (int)_sending_buffers.size() - _sending_buffer_start_index;
            static_assert (sizeof(send_buf) == sizeof(WSABUF), "make sure they are compatible");

            if (buffer_count > net().max_buffer_block_count_per_send())
            {
                buffer_count = net().max_buffer_block_count_per_send();
            }

            DWORD bytes = 0;
            int rt = WSASend(
                _socket,
                (LPWSABUF)&_sending_buffers[_sending_buffer_start_index],
                (DWORD)buffer_count,
                &bytes,
                0,
                &_write_event.olp,
                NULL
            );

            if (SOCKET_ERROR == rt && (WSAGetLastError() != ERROR_IO_PENDING))
            {
                dwarn("WSASend to %s failed, err = %d", remote_address().to_string(), ::WSAGetLastError());
                release_ref(); // added before write 
                on_failure();
            }
        }

        void hpc_rpc_session::close()
        {
            if (-1 != _socket)
            {
                CloseHandle((HANDLE)_socket);
                dinfo("(s = %d) close socket %p", (int)_socket, this);
                _socket = -1;
            }
        }

        error_code hpc_rpc_session::bind_looper()
        {
            init_thread_checker(_looper->native_tid());
            return _looper->bind_io_handle(
                _socket,
                &s_ready_event,
                0
            );
        }

        void hpc_rpc_session::do_read(int read_next)
        {
            add_ref();
            
            memset(&_read_event.olp, 0, sizeof(_read_event.olp));

            WSABUF buf[1];

            void* ptr = _reader.read_buffer_ptr(read_next);
            int remaining = _reader.read_buffer_capacity();
            buf[0].buf = (char*)ptr;
            buf[0].len = remaining;

            DWORD bytes = 0;
            DWORD flag = 0;
            int rt = WSARecv(
                _socket,
                buf,
                1,
                &bytes,
                &flag,
                &_read_event.olp,
                NULL
                );
            
            if (SOCKET_ERROR == rt && (WSAGetLastError() != ERROR_IO_PENDING))
            {
                dwarn("WSARecv to %s failed, err = %d", remote_address().to_string(), ::WSAGetLastError());
                release_ref();
                on_failure();
            }

            //dinfo("WSARecv called, err = %d", rt);
        }

# endif
    }
}

