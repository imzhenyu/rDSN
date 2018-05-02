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
 *     specification for the labeled tasks (task kinds)
 *
 * Revision history:
 *     Mar., 2015, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# pragma once

# include <dsn/service_api_c.h>
# include <dsn/cpp/auto_codes.h>
# include <dsn/cpp/config_helper.h>
# include <dsn/cpp/safe_string.h>
# include <dsn/utility/enum_helper.h>
# include <dsn/utility/customizable_id.h>
# include <dsn/utility/singleton_vector_store.h>
# include <dsn/utility/join_point.h>
# include <dsn/utility/extensible_object.h>
# include <dsn/utility/exp_delay.h>
# include <dsn/utility/dlib.h>
# include <dsn/tool-api/perf_counter.h>

ENUM_BEGIN(dsn_log_level_t, LOG_LEVEL_INVALID)
    ENUM_REG(LOG_LEVEL_INFORMATION)
    ENUM_REG(LOG_LEVEL_DEBUG)
    ENUM_REG(LOG_LEVEL_WARNING)
    ENUM_REG(LOG_LEVEL_ERROR)
    ENUM_REG(LOG_LEVEL_FATAL)
ENUM_END(dsn_log_level_t)

ENUM_BEGIN(dsn_task_type_t, TASK_TYPE_INVALID)
    ENUM_REG(TASK_TYPE_RPC_REQUEST)
    ENUM_REG(TASK_TYPE_RPC_RESPONSE)
    ENUM_REG(TASK_TYPE_COMPUTE)
    ENUM_REG(TASK_TYPE_AIO)
    ENUM_REG(TASK_TYPE_CONTINUATION)
ENUM_END(dsn_task_type_t)

ENUM_BEGIN(dsn_task_priority_t, TASK_PRIORITY_INVALID)
    ENUM_REG(TASK_PRIORITY_LOW)
    ENUM_REG(TASK_PRIORITY_COMMON)
    ENUM_REG(TASK_PRIORITY_HIGH)
ENUM_END(dsn_task_priority_t)

namespace dsn {
   
enum worker_priority_t
{
    THREAD_xPRIORITY_LOWEST,
    THREAD_xPRIORITY_BELOW_NORMAL,
    THREAD_xPRIORITY_NORMAL,
    THREAD_xPRIORITY_ABOVE_NORMAL,
    THREAD_xPRIORITY_HIGHEST,
    THREAD_xPRIORITY_COUNT,
    THREAD_xPRIORITY_INVALID
};

ENUM_BEGIN(worker_priority_t, THREAD_xPRIORITY_INVALID)
    ENUM_REG(THREAD_xPRIORITY_LOWEST)
    ENUM_REG(THREAD_xPRIORITY_BELOW_NORMAL)
    ENUM_REG(THREAD_xPRIORITY_NORMAL)
    ENUM_REG(THREAD_xPRIORITY_ABOVE_NORMAL)
    ENUM_REG(THREAD_xPRIORITY_HIGHEST)
ENUM_END(worker_priority_t)

enum task_state
{
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_FINISHED,
    TASK_STATE_CANCELLED,
    TASK_STATE_COUNT,
    TASK_STATE_INVALID
};

ENUM_BEGIN(task_state, TASK_STATE_INVALID)
    ENUM_REG(TASK_STATE_READY)
    ENUM_REG(TASK_STATE_RUNNING)
    ENUM_REG(TASK_STATE_FINISHED)
    ENUM_REG(TASK_STATE_CANCELLED)
ENUM_END(task_state)

typedef enum throttling_mode_t
{
    TM_NONE,    // no throttling applied
    TM_REJECT,  // reject the incoming request 
    TM_DELAY,   // delay network receive ops to reducing incoming rate
    TM_COUNT,
    TM_INVALID
} throttling_mode_t;

ENUM_BEGIN(throttling_mode_t, TM_INVALID)
    ENUM_REG(TM_NONE)
    ENUM_REG(TM_REJECT)
    ENUM_REG(TM_DELAY)
ENUM_END(throttling_mode_t)

ENUM_BEGIN(dsn_msg_serialize_format, DSF_INVALID)
    ENUM_REG(DSF_RDSN)
    ENUM_REG(DSF_PROTOC_BINARY)
    ENUM_REG(DSF_PROTOC_JSON)
    ENUM_REG(DSF_JSON)
ENUM_END(dsn_msg_serialize_format)

// define thread pool code 
DEFINE_CUSTOMIZED_ID_TYPE(threadpool_code2)

class task;
class task_queue;
class aio_task;
class rpc_request_task;
class rpc_response_task;
class message_ex;
class admission_controller;
typedef void (*task_rejection_handler)(task*, admission_controller*);
struct rpc_handler_info;

class task_spec : public extensible_object<task_spec, 4>
{
public:
    DSN_API static task_spec* get(int ec);
    DSN_API static task_spec* rpc_get(const char* code);
    DSN_API static void register_task_code(dsn_task_code_t code, dsn_task_type_t type, dsn_task_priority_t pri, dsn_threadpool_code_t pool);

public:
    // not configurable [
    dsn_task_code_t        code;
    dsn_task_type_t        type;
    safe_string            name;    
    dsn_task_code_t        rpc_paired_code;
    shared_exp_delay       rpc_request_delayer;
    // ]

