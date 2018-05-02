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

# pragma once

# include <dsn/tool_api.h>
# include <dsn/tool-api/rpc_client_matcher.h>

namespace dsn { namespace tools {

    class sim_network_provider;
    class sim_client_session : public rpc_session
    {
    public:
        sim_client_session(
            sim_network_provider& net, 
            rpc_client_matcher* matcher, 
            ::dsn::rpc_address remote_addr, 
            net_header_format hdr_format
            );

        virtual void send_message(message_ex* msg) override;
        virtual message_ex* recv_message() override;
        void set_response_msg(message_ex* resp);
        
        virtual void connect() override {}
        virtual void disconnect_and_release() override;
        virtual bool is_disconnected() const override { return false; }
        virtual void delay_recv(int milliseconds) override {}
        virtual uint64_t get_secret() const override { return (uint64_t)(uintptr_t)this; }
        
    private:
        rpc_session* _server_s;
        message_ex*  _sync_resp_msg; // use when is_async() == true 
    };

    class sim_server_session : public rpc_session
    {
    public:
        sim_server_session(
            sim_network_provider& net, 
            rpc_client_matcher* matcher, 
            ::dsn::rpc_address remote_addr, 
            rpc_session* client, 
            net_header_format hdr_format
            );

        virtual void send_message(message_ex* msg) override;

        virtual void connect() override {}
        virtual void disconnect_and_release() override;
        virtual bool is_disconnected() const override { return false; }
        virtual void delay_recv(int milliseconds) override {}
        virtual uint64_t get_secret() const override { return 0; }

    private:
        sim_client_session* _client;
    };

    class sim_network_provider : public network
    {
    public:
        sim_network_provider(rpc_engine* rpc, network* inner_provider);
        ~sim_network_provider(void) {}

        virtual error_code start(net_channel channel, int port, bool client_only) override;
    
        virtual ::dsn::rpc_address address() override { return _address; }

        virtual rpc_session* create_client_session(
            net_header_format fmt, 
            ::dsn::rpc_address server_addr,
            bool is_async) override
        {
            return new sim_client_session(*this, &_client_matcher, server_addr, fmt);
        }

        uint32_t net_delay_milliseconds() const;

        rpc_session* get_server_session(::dsn::rpc_address ep);

        virtual void send_reply_message(message_ex* msg) override ;

    private:
        ::dsn::rpc_address    _address;
        uint32_t               _min_message_delay_microseconds;
        uint32_t               _max_message_delay_microseconds;
        rpc_client_matcher     _client_matcher;
    };
        
    //------------- inline implementations -------------


}} // end namespace

