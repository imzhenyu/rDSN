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
 *     task and execution model 
 *
 * Revision history:
 *     Feb., 2016, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# pragma once

# include <dsn/c/api_common.h>

# ifdef __cplusplus
extern "C" {
# endif

/*!
@addtogroup task-common
@{
 */

/*! task/event type definition */
typedef enum dsn_task_type_t
{
    TASK_TYPE_RPC_REQUEST,   ///< task handling rpc request
    TASK_TYPE_RPC_RESPONSE,  ///< task handling rpc response or timeout
    TASK_TYPE_COMPUTE,       ///< async calls or timers
    TASK_TYPE_AIO,           ///< callback for file read and write
    TASK_TYPE_CONTINUATION,  ///< above tasks are seperated into several continuation
                             ///< tasks by thread-synchronization operations.
                             ///< so that each "task" is non-blocking
    TASK_TYPE_COUNT,
    TASK_TYPE_INVALID
} dsn_task_type_t;

typedef enum dsn_rpc_error_t
{
    RPC_ERR_OK,
    RPC_ERR_TIMEOUT,
    RPC_ERR_HANDLER_NOT_FOUND,
    RPC_ERR_SERVER_BUSY,
    RPC_ERR_UNKNOWN,
    RPC_ERR_COUNT
} dsn_rpc_error_t;

/*! callback prototype for \ref TASK_TYPE_COMPUTE */
typedef void(*dsn_task_handler_t)(
    void* ///< void* context
    );

/*! callback prototype for \ref TASK_TYPE_RPC_REQUEST, return false when
    request is used asynchronously and developers need to destroy it 
    explicitly using dsn_msg_destroy.
*/
typedef bool (*dsn_rpc_request_handler_t)(
    dsn_message_t, ///< incoming request
    void*           ///< handler context registered
    );

/*! callback prototype for \ref TASK_TYPE_RPC_RESPONSE, return false when
    request/response are used asynchronously and developers need to destroy them 
    explicitly using dsn_msg_destroy or resend them via dsn_rpc_xxx
*/
typedef bool (*dsn_rpc_response_handler_t)(
    dsn_rpc_error_t, ///< rpc error
    dsn_message_t,   ///< incoming rpc response
    void*             ///< context when rpc is called
    );

/*! callback prototype for \ref TASK_TYPE_AIO */
typedef void(*dsn_aio_handler_t)(
    dsn_error_t,   ///< error code for the io operation
    size_t,         ///< transferred io size
    void*           ///< context when rd/wt is called
    );

/*! task priority */
typedef enum dsn_task_priority_t
{
    TASK_PRIORITY_LOW,
    TASK_PRIORITY_COMMON,
    TASK_PRIORITY_HIGH,
    TASK_PRIORITY_COUNT,
    TASK_PRIORITY_INVALID
} dsn_task_priority_t;

/*! define a new thread pool with a given name */
extern DSN_API dsn_threadpool_code_t  dsn_threadpool_code_register(const char* name);
extern DSN_API const char*            dsn_threadpool_code_to_string(dsn_threadpool_code_t pool_code);
extern DSN_API dsn_threadpool_code_t  dsn_threadpool_code_from_string(
                                        const char* s, 
                                        dsn_threadpool_code_t default_code // when s is not registered
                                        );
extern DSN_API int                    dsn_threadpool_code_max();
extern DSN_API int                    dsn_threadpool_get_current_tid();

/*! register a new task code */
extern DSN_API dsn_task_code_t        dsn_task_code_register(
                                        const char* name,          // task code name
                                        dsn_task_type_t type,
                                        dsn_task_priority_t, 
                                        dsn_threadpool_code_t pool // in which thread pool the tasks run
                                        );
extern DSN_API void                  dsn_task_code_query(
                                        dsn_task_code_t code, 
                                        /*out*/ dsn_task_type_t *ptype, 
                                        /*out*/ dsn_task_priority_t *ppri, 
                                        /*out*/ dsn_threadpool_code_t *ppool
                                        );
extern DSN_API void                  dsn_task_code_set_threadpool( // change thread pool for this task code
                                        dsn_task_code_t code, 
                                        dsn_threadpool_code_t pool
                                        );
extern DSN_API void                  dsn_task_code_set_priority(dsn_task_code_t code, dsn_task_priority_t pri);
extern DSN_API const char*           dsn_task_code_to_string(dsn_task_code_t code);
extern DSN_API dsn_task_code_t       dsn_task_code_from_string(const char* s, dsn_task_code_t default_code);
extern DSN_API int                   dsn_task_code_max();
extern DSN_API const char*           dsn_task_type_to_string(dsn_task_type_t tt);
extern DSN_API const char*           dsn_task_priority_to_string(dsn_task_priority_t tt);

/*!
apps updates the value at dsn_task_queue_virtual_length_ptr(..) to control
the length of a vitual queue (bound to current code + hash) to
enable customized throttling, see spec of thread pool for more information
*/
extern DSN_API volatile int*         dsn_task_queue_virtual_length_ptr(
                                        dsn_task_code_t code,
                                        int hash DEFAULT(0)
                                        );

/*@}*/

# ifdef __cplusplus
}
# endif