    // configurable [
    dsn_task_priority_t    priority;
    dsn_threadpool_code_t  pool_code;

    // allow task executed in other thread pools or tasks    
    // for TASK_TYPE_COMPUTE - allow-inline allows a task being executed in its caller site
    // for other tasks - allow-inline allows a task being execution in io-thread
    bool                   allow_inline;
    bool                   randomize_timer_delay_if_zero; // to avoid many timers executing at the same time
    dsn_msg_serialize_format rpc_msg_payload_serialize_default_format;

    int32_t                rpc_timeout_milliseconds;
    int32_t                rpc_request_resend_timeout_milliseconds; // 0 for no auto-resend
    throttling_mode_t      rpc_request_throttling_mode; // 
    safe_vector<int>       rpc_request_delays_milliseconds; // see exp_delay for delaying recving
    bool                   rpc_request_dropped_before_execution_when_timeout;

    // layer 2 configurations
    bool                   rpc_request_framework_required; // need layer 2 handler
    bool                   rpc_request_is_write_operation;      // need stateful replication
    bool                   rpc_request_is_write_allow_batch;    // if write allow batch
    // ]

    task_rejection_handler rejection_handler;
    
    // COMPUTE
    /*! 
     @addtogroup tool-api-hooks 
     @{
     */
    join_point<void, task*, task*>               on_task_create;

    join_point<void, task*, task*>               on_task_enqueue;    
    join_point<void, task*>                      on_task_begin; // TODO: parent task
    join_point<void, task*>                      on_task_end;
    join_point<void, task*>                      on_task_cancelled;

    join_point<void, task*, task*, uint32_t>     on_task_wait_pre; // waitor, waitee, timeout
    join_point<void, task*>                      on_task_wait_notified;
    join_point<void, task*, task*, bool>         on_task_wait_post; // wait succeeded or timedout
    join_point<void, task*, task*, bool>         on_task_cancel_post; // cancel succeeded or not
    

    // AIO
    join_point<bool, task*, aio_task*>           on_aio_call; // return true means continue, otherwise early terminate with task::set_error_code
    join_point<void, aio_task*>                  on_aio_enqueue; // aio done, enqueue callback

    // RPC_REQUEST
    join_point<bool, task*, message_ex*, rpc_response_task*>  on_rpc_call; // return true means continue, otherwise dropped and (optionally) timedout
    join_point<bool, rpc_request_task*>          on_rpc_request_enqueue;
    
    // RPC_RESPONSE
    join_point<bool, task*, message_ex*>         on_rpc_reply;
    join_point<bool, rpc_response_task*>         on_rpc_response_enqueue; // response, task

    // message data flow
    join_point<void, message_ex*, message_ex*>   on_rpc_create_response;
    /*@}*/

public:    
    DSN_API task_spec(int code, const char* name, dsn_task_type_t type, dsn_task_priority_t pri, dsn_threadpool_code_t pool);
    
public:
    DSN_API static bool init();
    DSN_API void init_profiling(bool profile);
};

CONFIG_BEGIN(task_spec)
    CONFIG_FLD_ENUM(dsn_task_priority_t, priority, TASK_PRIORITY_COMMON, TASK_PRIORITY_INVALID, true, "task priority")
    CONFIG_FLD_ID(threadpool_code2, pool_code, THREAD_POOL_DEFAULT, false, "thread pool to execute the task")
    CONFIG_FLD(bool, bool, allow_inline, false, 
        "allow task executed in other thread pools or tasks "
        "for TASK_TYPE_COMPUTE - allow-inline allows a task being executed in its caller site "
        "for other tasks - allow-inline allows a task being execution in io-thread "        
        )
    CONFIG_FLD(bool, bool, randomize_timer_delay_if_zero, false, "whether to randomize the timer delay to random(0, timer_interval), if the initial delay is zero, to avoid multiple timers executing at the same time (e.g., checkpointing)")
    CONFIG_FLD_ENUM(dsn_msg_serialize_format, rpc_msg_payload_serialize_default_format, DSF_RDSN, DSF_INVALID, false, "what kind of payload serialization format for this kind of msgs")
    CONFIG_FLD(int32_t, uint64, rpc_timeout_milliseconds, 5000, "what is the default timeout (ms) for this kind of rpc calls")    
    CONFIG_FLD(int32_t, uint64, rpc_request_resend_timeout_milliseconds, 0, "for how long (ms) the request will be resent if no response is received yet, 0 for disable this feature")
    CONFIG_FLD_ENUM(throttling_mode_t, rpc_request_throttling_mode, TM_NONE, TM_INVALID, false, "throttling mode for rpc requets: TM_NONE, TM_REJECT, TM_DELAY when queue length > pool.queue_length_throttling_threshold")
    CONFIG_FLD_INT_LIST(rpc_request_delays_milliseconds, "how many milliseconds to delay recving rpc session for when queue length ~= [1.0, 1.2, 1.4, 1.6, 1.8, >=2.0] x pool.queue_length_throttling_threshold, e.g., 0, 0, 1, 2, 5, 10")
    CONFIG_FLD(bool, bool, rpc_request_dropped_before_execution_when_timeout, false, "whether to drop a request right before execution when its queueing time is already greater than its timeout value")    

