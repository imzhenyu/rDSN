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
 *     base interface for a network provider
 *
 * Revision history:
 *     Mar., 2015, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# pragma once

# include <dsn/tool-api/task.h>
# include <dsn/utility/synchronize.h>
# include <dsn/tool-api/message_parser.h>
# include <dsn/tool-api/rpc_client_matcher.h>
# include <dsn/cpp/address.h>
# include <dsn/utility/exp_delay.h>
# include <dsn/utility/dlib.h>
# include <dsn/cpp/thread_checker.h>
# include <atomic>

namespace dsn {

    class rpc_engine;
    class service_node;
    class task_worker_pool;
    class task_queue;
    /*!
    @addtogroup tool-api-providers
    @{
    */

    /*!
      network bound to a specific net_channel and port (see start)
     !!! all threads must be started with task::set_tls_dsn_context(null, provider->node());
    */
    class network
    {
    public:
        //
        // network factory prototype
        //
        template <typename T> static network* create(rpc_engine* srv, network* inner_provider)
        {
            return new T(srv, inner_provider);
        }
        
        typedef network* (*factory)(rpc_engine*, network*);

    public:
        //
        // srv - the rpc engine, could contain many networks there
        // inner_provider - when not null, this network is simply a wrapper for tooling purpose (e.g., tracing)
        //                  all downcalls should be redirected to the inner provider in the end
        //
        DSN_API network(rpc_engine* srv, network* inner_provider);
        virtual ~network() {}

        //
        // when client_only is true, port is faked (equal to app id for tracing purpose)
        //
        virtual error_code start(net_channel channel, int port, bool client_only) = 0;

        //
        // the named address
        //
        virtual ::dsn::rpc_address address() = 0;

        //
        // create client session 
        //
        virtual rpc_session* create_client_session(
                net_header_format fmt, 
                ::dsn::rpc_address server_addr,
                bool is_async
                ) = 0;

        //
        // send reply message on server, note in many
        // cases we can simply use message_ex::io_session_secret
        // as the rpc_session* to send message.
        // however, in some cases, we have to ensure rpc_session
        // is still valid before sending a message.
        // This is not an issue though for request message as we
        // always explicitly use a rpc session for sending a 
        // rpc request message.
        //
        virtual void send_reply_message(message_ex* msg) = 0;

        //
        // called when network received a complete request message
        //
        DSN_API void on_recv_request(message_ex* msg, int delay_ms);

        // server session management
        void on_server_session_created(rpc_session* s);
        DSN_API bool on_server_session_disconnected(rpc_session* s);
        DSN_API bool is_server_session_exist(rpc_session* s, rpc_address remote_addr);

        // client session management
        void on_client_session_created(rpc_session* s);
        bool on_client_session_destroyed(rpc_session* s);
        DSN_API bool is_client_session_exist(rpc_session* s, rpc_address remote_addr);

        // for in-place new message parser
        DSN_API std::pair<message_parser::factory2, size_t> get_message_parser_info(net_header_format hdr_format);
        DSN_API service_node* node() const;
        rpc_engine* engine() const { return _engine; }
        net_channel channel_type() const { return _channel_type; }
        int max_buffer_block_count_per_send() const { return _max_buffer_block_count_per_send; }
        net_header_format unknown_msg_hdr_format() const { return _unknown_msg_header_format; }
        int message_buffer_block_size() const { return _message_buffer_block_size; }
        bool enable_ipc_if_possible() const { return _enable_ipc_if_possible; }
        DSN_API virtual void get_runtime_info(const safe_string& indent, const safe_vector<safe_string>& args, /*out*/ safe_sstream& ss);

    protected:
        DSN_API static uint32_t get_local_ipv4();

    protected:
        rpc_engine                    *_engine;
        net_channel                   _channel_type;
        net_header_format             _unknown_msg_header_format; // default is NET_HDR_INVALID
        int                           _message_buffer_block_size;
        int                           _max_buffer_block_count_per_send;
        int                           _send_queue_threshold;
        bool                          _enable_ipc_if_possible;

    protected:
        typedef std::unordered_set< rpc_session* > sessions;

        sessions                      _clients; // to_address => <header_fmt, rpc_session>
        utils::rw_lock_nr             _clients_lock;
        sessions                      _servers; // from_address => rpc_session
        utils::rw_lock_nr             _servers_lock;

    private:
        friend class rpc_engine;
        DSN_API void reset_parser_attr(int message_buffer_block_size);
        DSN_API error_code start_internal(net_channel channel, int port, bool client_only);
    };

    /*!
      session managements (both client and server types)
    */
    class rpc_client_matcher;
    class rpc_session : public single_thread_ref_counter
    {
    public:
        /*!
        @addtogroup tool-api-hooks
        @{
        */
        DSN_API static join_point<void, rpc_session*> on_rpc_session_connected;
        DSN_API static join_point<void, rpc_session*> on_rpc_session_disconnected;
        /*@}*/

    public:
        //
        // virtual functions to be implemented
        //
        virtual void connect() = 0;
        virtual void disconnect_and_release() = 0;

        // msg should be deleted by rpc session
        virtual void send_message(message_ex* msg) = 0;
        virtual message_ex* recv_message() { dassert (false, "only implemented by sync client"); return nullptr; }
        virtual bool is_disconnected() const = 0;
        virtual void delay_recv(int milliseconds) = 0;

        // get a secret with which we can find
        // this rpc_session, see network::send_reply_message
        virtual uint64_t get_secret() const = 0;

    public:
        DSN_API rpc_session(
            network& net,
            rpc_client_matcher* matcher, 
            ::dsn::rpc_address remote_addr,
            net_header_format hdr_format,
            bool is_client,
            bool is_async
            );
        DSN_API virtual ~rpc_session();  

        DSN_API void on_recv_request(message_ex* msg, int delay_ms);
        DSN_API void on_recv_response(uint32_t id, message_ex* msg, int delay_ms);

        DSN_API bool on_disconnected();

        bool is_client() const { return _is_client; }
        bool is_async() const { return _is_async; }
        ::dsn::rpc_address remote_address() const { return _remote_addr; }
        network& net() const { return _net; }
        message_parser* parser() const { return _parser; }
        // should be called in do_read() before using _parser when it is nullptr.
        // returns:
        //   -1 : prepare failed, maybe because of invalid message header type
        //    0 : prepare succeed, _parser is not nullptr now.
        //   >0 : need read more data, returns read_next.
        DSN_API int prepare_parser(message_reader& reader);
        void* context() const { return _session_context; }
        void attach_context(void* ptr) 
        { 
            dassert (nullptr == _session_context, "context cannot be attached again without detach");
            _session_context = ptr; 
        }
        void* detach_context() 
        {
            void* c = _session_context;
            _session_context = nullptr;
            return c;
        }

        
    private:
        // constant info
        network                              &_net;
        ::dsn::rpc_address                   _remote_addr;
        const bool                           _is_client;
        const bool                           _is_async;
        void*                                _session_context;
        
    protected:
        rpc_client_matcher                   *_matcher;
        message_parser*                      _parser;
    };


    /*@}*/
}
