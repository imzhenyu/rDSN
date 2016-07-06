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


# include <boost/asio.hpp>
# include <dsn/service_api_c.h>
# include <dsn/internal/singleton_store.h>
# include <dsn/tool/node_scoper.h>
# include "network.sim.h" 

# ifdef __TITLE__
# undef __TITLE__
# endif
# define __TITLE__ "net.provider.sim"

namespace dsn { namespace tools {

    // switch[channel][header_format]
    // multiple machines connect to the same switch
    // 10 should be >= than rpc_channel::max_value() + 1
    // 10 should be >= than network_header_format::max_value() + 1
    static utils::safe_singleton_store< ::dsn::rpc_address, sim_network_provider*> s_switch[10][10];

    sim_client_session::sim_client_session(
        sim_network_provider& net, 
        ::dsn::rpc_address remote_addr, 
        message_parser_ptr& parser
        )
        : rpc_session(net, remote_addr, parser, true)
    {}

    void sim_client_session::connect() 
    {
        if (try_connecting())
           set_connected();
    }

    static message_ex* virtual_send_message(message_ex* msg)
    {
        std::shared_ptr<char> buffer(new char[msg->header->body_length + sizeof(message_header)], std::default_delete<char[]>());
        char* tmp = buffer.get();

        for (auto& buf : msg->buffers)
        {
            memcpy((void*)tmp, (const void*)buf.data(), (size_t)buf.length());
            tmp += buf.length();
        }

        blob bb(buffer, 0, msg->header->body_length + sizeof(message_header));
        message_ex* recv_msg = message_ex::create_receive_message(bb);
        recv_msg->to_address = msg->to_address;

        msg->copy_to(*recv_msg); // extensible object state move

        return recv_msg;
    }

    void sim_client_session::send(uint64_t sig)
    {
        for (auto& msg : _sending_msgs)
        {
            network_header_format hdr_format(NET_HDR_DSN);
            bool r = header_type::header_type_to_format(msg->header->hdr_type, hdr_format);
            dassert(r, "header_type_to_format(%s) failed", msg->header->hdr_type.debug_string().c_str());

            sim_network_provider* rnet = nullptr;
            if (!s_switch[task_spec::get(msg->local_rpc_code)->rpc_call_channel][hdr_format].get(remote_address(), rnet))
            {
                derror("cannot find destination node %s in simulator",
                    remote_address().to_string()
                    );
                //on_disconnected();  // disable this to avoid endless resending
            }
            else
            {
                auto server_session = rnet->get_server_session(_net.address());
                if (nullptr == server_session)
                {
                    rpc_session_ptr cptr = this;
                    message_parser_ptr parser = _net.new_message_parser(hdr_format);
                    server_session = new sim_server_session(*rnet, _net.address(), 
                        cptr, parser);
                    rnet->on_server_session_accepted(server_session);
                }

                message_ex* recv_msg = virtual_send_message(msg);

                {
                    node_scoper ns(rnet->node());

                    bool ret = server_session->on_recv_message(recv_msg,
                        recv_msg->to_address == recv_msg->header->from_address ?
                        0 : rnet->net_delay_milliseconds()
                        );
                    dassert(ret, "");
                }
            }
        }

        on_send_completed(sig);
    }

    sim_server_session::sim_server_session(
        sim_network_provider& net, 
        ::dsn::rpc_address remote_addr,
        rpc_session_ptr& client,
        message_parser_ptr& parser
        )
        : rpc_session(net, remote_addr, parser, false)
    {
        _client = client;
    }

    void sim_server_session::send(uint64_t sig)
    {
        for (auto& msg : _sending_msgs)
        {
            message_ex* recv_msg = virtual_send_message(msg);

            {
                node_scoper ns(_client->net().node());

                bool ret = _client->on_recv_message(recv_msg,
                    recv_msg->to_address == recv_msg->header->from_address ?
                    0 : (static_cast<sim_network_provider*>(&_net))->net_delay_milliseconds()
                    );
                dassert(ret, "");
            }
        }        

        on_send_completed(sig);
    }

    sim_network_provider::sim_network_provider(rpc_engine* rpc, network* inner_provider)
        : connection_oriented_network(rpc, inner_provider)
    {
        _address.assign_ipv4("localhost", 1);

        _min_message_delay_microseconds = 1;
        _max_message_delay_microseconds = 100000;

        auto config = tool_app::get_service_spec().config;
        if (config != NULL)
        {
            _min_message_delay_microseconds = config->get_value<uint32_t>("tools.simulator", 
                "min_message_delay_microseconds", _min_message_delay_microseconds,
                "min message delay (us)");
            _max_message_delay_microseconds = config->get_value<uint32_t>("tools.simulator", 
                "max_message_delay_microseconds", _max_message_delay_microseconds,
                "max message delay (us)");
        }
    }

    error_code sim_network_provider::start(rpc_channel channel, int port, bool client_only, io_modifer& ctx)
    { 
        dassert(channel == RPC_CHANNEL_TCP || channel == RPC_CHANNEL_UDP, "invalid given channel %s", channel.to_string());

        _address = ::dsn::rpc_address("localhost", port);
        auto hostname = boost::asio::ip::host_name();
        if (!client_only)
        {
            for (int i = 0; i <= network_header_format::max_value(); i++)
            {
                auto hdr_format = network_header_format(network_header_format::to_string(i));
                if (s_switch[channel][hdr_format].put(_address, this))
                {
                    auto ep2 = ::dsn::rpc_address(hostname.c_str(), port);
                    s_switch[channel][hdr_format].put(ep2, this);
                }
                else
                {
                    return ERR_ADDRESS_ALREADY_USED;
                }
            }
            return ERR_OK;
        }
        else
        {
            return ERR_OK;
        }
    }

    uint32_t sim_network_provider::net_delay_milliseconds() const
    {
        return static_cast<uint32_t>(dsn_random32(_min_message_delay_microseconds, _max_message_delay_microseconds)) / 1000;
    }    
}} // end namespace
