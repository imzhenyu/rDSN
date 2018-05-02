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

# ifdef _WIN32
# include <Winsock2.h>
# endif
# include <dsn/tool-api/network.h>
# include <dsn/utility/factory_store.h>
# include <dsn/tool-api/thread_profiler.h>
# include "message_parser_manager.h"
# include "rpc_engine.h"

# ifdef __TITLE__
# undef __TITLE__
# endif
# define __TITLE__ "network"

namespace dsn 
{
    /*static*/ join_point<void, rpc_session*> rpc_session::on_rpc_session_connected("rpc.session.connected");
    /*static*/ join_point<void, rpc_session*> rpc_session::on_rpc_session_disconnected("rpc.session.disconnected");

    rpc_session::~rpc_session()
    {
        if (_parser)
        {
            delete _parser;
        }

        if (_is_client)
        {
            _net.on_client_session_destroyed(this);
        }
    }

    int rpc_session::prepare_parser(message_reader& reader)
    {
        auto hdr_format = message_parser::get_header_type((const char*)reader.data(), (int)reader.length());
        if (hdr_format == NET_HDR_INVALID)
        {
            hdr_format = _net.unknown_msg_hdr_format();

            if (hdr_format == NET_HDR_INVALID)
            {
                derror("invalid header type, remote_client = %s, header_type = '%s'",
                       _remote_addr.to_string(),
                       message_parser::get_debug_string((const char*)reader.data(), (int)reader.length()).c_str()
                    );
                return -1;
            }
        }
        _parser = message_parser::new_message_parser(hdr_format, _is_client);
        dinfo("message parser created, remote_client = %s, header_format = %s, is_client = %d",
            _remote_addr.to_string(), hdr_format.to_string(), _is_client ? 1 : 0
            );

        return 0;
    }
    
    rpc_session::rpc_session(
        network& net,
        rpc_client_matcher* matcher, 
        ::dsn::rpc_address remote_addr,
        net_header_format hdr_format,
        bool is_client,
        bool is_async
        )
        : _net(net),
        _remote_addr(remote_addr),
        _is_client(is_client),
        _is_async(is_async),
        _matcher(matcher),
        _parser(is_client ? message_parser::new_message_parser(hdr_format, is_client) : nullptr)

    {
        _session_context = nullptr;
        if (!is_client)
        {
            _net.on_server_session_created(this);
        }
        else
        {
            _net.on_client_session_created(this);
        }
    }

    bool rpc_session::on_disconnected()
    {
        bool ret;
        if (is_client())
        {
            ret = true;
        }
        else
        {
            ret = _net.on_server_session_disconnected(this);
        }
        
        return ret;
    }

    void rpc_session::on_recv_request(message_ex* msg, int delay_ms)
    {
        TPF_MARK("rpc_session::on_recv_request");

        dassert (msg->header->context.u.is_request, "must be request");
        msg->from_address = _remote_addr;
        msg->to_address = _net.address();
        msg->u.server.s = this;
        msg->u.server.net = &_net;
        msg->io_session_context = this->context();
        
        //dbg_dassert(!is_client() || (is_client() && msg->io_session_secret == 0xdeadbeef), "only rpc server session can recv rpc requests");
        dbg_dassert(!is_client(), "only rpc server session can recv rpc requests");
        msg->io_session_secret = this->get_secret();
        dbg_dassert(msg->io_session_secret, "io_session_secret not set");
        _net.on_recv_request(msg, delay_ms);
    }

    void rpc_session::on_recv_response(uint32_t id, message_ex* msg, int delay_ms)
    {
        TPF_MARK("rpc_session::on_recv_response");

        if (!_matcher)
        {
            derror ("no matcher to recv msg, just discard directly");
            if (msg)
                delete msg;
            return;
        }

        if (msg)
        {
            dassert (!msg->header->context.u.is_request, "must be request");
            msg->from_address = _remote_addr;
            msg->to_address = _net.address();
        }

        _matcher->on_recv_reply(&_net, id, msg, delay_ms);
    }
    
    ////////////////////////////////////////////////////////////////////////////////////////////////
    network::network(rpc_engine* srv, network* inner_provider)
        : _engine(srv), 
        _channel_type(NET_CHANNEL_INVALID),
        _unknown_msg_header_format(NET_HDR_INVALID)
    {   
        _message_buffer_block_size = 1024 * 64;
        _max_buffer_block_count_per_send = 64; 
        _send_queue_threshold = (int)dsn_config_get_value_uint64(
            "network", "send_queue_threshold",
            4 * 1024, "send queue size above which throttling is applied"
            );
        _enable_ipc_if_possible = dsn_config_get_value_bool(
            "network", "enable_ipc_if_possible",
            false, "whether to enable IPC instead of TCP/IP etc. when local box net is required"
            );

        _unknown_msg_header_format = net_header_format::from_string(
            dsn_config_get_value_string(
                "network", 
                "unknown_message_header_format", 
                NET_HDR_INVALID.to_string(),
                "format for unknown message headers, default is NET_HDR_INVALID"
                ), NET_HDR_INVALID);
    }

