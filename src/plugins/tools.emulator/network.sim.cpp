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

# ifndef _WIN32
# include <unistd.h>
# else
# include <WinSock2.h>
# endif
# include <dsn/service_api_c.h>
# include <dsn/utility/singleton_store.h>
# include <dsn/tool-api/thread_profiler.h>
# include <dsn/tool-api/node_scoper.h>
# include "network.sim.h" 

# ifdef __TITLE__
# undef __TITLE__
# endif
# define __TITLE__ "net.provider.sim"

namespace dsn { namespace tools {

    // switch[channel][header_format]
    // multiple machines connect to the same switch
    // 10 should be >= than net_channel::max_value() + 1
    // 10 should be >= than net_header_format::max_value() + 1
    static utils::safe_singleton_store< ::dsn::rpc_address, sim_network_provider*> s_switch[10][10];

    sim_client_session::sim_client_session(
        sim_network_provider& net,
        rpc_client_matcher* matcher,
        ::dsn::rpc_address remote_addr, 
        net_header_format hdr_format
        )
        : rpc_session(net, matcher, remote_addr, hdr_format, true, true)
    {
        sim_network_provider* rnet = nullptr;
        _sync_resp_msg = nullptr;
        auto raddr = remote_address();
        if (s_switch[net.channel_type()][parser()->format()].get(raddr, rnet))
        {
            _server_s = rnet->get_server_session(net.address());
            if (nullptr == _server_s)
            {
                _server_s = new sim_server_session(*rnet, nullptr,
                    net.address(),
                    this, parser()->format());
            }
        }
    }

    static message_ex* virtual_send_message(
        message_ex* msg, 
        message_parser* parser)
    {
        TPF_MARK("virtual_send_message.begin"); 

        parser->prepare_on_send(msg);

        TPF_MARK("virtual_send_message.1");

        std::vector<send_buf> buffers;
        msg->get_buffers(parser, buffers);

        TPF_MARK("virtual_send_message.2");

        int bytes = 0;
        for (size_t i = 0; i < buffers.size(); i++)
        {
            bytes += buffers[i].sz;
        }

        ::dsn::message_reader reader(bytes);
        char* target = reader.read_buffer_ptr(bytes);
        for (size_t i = 0; i < buffers.size(); i++)
        {
            memcpy(target, buffers[i].buf, buffers[i].sz);
            target += buffers[i].sz;
        }
        reader.mark_read(bytes);

        TPF_MARK("virtual_send_message.3");

        int read_next = 10;
        auto rmsg = parser->get_message_on_receive(&reader, read_next);

        TPF_MARK("virtual_send_message.end");
        return rmsg;
    }

    void sim_client_session::disconnect_and_release()
    {
        if (_server_s)
        {
            auto s = _server_s;
            _server_s = nullptr;
            s->on_disconnected();
        }

        on_disconnected();
        release_ref();
    }

    void sim_client_session::send_message(message_ex* msg)
    {
        if (this->is_async())
        {
            if (msg->header->context.u.is_request &&
                msg->u.client.call)
            {
                _matcher->on_call(msg, msg->u.client.call);
                msg->u.client.call = nullptr;
            }
        }
        
        message_ex* recv_msg;
        if (_server_s)
        {
            node_scoper ns(_server_s->net().node());
            recv_msg = virtual_send_message(msg, _server_s->parser());
            recv_msg->from_address = net().address();
            recv_msg->to_address = _server_s->net().address();

            _server_s->on_recv_request(recv_msg,
                msg->to_address == msg->from_address ?
                0 : ((sim_network_provider&)_server_s->net()).net_delay_milliseconds()
            );
        }
        else
        {
            dwarn("client session %s is disconnected, msg %s with trace_id = %16" PRIx64 " will be dropped",
                remote_address().to_string(),
                msg->dheader.rpc_name,
                msg->header->trace_id
                );
        }

        delete msg;
    }

    message_ex* sim_client_session::recv_message()
    {
        auto r = _sync_resp_msg;
        _sync_resp_msg = nullptr;
        return r;
    }

