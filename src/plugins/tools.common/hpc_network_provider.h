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
 *     What is this file about?
 *
 * Revision history:
 *     xxxx-xx-xx, author, first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

#pragma once

# ifdef _WIN32
    # include <Winsock2.h>
    typedef SOCKET socket_t;
# else
    # include <sys/types.h>
    # include <sys/socket.h>
    # include <netdb.h>
    # include <arpa/inet.h>
    typedef int socket_t;
    # if defined(__FreeBSD__)
        # include <netinet/in.h>
    # endif
# endif


# include <dsn/tool_api.h>
# include "io_looper.h"
# include <dsn/cpp/thread_checker.h>

namespace dsn {
    namespace tools {
        
        class hpc_network_provider : public network
        {
        public:
            hpc_network_provider(rpc_engine* srv, network* inner_provider);

            virtual error_code start(net_channel channel, int port, bool client_only) override;
            virtual ::dsn::rpc_address address() override { return _address;  }
            virtual rpc_session* create_client_session(
                net_header_format fmt, 
                ::dsn::rpc_address server_addr,
                bool is_async
                ) override;
            virtual void send_reply_message(message_ex* msg) override;
            
        private:
            void do_accept(bool ipc);

        public:
# ifdef _WIN32
            class ready_event
            {
            public:
                OVERLAPPED       olp;
                io_loop_callback callback;
            };
            ready_event          _accept_event;
# else
            friend void __hpc_accept_handler(event_loop* lp, int fd, void* ctx, int events);
            friend void __hpc_accept_ipc_handler(event_loop* lp, int fd, void* ctx, int events);
# endif

        private: 
# ifdef _WIN32
            socket_t             _accept_sock;
            char                 _accept_buffer[1024];
# endif
            socket_t             _listen_fd;
            socket_t             _listen_fd_ipc;
            ::dsn::rpc_address  _address;
            io_looper            *_looper;
            
        };

        enum session_state
        {
            SS_CONNECTING,
            SS_CONNECTED,
            SS_DISCONNECTED
        };

        class hpc_rpc_session : 
            public rpc_session, 
            public single_thread_context
        {
        public:
            hpc_rpc_session(
                socket_t sock,
                net_header_format hdr_format,
                network& net,
                ::dsn::rpc_address remote_addr,
                bool is_client,
                bool is_ipc_socket
                );
            ~hpc_rpc_session();

            virtual void connect() override;
            virtual void disconnect_and_release() override;
            virtual void send_message(message_ex* msg) override;
            virtual bool is_disconnected() const override { return SS_DISCONNECTED == _connect_state; }
            virtual void delay_recv(int milliseconds) override;
            virtual uint64_t get_secret() const override { return _safe_ptr; }

        public:
            void local_connect();
            void local_send(message_ex* msg);
            void start_read_next(int read_next);
            void save_to_safe_mgr();

        private:
            friend class hpc_network_provider;
            void on_connect_events_ready(int mask);
            void close();
            void on_failure();
            void on_send_ready();
            bool prepare_send_buffers();
            void on_send_completed();
            void do_write();
            void do_read(int read_next);
            error_code bind_looper();
            uint64_t get_safe_ptr() const { return _safe_ptr & (~(0x3FFULL << 54)); }

        private:
            message_reader              _reader;
            int                         _message_count; // count of _messages
            dlink                       _messages;        
            session_state               _connect_state;
            uint64_t                    _message_sent;
            int                         _socket_recv_buffer_bytes;

            int                         _delay_server_receive_ms;

            socket_t                    _socket;
            bool                        _sending;
            std::vector<message_ex*>    _sending_msgs;
            std::vector<send_buf>       _sending_buffers;
            int                         _sending_buffer_start_index;

# ifdef _WIN32
            hpc_network_provider::ready_event _read_event;
            hpc_network_provider::ready_event _write_event;
            hpc_network_provider::ready_event _connect_event;
# else
            friend void __hpc_read_write_handler(event_loop* lp, int fd, void* ctx, int events);
            friend void __hpc_connected_handler(event_loop* lp, int fd, void* ctx, int events);
# endif

            struct sockaddr_in          _peer_addr;
            io_looper*                  _looper;
            uint64_t                    _safe_ptr;
            bool                        _is_ipc_socket;

            friend void remote_disconnect(void* data, void*);
# ifndef _WIN32
            bool check_health();
            friend void __local_health_timer_cb(event_loop*, void* data, void* data2);
# endif
        };

        class hpc_sync_client_session : 
            public rpc_session
        {
        public:
            hpc_sync_client_session(
                socket_t sock,
                net_header_format hdr_format,
                network& net,
                ::dsn::rpc_address remote_addr,
                bool is_ipc_socket
                );
            ~hpc_sync_client_session();

            virtual void connect() override;
            virtual void disconnect_and_release() override;
            virtual void send_message(message_ex* msg) override;
            virtual message_ex* recv_message() override;

            virtual bool is_disconnected() const override { return SS_DISCONNECTED == _connect_state; }
            virtual void delay_recv(int milliseconds) override { dassert (false, "exec flow should not reach here");}
            virtual uint64_t get_secret() const override { dassert (false, "exec flow should not reach here"); return 0; }
        
        private:
            void close();
            void on_failure();

        private:
            session_state               _connect_state;
            struct sockaddr_in          _peer_addr;
            socket_t                    _socket;
            message_reader              _reader;
            bool                        _is_ipc_socket;
        };

        socket_t create_socket(int port, bool is_async, bool is_ipc, bool bind_addr);
        int      connect_socket(socket_t s, uint32_t ip, uint16_t port, bool is_ipc);

        // --------- inline implementation --------------
        inline void hpc_rpc_session::delay_recv(int delay_ms)
        {
            int old_delay_ms = _delay_server_receive_ms;
            if (delay_ms > old_delay_ms)
            {
                _delay_server_receive_ms = delay_ms;
            }
        }
    }
}