    void network::reset_parser_attr(int message_buffer_block_size)
    {
        _message_buffer_block_size = message_buffer_block_size;
    }

    error_code network::start_internal(net_channel channel, int port, bool client_only)
    {
        _channel_type = channel;
        return start(channel, port, client_only);
    }

    service_node* network::node() const
    {
        return _engine->node();
    }

    void network::on_recv_request(message_ex* msg, int delay_ms)
    {
        return _engine->on_recv_request(this, msg, delay_ms);
    }

    std::pair<message_parser::factory2, size_t>  network::get_message_parser_info(net_header_format hdr_format)
    {
        auto& pinfo = message_parser_manager::instance().get(hdr_format);
        dassert(pinfo.factory2, "message parser '%s' not registerd or invalid!", hdr_format.to_string());
        return std::make_pair(pinfo.factory2, pinfo.parser_size);
    }

    uint32_t network::get_local_ipv4()
    {
        static const char* explicit_host = dsn_config_get_value_string(
            "network", "explicit_host_address",
            "", "explicit host name or ip (v4) assigned to this node (e.g., service ip for pods in kubernets)"
            );

        static const char* inteface = dsn_config_get_value_string(
            "network", "primary_interface",
            "", "network interface name used to init primary ipv4 address, if empty, means using the first \"eth\" prefixed non-loopback ipv4 address");

        uint32_t ip = 0;

        if (strlen(explicit_host) > 0)
        {
            ip = dsn_ipv4_from_host(explicit_host);
        }

        if (0 == ip)
        {
            ip = dsn_ipv4_local(inteface);
        }
        
        if (0 == ip)
        {
            char name[128];
            if (gethostname(name, sizeof(name)) != 0)
            {
                dassert(false, "gethostname failed, err = %s", strerror(errno));
            }
            ip = dsn_ipv4_from_host(name);
        }

        return ip;
    }

    void network::get_runtime_info(const safe_string& indent,
        const safe_vector<safe_string>& args, /*out*/ safe_sstream& ss)
    {
        auto indent2 = indent + "\t";
        {
            utils::auto_read_lock l(_clients_lock);
            for (auto& s : _clients)
            {
                ss << indent2
                    << s->remote_address().to_string()
                    << "(" << (s->is_disconnected() ? "x" : "v") << ")"
                    << std::endl;
            }
        }
        {
            utils::auto_read_lock l(_servers_lock);
            for (auto& s : _servers)
            {
                ss << indent2
                    << s->remote_address().to_string()
                    << "(" << (s->is_disconnected() ? "x" : "v") << ")"
                    << std::endl;
            }
        }
        ss << indent2 << std::endl;
    }

    

    void network::on_server_session_created(rpc_session* s)
    {
        int scount = 0;
        {
            utils::auto_write_lock l(_servers_lock);
            _servers.insert(s);
            scount = (int)_servers.size();
        }

        rpc_session::on_rpc_session_connected.execute(s);
        s->add_ref(); //released in on_server_session_disconnected
        ddebug("server session created, remote_client = %s, connection# @ %s = %d",
               s->remote_address().to_string(), address().to_string(), scount);
    }

    bool network::on_server_session_disconnected(rpc_session* s)
    {
        int scount = 0;
        bool r = false;
        {
            utils::auto_write_lock l(_servers_lock);
            r = _servers.erase(s) > 0;
            scount = (int)_servers.size();
        }

        if (r)
        {
            rpc_session::on_rpc_session_disconnected.execute(s);
            ddebug("server session disconnected, remote_client = %s, connection# @ %s = %d",
                   s->remote_address().to_string(), address().to_string(), scount);
            s->release_ref(); // added in on_server_session_created
        }
        return r;
    }

    bool network::is_server_session_exist(rpc_session* s, rpc_address remote_addr)
    {
        utils::auto_read_lock l(_servers_lock);
        auto it = _servers.find(s);
        return (it != _servers.end() && (*it)->remote_address() == remote_addr);
    }

    void network::on_client_session_created(rpc_session* s)
    {
        bool r = false;
        int scount = 0;
        {
            utils::auto_write_lock l(_clients_lock);
            r = _clients.insert(s).second;
            scount = (int)_clients.size();
        }

        dassert(r, "");
        ddebug("client session created, remote_server = %s, connection# @ %s = %d",
                   s->remote_address().to_string(), address().to_string(), scount);
    }

    bool network::on_client_session_destroyed(rpc_session* s)
    {
        int scount = 0;
        bool r = false;
        {
            utils::auto_write_lock l(_clients_lock);
            r = _clients.erase(s) > 0;
            scount = (int)_clients.size();
        }
        
        dassert(r, "");
        ddebug("client session failed, remote_server = %s, connection# @ %s = %d",
                   s->remote_address().to_string(), address().to_string(), scount);

        return r;
    }

    bool network::is_client_session_exist(rpc_session* s, rpc_address remote_addr)
    {
        utils::auto_read_lock l(_clients_lock);
        auto it = _clients.find(s);
        return (it != _clients.end() && (*it)->remote_address() == remote_addr);
    }
}
