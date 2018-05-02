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

# include "service_engine.h"
# include "task_engine.h"
# include "disk_engine.h"
# include "rpc_engine.h"
# include "perf_counters.h"
# include <dsn/tool-api/env_provider.h>
# include <dsn/tool-api/memory_provider.h>
# include <dsn/tool-api/nfs.h>
# include <dsn/utility/factory_store.h>
# include <dsn/tool-api/command.h>
# include <dsn/tool-api/perf_counter.h>
# include <dsn/tool_api.h>
# include <dsn/tool-api/node_scoper.h>
# include <dsn/cpp/framework.h>

# ifdef __TITLE__
# undef __TITLE__
# endif
# define __TITLE__ "service_engine"

using namespace dsn::utils;

namespace dsn {

DEFINE_TASK_CODE_RPC(RPC_L2_CLIENT_READ, TASK_PRIORITY_COMMON, THREAD_POOL_DEFAULT)
DEFINE_TASK_CODE_RPC(RPC_L2_CLIENT_WRITE, TASK_PRIORITY_COMMON, THREAD_POOL_DEFAULT)

service_node::service_node(service_app_spec& app_spec)
    : _framework(this), 
    _layer2_rpc_redsn_handler(RPC_L2_CLIENT_READ),
    _layer2_rpc_write_handler(RPC_L2_CLIENT_WRITE)
{
    _computation = nullptr;
    _app_spec = app_spec;

    memset(&_app_info, 0, sizeof(_app_info));
    _app_info.app.app_context_ptr = nullptr;
    _app_info.app_id = id();
    _app_info.index = spec().index;
    strncpy(_app_info.role, spec().role_name.c_str(), sizeof(_app_info.role));
    strncpy(_app_info.type, spec().type.c_str(), sizeof(_app_info.type));
    strncpy(_app_info.name, spec().name.c_str(), sizeof(_app_info.name));
    strncpy(_app_info.data_dir, spec().data_dir.c_str(), sizeof(_app_info.data_dir)); 
    
    _layer2_rpc_redsn_handler.method_name = "RPC_L2_CLIENT_READ";
    _layer2_rpc_redsn_handler.c_handler = [](dsn_message_t req, void* this_) 
    {
        auto req2 = (message_ex*)req;
        return ((service_node*)this_)->handle_l2_rpc_request(
            req2->get_gpid(),
            false,
            req
            );
    };
    _layer2_rpc_redsn_handler.parameter = this;

    _layer2_rpc_write_handler.method_name = "RPC_L2_CLIENT_WRITE";
    _layer2_rpc_write_handler.c_handler = [](dsn_message_t req, void* this_)
    {
        auto req2 = (message_ex*)req;
        return ((service_node*)this_)->handle_l2_rpc_request(
            req2->get_gpid(),
            true,
            req
        );
    };
    _layer2_rpc_write_handler.parameter = this;
}

bool service_node::rpc_register_handler(rpc_handler_info* handler, dsn_gpid gpid)
{
    if (gpid.value == 0)
    {
        if (_node_io.rpc)
        {
            bool r = _node_io.rpc->register_rpc_handler(handler);
            if (!r)
                return false;
        }
    }
    else
    {
        _framework.rpc_register_handler(gpid, handler);
    }
    return true;
}

rpc_handler_info* service_node::rpc_unregister_handler(dsn_task_code_t rpc_code, dsn_gpid gpid, const char* service_name)
{
    if (gpid.value == 0)
    {
        return rpc()->unregister_rpc_handler(rpc_code, service_name);
    }
    else
    {
        return _framework.rpc_unregister_handler(gpid, rpc_code, service_name);
    }
}

error_code service_node::init_io_engine()
{
    io_engine& io = _node_io;
    auto& spec = service_engine::fast_instance().spec();
    error_code err = ERR_OK;
    
    // init timer service
    io.tsvc = factory_store<timer_service>::create(
        service_engine::fast_instance().spec().timer_factory_name.c_str(),
        PROVIDER_TYPE_MAIN, this, nullptr);
    for (auto& s : service_engine::fast_instance().spec().timer_aspects)
    {
        io.tsvc = factory_store<timer_service>::create(
            s.c_str(),
            PROVIDER_TYPE_ASPECT,
            this, io.tsvc
            );
    }

    // init disk engine
    io.disk = new disk_engine(this);
    aio_provider* aio = factory_store<aio_provider>::create(
        spec.aio_factory_name.c_str(), ::dsn::PROVIDER_TYPE_MAIN, io.disk, nullptr);
    for (auto it = spec.aio_aspects.begin();
        it != spec.aio_aspects.end();
        it++)
    {
        aio = factory_store<aio_provider>::create(it->c_str(),
            PROVIDER_TYPE_ASPECT, io.disk, aio);
    }
    io.aio = aio;

    // init rpc engine
    io.rpc = new rpc_engine(get_main_config(), this);
    
    // init nfs
    io.nfs = nullptr;
    if (!spec.start_nfs)
    {
        ddebug("nfs not started coz [core] start_nfs = false");
    }
    else if (spec.nfs_factory_name == "")
    {
        ddebug("nfs not started coz no nfs_factory_name is specified,"
            " continue with no nfs");
    }
    else
    {
        io.nfs = factory_store<nfs_node>::create(spec.nfs_factory_name.c_str(),
            PROVIDER_TYPE_MAIN, this);

        for (auto& anfs : spec.nfs_aspects)
        {
            io.nfs = utils::factory_store<nfs_node>::create(
                anfs.c_str(), ::dsn::PROVIDER_TYPE_ASPECT, this, io.nfs);
        }
    }

    return err;
}

error_code service_node::start_io_engine_in_main()
{
    //auto& spec = service_engine::fast_instance().spec();
    error_code err = ERR_OK;
    
    // start timer service    
    if (_node_io.tsvc)
    {
        _node_io.tsvc->start();
    }

    // start disk engine
    if (_node_io.disk)
    {
        _node_io.disk->start(_node_io.aio);
    }

    // start rpc engine
    if (_node_io.rpc)
    {
        err = _node_io.rpc->start(_app_spec);
        if (err != ERR_OK) return err;
    }

    return err;
}


error_code service_node::start_io_engine_in_node_start_task()
{
    error_code err = ERR_OK;
    
    // start nfs delayed when the app is started
    if (_node_io.nfs)
    {
        err = _node_io.nfs->start();
        if (err != ERR_OK) return err;
    }

    return err;
}

dsn_error_t service_node::start_app()
{
    return start_app(_app_info.app.app_context_ptr, spec().arguments, _app_spec.role->layer1.start, spec().name);
}

dsn_error_t service_node::start_app(void* app_context, const safe_string& sargs, dsn_app_start start, const safe_string& app_name)
{
    std::vector<std::string> args;
    std::vector<char*> args_ptr;

    utils::split_args(sargs.c_str(), args);

    int argc = static_cast<int>(args.size()) + 1;
    args_ptr.resize(argc);
    args.resize(argc);
    for (int i = argc - 1; i >= 0; i--)
    {
        if (0 == i)
        {
            args[0] = app_name.c_str();
        }
        else
        {
            args[i] = args[i - 1];
        }

        args_ptr[i] = ((char*)args[i].c_str());
    }

    return start(app_context, argc, &args_ptr[0]);
}

error_code service_node::start()
{
    error_code err = ERR_OK;

    // init data dir
    if (!dsn::utils::filesystem::path_exists(spec().data_dir.c_str()))
        dsn::utils::filesystem::create_directory(spec().data_dir.c_str());

    // init task engine    
    _computation = new task_engine(this);
    _computation->create(_app_spec.pools);    
    dassert (!_computation->is_started(), 
        "task engine must not be started at this point");

    // init per node io engines
    err = init_io_engine();
    if (err != ERR_OK) return err;

    // start task engine
    _computation->start();
    dassert(_computation->is_started(),
        "task engine must be started at this point");

    // start io engines (only timer, disk and rpc), others are started in app start task
    start_io_engine_in_main();

    // create app
    {
        ::dsn::tools::node_scoper scoper(this);
        _app_info.app.app_context_ptr = _app_spec.role->layer1.create(_app_spec.role->type_name, dsn_gpid{ .value = 0 });
    }

    // start rpc serving
    rpc()->start_serving();

    return err;
}

void service_node::get_runtime_info(
    const safe_string& indent, 
    const safe_vector<safe_string>& args,
    /*out*/ safe_sstream& ss
    )
{
    ss << indent << name() << ":" << std::endl;

    auto indent2 = indent + "\t";
    _computation->get_runtime_info(indent2, args, ss);

    rpc()->get_runtime_info(indent2, args, ss);
}

void service_node::get_queue_info(
    /*out*/ safe_sstream& ss
    )
{
    ss << "{\"app_name\":\"" << name() << "\",\n\"thread_pool\":[\n";
    _computation->get_queue_info(ss);
    ss << "]}";
}

bool service_node::handle_l2_rpc_request(dsn_gpid gpid, bool is_write, dsn_message_t req)
{
    auto cb = _app_spec.role->layer2.frameworks.on_rpc_request;
    return cb(_app_info.app.app_context_ptr, gpid, is_write, req);
}

rpc_request_task* service_node::generate_l2_rpc_request_task(message_ex* req)
{
    auto cb = _app_spec.role->layer2.frameworks.on_rpc_request;
    if (nullptr != cb)
    {
        rpc_request_task* t;
        if (task_spec::get(req->local_rpc_code)->rpc_request_is_write_operation)
        {
            t = new rpc_request_task(req, _layer2_rpc_write_handler.c_handler, _layer2_rpc_write_handler.parameter, this);
        }
        else
        {
            t = new rpc_request_task(req, _layer2_rpc_redsn_handler.c_handler, _layer2_rpc_redsn_handler.parameter, this);
        }
        t->spec().on_task_create.execute(nullptr, t);
        return t;
    }
    else
    {
        return nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

service_engine::service_engine(void)
{
    _env = nullptr;
    _logging = nullptr;
    _memory = nullptr;

    ::dsn::register_command("engine", "engine - get engine internal information",
        "engine [app-id]",
        &service_engine::get_runtime_info
        );
    ::dsn::register_command("system.queue", "system.queue - get queue internal information",
        "system.queue",
        &service_engine::get_queue_info
        );
}

void service_engine::init_before_toollets(const service_spec& spec)
{
    _spec = spec;

    // init common providers (first half)
    _logging = factory_store<logging_provider>::create(
        spec.logging_factory_name.c_str(), ::dsn::PROVIDER_TYPE_MAIN, spec.dir_log.c_str(), nullptr
        );

    for (auto& alog : spec.logging_aspects)
    {
        _logging = utils::factory_store<logging_provider>::create(
            alog.c_str(), ::dsn::PROVIDER_TYPE_ASPECT, spec.dir_log.c_str(), _logging);
    }

    _memory = factory_store<memory_provider>::create(
        spec.memory_factory_name.c_str(), ::dsn::PROVIDER_TYPE_MAIN
        );

    perf_counters::instance().register_factory(
        factory_store<perf_counter>::get_factory<perf_counter::factory>(
        spec.perf_counter_factory_name.c_str(), ::dsn::PROVIDER_TYPE_MAIN
        )
        );

    // init common for all per-node providers
    message_ex::s_local_hash = (uint32_t)dsn_config_get_value_uint64(
        "core",
        "local_hash",
        0,
        "a same hash value from two processes indicate the rpc code are registered in the same order, "
        "and therefore the mapping between rpc code string and integer is the same, which we leverage "
        "for fast rpc handler lookup optimization"
        );
}

void service_engine::init_after_toollets()
{
    // init common providers (second half)
    _env = factory_store<env_provider>::create(_spec.env_factory_name.c_str(), 
        PROVIDER_TYPE_MAIN, nullptr);
    for (auto it = _spec.env_aspects.begin();
        it != _spec.env_aspects.end();
        it++)
    {
        _env = factory_store<env_provider>::create(it->c_str(), 
            PROVIDER_TYPE_ASPECT, _env);
    }
    tls_dsn.env = _env;
}

void service_engine::register_system_rpc_handler(
    dsn_task_code_t code, 
    const char* name, 
    dsn_rpc_request_handler_t cb, 
    void* param, 
    int port /*= -1*/
    ) // -1 for all node
{
    ::dsn::rpc_handler_info* h(new ::dsn::rpc_handler_info(code));
    h->method_name = name;
    h->c_handler = cb;
    h->parameter = param;

    if (port == -1)
    {
        for (auto& n : _nodes_by_app_id)
        {
            n.second->rpc()->register_rpc_handler(h);
        }
    }
    else
    {
        auto it = _nodes_by_app_port.find(port);
        if (it != _nodes_by_app_port.end())
        {
            it->second->rpc()->register_rpc_handler(h);
        }
        else
        {
            dwarn("cannot find service node with port %d", port);
        }
    }
}

service_node* service_engine::start_node(service_app_spec& app_spec)
{
    auto it = _nodes_by_app_id.find(app_spec.id);
    if (it != _nodes_by_app_id.end())
    {
        return it->second;
    }
    else
    {
        for (auto p : app_spec.ports)
        {
            // union to existing node if any port is shared
            if (_nodes_by_app_port.find(p) != _nodes_by_app_port.end())
            {
                service_node* n = _nodes_by_app_port[p];

                dassert(false, "network port %d usage confliction for %s vs %s, "
                    "please reconfig",
                    p,
                    n->name(),
                    app_spec.name.c_str()
                    );
            }
        }
                
        auto node = new service_node(app_spec);
        error_code err = node->start();
        dassert (err == ERR_OK, "service node start failed, err = %s", err.to_string());
        
        _nodes_by_app_id[node->id()] = node;
        for (auto p1 : node->spec().ports)
        {
            _nodes_by_app_port[p1] = node;
        }

        return node;
    }
}

safe_string service_engine::get_runtime_info(const safe_vector<safe_string>& args)
{
    safe_sstream ss;
    if (args.size() == 0)
    {
        ss << "" << service_engine::fast_instance()._nodes_by_app_id.size() 
            << " nodes available:" << std::endl;
        for (auto& kv : service_engine::fast_instance()._nodes_by_app_id)
        {
            ss << "\t" << kv.second->id() << "." << kv.second->name() << std::endl;
        }
    }
    else
    {
        auto indent = "";
        int id = atoi(args[0].c_str());
        auto it = service_engine::fast_instance()._nodes_by_app_id.find(id);
        if (it != service_engine::fast_instance()._nodes_by_app_id.end())
        {
            auto args2 = args;
            args2.erase(args2.begin());
            it->second->get_runtime_info(indent, args2, ss);
        }
        else
        {
            ss << "cannot find node with given app id";
        }
    }
    return ss.str();
}

safe_string service_engine::get_queue_info(const safe_vector<safe_string>& args)
{
    safe_sstream ss;
    ss << "[";
    for (auto &it : service_engine::fast_instance()._nodes_by_app_id)
    {
        if (it.first != service_engine::fast_instance()._nodes_by_app_id.begin()->first) ss << ",";
        it.second->get_queue_info(ss);
    }
    ss << "]";
    return ss.str();
}

void service_engine::configuration_changed()
{
    task_spec::init();
}

} // end namespace