    void sim_client_session::set_response_msg(message_ex* resp)
    {
        dassert (_sync_resp_msg == nullptr, "response message must be null for sync client");
        _sync_resp_msg = resp;
    }

    sim_server_session::sim_server_session(
        sim_network_provider& net,
        rpc_client_matcher* matcher, 
        ::dsn::rpc_address remote_addr,
        rpc_session* client,
        net_header_format hdr_format
        )
        : rpc_session(net, matcher, remote_addr, hdr_format, false, true)
    {
        _client = (sim_client_session*)client;
        if (!parser())
        {
            _parser = message_parser::new_message_parser(hdr_format, false);
        }
    }

    void sim_server_session::disconnect_and_release()
    {
        if (_client)
        {
            auto s = _client;
            _client = nullptr;
            s->on_disconnected();
        }

        on_disconnected();
        release_ref();
    }

    void sim_server_session::send_message(message_ex* msg)
    {
        if (_client)
        {
            message_ex* recv_msg;
            {
                node_scoper ns(_client->net().node());
                recv_msg = virtual_send_message(msg, _client->parser());
            }

            recv_msg->header->context.u.is_request = false;
            recv_msg->from_address = net().address();
            recv_msg->to_address = _client->net().address();

            {
                node_scoper ns(_client->net().node());

                if (_client->is_async())
                    _client->on_recv_response(
                        recv_msg->header->id,
                        recv_msg,
                        msg->to_address == msg->from_address ?
                        0 : (static_cast<sim_network_provider*>(&net()))->net_delay_milliseconds()
                    );
                else
                    _client->set_response_msg(recv_msg);
            }
        }
        else
        {
            dwarn("server session %s is disconnected, msg %s with trace_id = %16" PRIx64 " will be dropped",
                remote_address().to_string(),
                msg->dheader.rpc_name,
                msg->header->trace_id
            );
        }

        delete msg;
    }

    sim_network_provider::sim_network_provider(rpc_engine* rpc, network* inner_provider)
        : network(rpc, inner_provider)
    {
        _address.assign_ipv4("localhost", 1);

        _min_message_delay_microseconds = 0;
        _max_message_delay_microseconds = 0;

        _min_message_delay_microseconds = (uint32_t)dsn_config_get_value_uint64("tools.emulator",
            "min_message_delay_microseconds", _min_message_delay_microseconds,
            "min message delay (us)");
        _max_message_delay_microseconds = (uint32_t)dsn_config_get_value_uint64("tools.emulator",
            "max_message_delay_microseconds", _max_message_delay_microseconds,
            "max message delay (us)");
    }

    error_code sim_network_provider::start(net_channel channel, int port, bool client_only)
    {
        _address = ::dsn::rpc_address("localhost", port);
        char hostname[256];
        auto err = gethostname(hostname, sizeof(hostname));
# ifndef _WIN32
        dassert(0 == err, "gethostname failed, err = %d", errno);
# else
        dassert(0 == err, "gethostname failed, err = %d", ::GetLastError());
# endif

        if (!client_only)
        {
            for (int i = NET_HDR_INVALID + 1; i <= net_header_format::max_value(); i++)
            {
                auto addr = _address;
                auto this_ = this;
                if (s_switch[channel][i].put(std::move(addr), std::forward<sim_network_provider*>(this_)))
                {
                    auto ep2 = ::dsn::rpc_address(hostname, port);
                    s_switch[channel][i].put(std::move(ep2), std::forward<sim_network_provider*>(this_));
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

    rpc_session* sim_network_provider::get_server_session(::dsn::rpc_address ep)
    {
        utils::auto_read_lock l(_servers_lock);
        for (auto& s : _servers)
        {
            if (s->remote_address() == ep)
                return s;
        }
        return nullptr;
    }

    void sim_network_provider::send_reply_message(message_ex* msg)
    {
        auto s = get_server_session(msg->to_address);
        if (s)
            s->send_message(msg);
    }

}} // end namespace
