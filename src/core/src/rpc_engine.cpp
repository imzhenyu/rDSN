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
 *     rpc engine implementation
 *
 * Revision history:
 *     Mar., 2015, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# ifdef _WIN32
# include <WinSock2.h>
# else
# include <sys/socket.h>
# include <netdb.h>
# include <ifaddrs.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# endif

# include "rpc_engine.h"
# include "service_engine.h"
# include "group_address.h"
# include <dsn/tool-api/perf_counter.h>
# include <dsn/utility/factory_store.h>
# include <dsn/tool-api/task_queue.h>
# include <set>
# include <dsn/cpp/framework.h>
# include <dsn/tool-api/thread_profiler.h>
# include <dsn/tool-api/view_point.h>

# ifdef __TITLE__
# undef __TITLE__
# endif
# define __TITLE__ "rpc.engine"

namespace dsn 
{    
    rpc_client_matcher::~rpc_client_matcher()
    {
        for (int i = 0; i < MATCHER_BUCKET_NR; i++)
        {
            dassert(_requests[i].size() == 0, "all rpc entries must be removed before the matcher ends");
        }
    }

    bool rpc_client_matcher::on_recv_reply(network* net, uint32_t key, message_ex* reply, int delay_ms)
    {
        rpc_response_task* call = nullptr;
        int bucket_index = key % MATCHER_BUCKET_NR;

        {
            //utils::auto_lock< ::dsn::utils::ex_lock_nr_spin> l(_requests_lock[bucket_index]);
            auto it = _requests[bucket_index].find(key);
            if (it != _requests[bucket_index].end())
            {
                call = it->second;
                _requests[bucket_index].erase(it);
                --_flying_call_count;
            }
        }

        if (nullptr == call)
        {
            if (reply)
            {
                delete reply;
            }
            return false;
        }

        call->set_delay(delay_ms);

        if (nullptr == reply)
        {    
            call->enqueue(RPC_ERR_TIMEOUT, reply);
        }
        else
        {
            INSTALL_VIEW_POINT("on_recv_reply " + std::to_string(reply->header->trace_id));

            reply->local_rpc_code = call->spec().code;
            reply->dheader.rpc_name = call->spec().name.c_str();

            // failure injection applied
            if (!call->enqueue((dsn_rpc_error_t)reply->header->context.u.server_error, reply))
            {
                ddebug("rpc reply %s is dropped (fault inject), trace_id = %016" PRIx64,
                    reply->dheader.rpc_name,
                    reply->header->trace_id
                    );
                delete reply;
                return false;
            }
        }

        return true;
    }
    
    void rpc_client_matcher::on_call(message_ex* request, rpc_response_task* call)
    {
        message_header& hdr = *request->header;
        int bucket_index = hdr.id % MATCHER_BUCKET_NR;

        dbg_dassert(call != nullptr, "rpc response task cannot be empty");
        {
            //utils::auto_lock< ::dsn::utils::ex_lock_nr_spin> l(_requests_lock[bucket_index]);
            auto pr = _requests[bucket_index].emplace(hdr.id, call);
            dassert (pr.second, "the message is already on the fly!!!");
        }

        ++_flying_call_count;
    }

    //----------------------------------------------------------------------------------------------
    rpc_server_dispatcher::rpc_server_dispatcher()
    {
        _vhandlers.resize(dsn_task_code_max() + 1);
        for (auto& h : _vhandlers)
        {
            h = new per_code_handlers();
        }
    }

    rpc_server_dispatcher::~rpc_server_dispatcher()
    {
        for (auto& h : _vhandlers)
        {
            delete h;
        }
        _vhandlers.clear();
    }

    bool rpc_server_dispatcher::register_rpc_handler(rpc_handler_info* handler)
    {
        auto v = _vhandlers[handler->code];
        bool r;
        {
            utils::auto_write_lock l(v->l);
            auto it = v->services.find(handler->service_name);
            if (it != v->services.end())
            {
                r = false;
            }
            else
            {
                v->services.emplace(handler->service_name, handler);
                r = true;
            }
        }

        if (r)
        {
            dinfo("rpc registration for '%s.%s' succeeded",
                handler->service_name.c_str(),
                handler->method_name.c_str()
            );
            return true;
        }
        else
        {
            dassert(false, "rpc registration confliction for '%s.%s'",
                handler->service_name.c_str(),
                handler->method_name.c_str()
            );
            return false;
        }
    }

    rpc_handler_info* rpc_server_dispatcher::unregister_rpc_handler(dsn_task_code_t rpc_code, const char* service_name)
    {
        rpc_handler_info* ret = nullptr;
        auto v = _vhandlers[rpc_code];
        {
            utils::auto_write_lock l(v->l);
            auto it = v->services.find(service_name);
            if (it != v->services.end())
            {
                ret = it->second;
                v->services.erase(it);
            }
        }

        if (ret)
        {
            dinfo("rpc unregistration for '%s.%s' succeeded",
                ret->service_name.c_str(),
                ret->method_name.c_str()
            );
        }
        else
        {
            dwarn("rpc unregistration for '%s.%s' failed",
                ret->service_name.c_str(),
                ret->method_name.c_str()
            );
        }
        return ret;
    }