    // layer 2 configurations
    CONFIG_FLD(bool, bool, rpc_request_framework_required, false, "whether this request needs to be handled by a layer2 handler (e.g., replicated or partitioned)")
    CONFIG_FLD(bool, bool, rpc_request_is_write_operation, false, "whether this request updates app's state which needs to be replicated using a replication layer2 handler")
    CONFIG_FLD(bool, bool, rpc_request_is_write_allow_batch, true, "whether this write request allows updating app's state in batch mode")

CONFIG_END

struct threadpool_spec
{
    safe_string             name;
    dsn_threadpool_code_t   pool_code;
    int                     worker_count;
    worker_priority_t       worker_priority;
    bool                    worker_share_core;
    uint64_t                worker_affinity_mask;
    int                     dequeue_batch_size;
    bool                    partitioned;         // false by default
    safe_string             queue_factory_name;
    safe_string             worker_factory_name;
    safe_list<safe_string>  queue_aspects;
    safe_list<safe_string>  worker_aspects;
    int                     queue_length_throttling_threshold;
    bool                    enable_virtual_queue_throttling;
    safe_string             admission_controller_factory_name;
    safe_string             admission_controller_arguments;

    threadpool_spec(const dsn_threadpool_code_t& code) : name(dsn_threadpool_code_to_string(code)), pool_code(code) {}
    threadpool_spec(const threadpool_spec& source) = default;
    threadpool_spec& operator=(const threadpool_spec& source) = default;

    DSN_API static bool init(/*out*/ safe_vector<threadpool_spec>& specs);
};

CONFIG_BEGIN(threadpool_spec)
    // CONFIG_FLD_ID(dsn_threadpool_code_t, pool_code) // no need to define it inside section
    CONFIG_FLD_STRING(name, "", "thread pool name")
    CONFIG_FLD(int, uint64, worker_count, 2, "thread/worker count")
    CONFIG_FLD(int, uint64, dequeue_batch_size, 5, "how many tasks (if available) should be returned for one dequeue call for best batching performance") 
    CONFIG_FLD_ENUM(worker_priority_t, worker_priority, THREAD_xPRIORITY_NORMAL, THREAD_xPRIORITY_INVALID, false, "thread priority")
    CONFIG_FLD(bool, bool, worker_share_core, true, "whether the threads share all assigned cores")
    CONFIG_FLD(uint64_t, uint64, worker_affinity_mask, 0, "what CPU cores are assigned to this pool, 0 for all")
    CONFIG_FLD(bool, bool, partitioned, false, "whethe the threads share a single queue(partitioned=false) or not; the latter is usually for workload hash partitioning for avoiding locking")
    CONFIG_FLD_STRING(queue_factory_name, "", "task queue provider name")
    CONFIG_FLD_STRING(worker_factory_name, "", "task worker provider name")
    CONFIG_FLD_STRING_LIST(queue_aspects, "task queue aspects names, usually for tooling purpose")
    CONFIG_FLD_STRING_LIST(worker_aspects, "task aspects names, usually for tooling purpose")    
    CONFIG_FLD(int, uint64, queue_length_throttling_threshold, 1000000, "throttling: throttling threshold above which rpc requests will be dropped")
    CONFIG_FLD(bool, bool, enable_virtual_queue_throttling, false, "throttling: whether to enable throttling with virtual queues")        
    CONFIG_FLD_STRING(admission_controller_factory_name, "", "customized admission controller for the task queues")
    CONFIG_FLD_STRING(admission_controller_arguments, "", "arguments for the cusotmized admission controller")
CONFIG_END

} // end namespace

