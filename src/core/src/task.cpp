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
 *     task is the execution of a piece of sequence code, which completes
 *     a meaningful application level task.
 *
 * Revision history:
 *     Mar., 2015, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# include <dsn/service_api_c.h>
# include <dsn/tool-api/task.h>
# include <dsn/tool-api/env_provider.h>
# include <dsn/utility/misc.h>
# include <dsn/utility/synchronize.h>
# include <dsn/tool-api/node_scoper.h>
# include <dsn/tool-api/thread_profiler.h>

# include "task_engine.h"
# include "service_engine.h"
# include "service_engine.h"
# include "disk_engine.h"
# include "rpc_engine.h"


# ifdef __TITLE__
# undef __TITLE__
# endif
# define __TITLE__ "task"

namespace dsn 
{
__thread struct __tls_dsn__ tls_dsn;
__thread uint16_t tls_dsn_lower32_task_id_mask = 0;

/*static*/ void task::set_tls_dsn(const __tls_dsn__* ctx)
{
    tls_dsn = *ctx;
}

/*static*/ void task::get_tls_dsn(/*out*/ __tls_dsn__* ctx)
{
    *ctx = tls_dsn;
}

/*static*/ void task::set_tls_dsn_context(
    service_node* node,  // cannot be null
    task_worker* worker // null for io or timer threads if they are not worker threads
    )
{
    memset(static_cast<void*>(&tls_dsn), 0, sizeof(tls_dsn));
    tls_dsn.magic = 0xdeadbeef;
    tls_dsn.worker_index = -1;
    tls_dsn.pool_id = -1;

    if (node)
    {
        tls_dsn.node_id = node->id();

        if (worker != nullptr)
        {
            dassert(worker->pool()->node() == node,
                "worker not belonging to the given node: %s vs %s",
                worker->pool()->node()->name(),
                node->name()
                );
        }

        tls_dsn.node = node;
        tls_dsn.worker = worker;
        tls_dsn.worker_index = worker ? worker->index() : -1;
        tls_dsn.current_task = nullptr;
        tls_dsn.pool_id = worker ? (int)worker->pool_spec().pool_code : -1;
        tls_dsn.rpc = node->rpc();
        tls_dsn.disk = node->disk();
        tls_dsn.env = service_engine::fast_instance().env();
        tls_dsn.nfs = node->nfs();
        tls_dsn.tsvc = node->tsvc();
    }

    tls_dsn.node_pool_thread_ids = (node ? ((uint64_t)(uint8_t)node->id()) : 0) << (64 - 8); // high 8 bits for node id
    tls_dsn.node_pool_thread_ids |= (worker ? ((uint64_t)(uint8_t)(int)worker->pool_spec().pool_code) : 0) << (64 - 8 - 8); // next 8 bits for pool id
    auto worker_idx = worker ? worker->index() : -1;
    if (worker_idx == -1)
    {
        worker_idx = utils::get_current_tid();
    }
    tls_dsn.node_pool_thread_ids |= ((uint64_t)(uint16_t)worker_idx) << 32; // next 16 bits for thread id
    tls_dsn.last_lower32_task_id = worker ? 0 : ((uint32_t)(++tls_dsn_lower32_task_id_mask)) << 16;
}

/*static*/ void task::on_tls_dsn_not_set()
{
    if (service_engine::instance().spec().enable_default_app_mimic)
    {
        dsn_mimic_app("mimic", 1);
    }
    else
    {
        dassert(false, "rDSN context is not initialized properly, to be fixed as follows:\n"
            "(1). the current thread does NOT belongs to any rDSN service node, please invoke dsn_mimic_app first,\n"
            "     or, you can enable [core] enable_default_app_mimic = true in your config file so mimic_app can be omitted\n"
            "(2). the current thread belongs to a rDSN service node, and you are writing providers for rDSN, please use\n"
            "     task::set_tls_dsn_context(...) at the beginning of your new thread in your providers;\n"
            "(3). this should not happen, please help fire an issue so we we can investigate"            
            );
    }
}

task::task(dsn_task_code_t code, void* context, int hash, service_node* node)
    : _state(TASK_STATE_READY)
{
    _spec = task_spec::get(code);
    _allow_inline = _spec->allow_inline;
    _context = context;
    _hash = hash;
    _delay_milliseconds = 0;
    next = nullptr;
    
    if (node != nullptr)
    {
        _node = node;
    }
    else
    {
        auto p = get_current_node();
        dassert(p != nullptr, "tasks without explicit service node "
            "can only be created inside threads which is attached to specific node");
        _node = p;
    }

    if (tls_dsn.magic != 0xdeadbeef)
    {
        set_tls_dsn_context(nullptr, nullptr);
    }

    _task_id = tls_dsn.node_pool_thread_ids + (++tls_dsn.last_lower32_task_id);
}

task::~task()
{
}

void task::set_retry(bool enqueue_immediately /*= true*/)
{
    dassert(task::get_current_task() == this, "only callable from the curren task");
    dassert(TASK_STATE_RUNNING == _state, "invalid task state");
    _state = TASK_STATE_READY;
}

void task::exec_internal()
{
    dassert(TASK_STATE_READY == _state, "invalid task state");

    _state = TASK_STATE_RUNNING;
    dassert(tls_dsn.magic == 0xdeadbeef, "thread is not inited with task::set_tls_dsn_context");

    task* parent_task = tls_dsn.current_task;
    tls_dsn.current_task = this;

    _spec->on_task_begin.execute(this);

    exec();

    if (_state == TASK_STATE_RUNNING)
    {
        _state = TASK_STATE_FINISHED;
        _spec->on_task_end.execute(this);
        delete this;
    }
    else
    {
        _spec->on_task_end.execute(this);
        enqueue();
    }

    tls_dsn.current_task = parent_task;
    /*
    if (!_spec->allow_inline && !_is_null)
    {
        lock_checker::check_dangling_lock();
    }
    */
}

const char* task::get_current_node_name()
{
    auto n = get_current_node2();
    return n ? n->name() : "unknown";
}

service_node* task::get_first_node() 
{
    const ::dsn::service_nodes_by_app_id& nodes = ::dsn::service_engine::fast_instance().get_all_nodes();
    return nodes.size() > 0 ? nodes.begin()->second : nullptr;
}

const char* task::node_name() const
{
    return _node->name();
}

void task::enqueue()
{        
    dassert(_node != nullptr, "service node unknown for this task");
    dassert(_spec->type != TASK_TYPE_RPC_RESPONSE,
        "tasks with TASK_TYPE_RPC_RESPONSE type use task::enqueue(caller_pool()) instead");
    auto pool = node()->computation()->get_pool(spec().pool_code);
    enqueue(pool);
}

void task::enqueue(task_worker_pool* pool)
{
    TPF_MARK("task::enqueue");
    // for delayed tasks, refering to timer service
    if (_delay_milliseconds != 0)
    {
        _node->tsvc()->add_timer(this);
        return;
    }

    // fast execution
    if (_allow_inline)
    {
        // inlined
        // warning - this may lead to deadlocks, e.g., allow_inlined
        // task tries to get a non-recursive lock that is already hold
        // by the caller task
        
        if (_node != get_current_node())
        {
            tools::node_scoper ns(_node);
            exec_internal();
        }
        else
        {
            exec_internal();
        }

        return;
    }

    dassert(pool != nullptr, "pool %s not ready, and there are usually two cases: "
        "(1). thread pool not designatd in '[%s] pools'; "
        "(2). the caller is executed in io threads "
        "which is usually forbidden unless you explicitly set [task.%s].allow_inline = true",
        dsn_threadpool_code_to_string(_spec->pool_code),
        _node->spec().config_section.c_str(),
        _spec->name.c_str()
        );
    // normal path
    pool->enqueue(this);
}

timer_task::timer_task(
    dsn_task_code_t code, 
    dsn_task_handler_t cb, 
    dsn_task_handler_t deletor, 
    void* context,
    uint32_t interval_milliseconds, 
    int hash, 
    service_node* node
    )
    : task(code, context, hash, node),
    _interval_milliseconds(interval_milliseconds),
    _deletor(deletor),
    _cb(cb)
{
    dassert (TASK_TYPE_COMPUTE == spec().type,
        "%s is not a computation type task, please use DEFINE_TASK_CODE to define the task code",
        spec().name.c_str()
        );
}

void timer_task::enqueue()
{
    // enable timer randomization to avoid lots of timers execution simultaneously
    if (delay_milliseconds() == 0 && spec().randomize_timer_delay_if_zero)
    {
        set_delay(dsn_random32(0, _interval_milliseconds));
    }
    
    spec().on_task_enqueue.execute(get_current_task(), this);
    return task::enqueue();
}

void timer_task::exec()
{
    _cb(_context);

    if (_interval_milliseconds > 0)
    {
        set_retry();
        set_delay(_interval_milliseconds);
    }
}

void timer_task::stop()
{
    // TODO: safe stop (e.g., exec is running ...)
    _interval_milliseconds = 0;
    dinfo("stop timer task %016" PRIx64, id());
}


rpc_request_task::rpc_request_task(
        message_ex* request,
        dsn_rpc_request_handler_t handler,
        void* param,
        service_node* node)
    : task(
        dsn_task_code_t(request->local_rpc_code),  // TODO(Zhenyu): it is possible that request->local_rpc_code != h->code when it is handled in frameworks
        nullptr, 
        request->header->client.thread_hash,
        node),
    _request(request),
    _handler(handler),
    _param(param),
    _enqueue_ts_ns(0)
{
    dbg_dassert (TASK_TYPE_RPC_REQUEST == spec().type, 
        "%s is not a RPC_REQUEST task, please use DEFINE_TASK_CODE_RPC to define the task code",
        spec().name.c_str()
        );
}

rpc_request_task::~rpc_request_task()
{
    if (_request)
        delete _request;
}

void rpc_request_task::enqueue()
{
    if (spec().rpc_request_dropped_before_execution_when_timeout)
    {
        _enqueue_ts_ns = dsn_now_ns();
    }
    task::enqueue(node()->computation()->get_pool(spec().pool_code));
}

rpc_response_task::rpc_response_task(
    message_ex* request, 
    dsn_rpc_response_handler_t cb,
    void* context, 
    int hash, 
    service_node* node
    )
    : task(task_spec::get(request->local_rpc_code)->rpc_paired_code, context,
           hash == 0 ? request->header->client.thread_hash : hash, node)
{
    _cb = cb;
    _rpc_error = RPC_ERR_TIMEOUT;
    _trace_id = request->header->trace_id;

    set_error_code(ERR_IO_PENDING);

    dbg_dassert (TASK_TYPE_RPC_RESPONSE == spec().type, 
        "%s is not of RPC_RESPONSE type, please use DEFINE_TASK_CODE_RPC to define the request task code",
        spec().name.c_str()
        );

    _response = nullptr;

    _caller_pool = get_current_worker() ? 
        get_current_worker()->pool() : nullptr;
}

rpc_response_task::~rpc_response_task()
{
    if (_response) delete _response;
}

bool rpc_response_task::enqueue(dsn_rpc_error_t err, message_ex* reply)
{
    set_error_code(err);
    _rpc_error = err;
    _response = reply;

    bool ret = true;
    if (!spec().on_rpc_response_enqueue.execute(this, true))
    {
        set_error_code(ERR_NETWORK_FAILURE);        
        ret = false;
    }
    
    rpc_response_task::enqueue();
    return ret;
}

void rpc_response_task::enqueue()
{
    if (_caller_pool)
        task::enqueue(_caller_pool);

    // possible when it is called in non-rDSN threads
    else
    {
        auto pool = node()->computation()->get_pool(spec().pool_code);
        task::enqueue(pool);
    }
}

struct hook_context : public transient_object
{
    dsn_rpc_response_handler_t old_callback;
    void* old_context;
    dsn_rpc_response_handler_replace_t new_callback;
    uint64_t new_context;
};

static bool rpc_response_task_hook_callback(dsn_rpc_error_t err, dsn_message_t resp, void* ctx)
{
    auto nc = (hook_context*)ctx;
    auto r = nc->new_callback(
        nc->old_callback,
        err,
        resp,
        nc->old_context,
        nc->new_context
    );
    delete nc;
    return r;
};

void rpc_response_task::replace_callback(dsn_rpc_response_handler_replace_t callback, uint64_t context)
{
    hook_context* nc = new hook_context();
    nc->old_callback = _cb;
    nc->old_context = _context;
    nc->new_callback = callback;
    nc->new_context = context;

    _context = nc;
    _cb = rpc_response_task_hook_callback;
}

bool rpc_response_task::reset_callback()
{
    if (_cb == rpc_response_task_hook_callback)
    {
        if (state() != TASK_STATE_READY &&
            state() != TASK_STATE_RUNNING)
            return false;

        // ready or running
        auto nc = (hook_context*)_context;
        _context = nc->old_context;
        _cb = nc->old_callback;

        if (state() != TASK_STATE_RUNNING)
        {
            delete nc;
        }

        return true;
    }
    else
    {
        return false;
    }
}

aio_task::aio_task(
    dsn_task_code_t code, 
    dsn_aio_handler_t cb, 
    void* context, 
    int hash,
    service_node* node
    )
    : task(code, context, hash, node)
{
    _cb = cb;

    dassert (TASK_TYPE_AIO == spec().type, 
        "%s is not of AIO type, please use DEFINE_TASK_CODE_AIO to define the task code",
        spec().name.c_str()
        );
    set_error_code(ERR_IO_PENDING);
    _aio = get_current_disk()->prepare_aio_context(this);
}

aio_task::~aio_task()
{
    delete _aio;
}

void aio_task::enqueue(error_code err, size_t transferred_size)
{
    set_error_code(err);
    _transferred_size = transferred_size;

    spec().on_aio_enqueue.execute(this);

    task::enqueue(node()->computation()->get_pool(spec().pool_code));
}

} // end namespace