    bool rpc_server_dispatcher::get_rpc_handler(message_ex* msg, dsn_rpc_request_handler_t& handler, void*& param)
    {
        auto v = _vhandlers[msg->local_rpc_code];
        {
            utils::auto_read_lock l(v->l);
            auto it = v->services.find(msg->dheader.service_name);
            if (it != v->services.end())
            {
                handler = it->second->c_handler;
                param = it->second->parameter;
                return true;
            }
        }
        return false;
    }

    rpc_request_task* rpc_server_dispatcher::on_request(message_ex* msg, service_node* node)
    {
        dsn_rpc_request_handler_t handler;
        void * param;

        if (get_rpc_handler(msg, handler, param))
        {
            auto r = new rpc_request_task(msg, handler, param, node);
            r->spec().on_task_create.execute(task::get_current_task(), r);
            return r;
        }
        else
            return nullptr;
    }

    void rpc_server_dispatcher::on_request_with_inline_execution(message_ex* msg, service_node* node)
    {
        dsn_rpc_request_handler_t handler;
        void * param;

        if (get_rpc_handler(msg, handler, param))
        {
            if (handler(msg, param))
            {
                delete msg;
            }
            else
            {
                // nothing to do
            }
        }
        else
        {
            dwarn("msg %s not handled and dropped, trace_id = %" PRIx64,
                msg->dheader.rpc_name, msg->header->trace_id);
            delete msg;
        }
    }

    //----------------------------------------------------------------------------------------------
    rpc_engine::rpc_engine(configuration_ptr config, service_node* node)
        : _config(config), _node(node)
    {
        dassert (_node != nullptr, "");
        dassert (_config != nullptr, "");

        _is_running = false;
        _is_serving = false;
    }

    void rpc_engine::get_runtime_info(const safe_string& indent,
        const safe_vector<safe_string>& args, /*out*/ safe_sstream& ss)
    {
        auto indent2 = indent;
        auto indent3 = indent2 + "\t";
        ss << indent2 << "RPC.ConnectTo:" << std::endl;
        for (int ch = 0; ch < (int)_client_nets.size(); ch++)
        {
            auto n = _client_nets[ch];
            if (n != nullptr)
            {
                ss << indent3 
                    << "CHANNEL = "
                    << net_channel::to_string(ch)
                    << std::endl;

                n->get_runtime_info(indent3, args, ss);
            }
        }

        ss << indent2 << "RPC.Clients:" << std::endl;
        for (auto& kv : _server_nets)
        {
            auto port = kv.first;
            for (int ch = 0; ch < (int)kv.second.size(); ch++)
            {
                auto n = kv.second[ch];
                if (n != nullptr)
                {
                    ss << indent3 
                        << "CHANNEL = "
                        << net_channel::to_string(ch)
                        << ", PORT = " << port
                        << std::endl;

                    n->get_runtime_info(indent3, args, ss);
                }
            }
        }

        ss << indent2 << std::endl;
    }
        
    //
    // management routines
    //
    network* rpc_engine::create_network(
        const network_server_config& netcs, 
        bool client_only
        )
    {
        const service_spec& spec = service_engine::fast_instance().spec();
        network* net = utils::factory_store<network>::create(
            netcs.factory_name.c_str(), ::dsn::PROVIDER_TYPE_MAIN, this, nullptr);
        net->reset_parser_attr(netcs.message_buffer_block_size);

        for (auto it = spec.network_aspects.begin();
            it != spec.network_aspects.end();
            it++)
        {
            net = utils::factory_store<network>::create(it->c_str(), ::dsn::PROVIDER_TYPE_ASPECT, this, net);
        }

        // start the net
        error_code ret = net->start_internal(netcs.channel, netcs.port, client_only); 
        if (ret == ERR_OK)
        {
            return net;
        }
        else
        {
            // mem leak, don't care as it halts the program
            dassert(false, "create network failed, error_code: %s", ret.to_string());
            return nullptr;
        }   
    }

