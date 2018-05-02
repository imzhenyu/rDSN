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
 *     rpc service
 *
 * Revision history:
 *     Mar., 2015, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# pragma once

# include <dsn/tool-api/task.h>
# include <dsn/tool-api/network.h>
# include <dsn/tool-api/rpc_client_matcher.h>
# include <dsn/tool-api/global_config.h>
# include <dsn/utility/configuration.h>
# include <unordered_map>

namespace dsn {


#define MAX_CLIENT_PORT 1023

class service_node;
class rpc_engine;
class uri_resolver_manager;

class rpc_server_dispatcher
{
public:
    rpc_server_dispatcher();
    ~rpc_server_dispatcher();

    bool              register_rpc_handler(rpc_handler_info* handler);
    rpc_handler_info* unregister_rpc_handler(dsn_task_code_t rpc_code, const char* service_name);
    rpc_request_task* on_request(message_ex* msg, service_node* node);
    void              on_request_with_inline_execution(message_ex* msg, service_node* node);
    
private:
    bool get_rpc_handler(message_ex* msg, dsn_rpc_request_handler_t& handler, void*& param);

private:
    typedef std::unordered_map<safe_string, rpc_handler_info*> service_handlers;
    struct per_code_handlers 
    {
        service_handlers services; // <service_name, handlers>
        utils::rw_lock_nr l; // TODO: optimize it into readonly map
    };
    std::vector<per_code_handlers*  > _vhandlers;
};

class rpc_engine
{
public:
    rpc_engine(configuration_ptr config, service_node* node);

    //
    // management routines
    //
    ::dsn::error_code start(
        const service_app_spec& spec
        );
    void start_serving() { _is_serving = true; }

    //
    // rpc registrations
    //
    bool  register_rpc_handler(rpc_handler_info* handler);
    rpc_handler_info* unregister_rpc_handler(dsn_task_code_t rpc_code, const char* service_name);

    //
    // rpc routines
    //
    void on_recv_request(network* net, message_ex* msg, int delay_ms);
    void reply(message_ex* response, dsn_rpc_error_t err = RPC_ERR_OK);
   
    //
    // information inquery
    //
    service_node* node() const { return _node; }
    ::dsn::rpc_address primary_address() const { return _local_primary_address; }
    void get_runtime_info(const safe_string& indent, const safe_vector<safe_string>& args, /*out*/ safe_sstream& ss);
    network* get_client_network(net_channel channel);
    
private:
    network* create_network(
        const network_server_config& netcs, 
        bool client_only
        );

private:
    configuration_ptr                                _config;    
    service_node                                     *_node;
    std::vector<network*>                            _client_nets; // <CHANNEL, network*>
    std::unordered_map<int, std::vector<network*>>   _server_nets; // <port, <CHANNEL, network*>>
    ::dsn::rpc_address                               _local_primary_address;
    rpc_server_dispatcher                            _rpc_dispatcher;   

    volatile bool                                    _is_running;
    volatile bool                                    _is_serving;
};

// ------------------------ inline implementations --------------------
inline network* rpc_engine::get_client_network(net_channel channel)
{
    return _client_nets[channel];
}

} // end namespace