    error_code rpc_engine::start(
        const service_app_spec& aspec
        )
    {
        if (_is_running)
        {
            return ERR_SERVICE_ALREADY_RUNNING;
        }
    
        // local cache for shared networks with same provider and message format and port
        std::map<std::string, network*> named_nets; // factory##fmt##port -> net

        // start client networks for each channel
        _client_nets.resize(net_channel::max_value() + 1);
        for (int j = 1; j <= net_channel::max_value(); j++)
        {
            net_channel c = net_channel(net_channel::to_string(j));
            std::string factory;
            int blk_size;

            auto it1 = aspec.network_client_confs.find(c);
            if (it1 != aspec.network_client_confs.end())
            {
                factory = it1->second.factory_name.c_str();
                blk_size = it1->second.message_buffer_block_size;
            }
            else
            {
                dwarn("network client for channel %s not registered, assuming not used further", c.to_string());
                continue;
            }

            network_server_config cs(aspec.id, c);
            cs.factory_name = factory.c_str();
            cs.message_buffer_block_size = blk_size;

            auto net = create_network(cs, true);
            if (!net) return ERR_NETWORK_INIT_FAILED;
            _client_nets[j] = net;

            ddebug("[%s] network client started at %s, channel = %s ...",
                    node()->name(),
                    net->address().to_string(),
                    cs.channel.to_string()
                    );
        }
        // start server networks
        uint16_t last_server_port = 0;
        for (auto& sp : aspec.network_server_confs)
        {
            int port = sp.second.port;

            std::vector<network*>* pnets;
            auto it = _server_nets.find(port);

            if (it == _server_nets.end())
            {
                std::vector<network*> nets;
                auto pr = _server_nets.insert(std::map<int, std::vector<network*>>::value_type(port, nets));
                pnets = &pr.first->second;
                pnets->resize(net_channel::max_value() + 1);
            }
            else
            {
                pnets = &it->second;
            }

            auto net = create_network(sp.second, false);
            if (net == nullptr)
            {
                return ERR_NETWORK_INIT_FAILED;
            }

            (*pnets)[sp.second.channel] = net;
            last_server_port = net->address().port();
            dwarn("[%s] network server started at %s, channel = %s, ...",
                    node()->name(),
                    net->address().to_string(),
                    sp.second.channel.to_string()
                    );
        }

        _local_primary_address = _client_nets[NET_CHANNEL_TCP]->address();
        _local_primary_address.c_addr_ptr()->u.v4.port = aspec.ports.size() > 0 ? 
            (*aspec.ports.begin() != 0 ? *aspec.ports.begin() : last_server_port)
            : aspec.id;

        ddebug("=== service_node=[%s], primary_address=[%s] ===",
            _node->name(), _local_primary_address.to_string());

        _is_running = true;
        return ERR_OK;
    }

    bool rpc_engine::register_rpc_handler(rpc_handler_info* handler)
    {
        return _rpc_dispatcher.register_rpc_handler(handler);
    }

    rpc_handler_info* rpc_engine::unregister_rpc_handler(dsn_task_code_t rpc_code, const char* service_name)
    {
        return _rpc_dispatcher.unregister_rpc_handler(rpc_code, service_name);
    }

    void rpc_engine::on_recv_request(network* net, message_ex* msg, int delay_ms)
    {
        if (!_is_serving)
        {
            dwarn(
                "recv message with rpc name %s from %s when rpc engine is not serving, trace_id = %" PRIu64,
                msg->dheader.rpc_name,
                msg->from_address.to_string(),
                msg->header->trace_id
                );

            delete msg;
            return;
        }

        INSTALL_VIEW_POINT("on_recv_request " + std::to_string(msg->header->trace_id));
        auto code = msg->rpc_code();

        if (code != ::dsn::TASK_CODE_INVALID)
        {
            rpc_request_task* tsk = nullptr;

            // handle virtual nodes
            if (msg->header->context.u.app_id != 0)
            {
                tsk = _node->generate_l2_rpc_request_task(msg);
            }

            // handle common rpc
            else
            {
                tsk = _rpc_dispatcher.on_request(msg, _node);
            }

            if (tsk != nullptr)
            {
                // injector
                if (tsk->spec().on_rpc_request_enqueue.execute(tsk, true))
                {
                    tsk->set_delay(delay_ms);
                    tsk->enqueue();
                }

                // release the task when necessary
                else
                {
                    ddebug("rpc request %s is dropped (fault inject), trace_id = %016" PRIx64,
                        msg->dheader.rpc_name,
                        msg->header->trace_id
                        );

                    delete tsk;
                }
                return;
            }
        }

        dwarn(
            "recv message with unknown rpc name %s from %s, trace_id = %016" PRIx64,
            msg->dheader.rpc_name,
            msg->from_address.to_string(),
            msg->header->trace_id
            );

        delete msg;
    }

    void rpc_engine::reply(message_ex* response, dsn_rpc_error_t err)
    {
        TPF_MARK("rpc_engine::reply");
        INSTALL_VIEW_POINT("reply " + std::to_string(response->header->trace_id));

        response->header->context.u.server_error = err;
        auto sp = task_spec::get(response->local_rpc_code);

        // the following assert is invalid when the to-address is unix-ipc
        // dassert(!response->to_address.is_invalid(), "");

        bool no_fail = sp->on_rpc_reply.execute(task::get_current_task(), response, true);
        
        if (no_fail)
        {
            response->u.server.net->send_reply_message(response);
        }
        else
        {
            dinfo("reply message <%s, trace_id %016" PRIx64 "> dropped due to fault injection AT on_rpc_reply",
                response->dheader.rpc_name,
                response->header->trace_id
                );
            delete response;
        }
    }
}
