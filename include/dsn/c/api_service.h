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
 *     Service API  in rDSN
 *
 * Revision history:
 *     Mar., 2015, @imzhenyu (Zhenyu Guo), first version
 *     Feb., 2016, @imzhenyu (Zhenyu Guo), add comments for V1 release
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# pragma once

# include <dsn/c/api_common.h>
# include <dsn/c/api_task.h>
# include <dsn/c/api_rpc_utilities.h>

# ifdef __cplusplus
extern "C" {
# endif

/*!
 @defgroup service-api-c Core Service API

 @ingroup service-api
    
  Core service API for building applications and distributed frameworks, which 
  covers the major categories that a server application may use, shown in the modules below.

 @{
 */

/*!
 @defgroup tasking Asynchronous Tasks and Timers
 
 Asynchronous Tasks and Timers

 @{
 */

/*!
 create an asynchronous task.

 \param code             the task code, which defines which thread pool executes the task, see
                         \ref dsn_task_code_register for more details.
 \param cb               the callback for executing the task.
 \param context          the context used by the callback.
 \param hash             the hash value, which defines which thread in the target thread pool
                         executes the task, when the pool is partitioned, see remarks for more.
 \param delay_milliseconds when the task is ready to be executed
    
 code defines the thread pool which executes the callback, i.e., [task.%code$] pool_code =
 THREAD_POOL_DEFAULT; hash defines the thread with index hash % worker_count in the
 threadpool to execute the callback, when [threadpool.%pool_code%] partitioned = true.
 */
extern DSN_API void dsn_task_lpc(
                            dsn_task_code_t code,
                            dsn_task_handler_t cb,
                            void* context,
                            int hash DEFAULT(0),
                            int delay_milliseconds DEFAULT(0)
                            );

/*!
 create a timer task

 \param code             the task code, which defines which thread pool executes the task, see
                         \ref dsn_task_code_register for more details.
 \param cb               the callback for executing the task.
 \param context          the context used by the callback.
 \param hash             the hash value, which defines which thread in the target thread pool
                         executes the task, when the pool is partitioned, see remarks for more.
 \param interval_milliseconds timer interval with which the timer executes periodically.

 \param delay_milliseconds when the task is ready to be executed

 \return timer task handle
    
 code defines the thread pool which executes the callback, i.e., [task.%code$] pool_code =
 THREAD_POOL_DEFAULT; hash defines the thread with index hash % worker_count in the
 threadpool to execute the callback, when [threadpool.%pool_code%] partitioned = true.
 */
extern DSN_API dsn_timer_t  dsn_task_create_timer(
                            dsn_task_code_t code, 
                            dsn_task_handler_t cb, 
                            dsn_task_handler_t deletor,
                            void* context, 
                            int hash,
                            int interval_milliseconds, ///< must be > 0
                            int delay_milliseconds DEFAULT(0)
                            );

/*!
stop a timer task

\param task                the timer task handle

*/
extern DSN_API void        dsn_task_stop_timer(dsn_timer_t task);

/*@}*/


/*!
@defgroup rpc Remote Procedure Call (RPC)

Remote Procedure Call (RPC)

Note developers can easily plugin their own implementation to
replace the underneath implementation of the network (e.g., RDMA, simulated network)
@{
*/

/*!
@defgroup rpc-server Server-Side RPC Primitives

Server-Side RPC Primitives
@{
 */
 
/*! register callback to handle RPC request */
extern DSN_API bool          dsn_rpc_register_handler(
                                const char* service_name,
                                dsn_task_code_t code, 
                                const char* method_name,
                                dsn_rpc_request_handler_t cb, 
                                void* context,
                                dsn_gpid gpid DEFAULT(dsn_gpid{ .value = 0 })
                                );

/*! unregister callback to handle RPC request, and returns void* context upon \ref dsn_rpc_register_handler  */
extern DSN_API void*         dsn_rpc_unregiser_handler(
                                const char* service_name,
                                dsn_task_code_t code,
                                dsn_gpid gpid DEFAULT(dsn_gpid{ .value = 0 })
                                );

/*! reply with a response which is created using dsn_msg_create_response */
extern DSN_API void          dsn_rpc_reply(dsn_message_t response, dsn_rpc_error_t err DEFAULT(RPC_ERR_OK));

/*@}*/

/*!
@defgroup rpc-client Client-Side RPC Primitives

Client-Side RPC Primitives
@{
*/

/*!
open a channel for rpc calls
\param target   target := channel-type://destination-address/options
one example is: raw://hostname:54332
or, others
we may have many channel implementations in the future, need to
check the document for each channel type impl for details about
the specification for this target string
\param channel  TCP\UDP\RDMA, etc., see net_channel for options
\param header_format DSN\HTTP\SOFA\GRPC, etc., see net_header_format for options
\param is_async whether this is an async channel that can be multiplexied with multiple rpc calls

\return channel handle, null for error
*/
extern DSN_API dsn_channel_t dsn_rpc_channel_open(
    const char* target,
    const char* channel DEFAULT("NET_CHANNEL_TCP"),
    const char* header_format DEFAULT("NET_HDR_DSN"),
    bool        is_async DEFAULT(true)
);

/*! close the channel  */
extern DSN_API void          dsn_rpc_channel_close(dsn_channel_t ch);

/*!
client invokes the RPC call, and leaves a callback to handle the 
response message from RPC server, or timeout.

\param ch                message transmit channel
\param request           rpc request message
\param cb                callback to handle rpc response or timeout, unlike the other
                         kinds of tasks, response tasks are always executed in the thread pool invoking the rpc
\param context           context used by cb
\param reply_thread_hash if the curren thread pool is partitioned, this specify which thread
 to execute the callback

 */
extern DSN_API void          dsn_rpc_call(
                                dsn_channel_t ch,
                                dsn_message_t request,
                                dsn_rpc_response_handler_t cb,
                                void* context,
                                int reply_thread_hash DEFAULT(0)
                                );

/*! 
   client invokes the RPC call and waits for its response, note 
   returned msg must be explicitly deleted using \ref dsn_msg_destroy
 */
extern DSN_API dsn_message_t  dsn_rpc_call_wait(
                                dsn_channel_t ch, 
                                dsn_message_t request
                                );

/*! one-way RPC from client, no rpc response is expected */
extern DSN_API void          dsn_rpc_call_one_way(
                                dsn_channel_t ch, 
                                dsn_message_t request
                                );

/*@}*/

/*@}*/

/*!
@defgroup file File Operations

File Operations

Note developers can easily plugin their own implementation to
replace the underneath implementation of these primitives.
@{
*/
typedef struct
{
    void* buffer;
    int size;
} dsn_file_buffer_t;

/*! the following ctrl code are used by \ref dsn_file_ctrl. */
typedef enum dsn_ctrl_code_t
{
    CTL_BATCH_INVALID = 0,
    CTL_BATCH_WRITE = 1,            ///< (batch) set write batch size
    CTL_MAX_CON_READ_OP_COUNT = 2,  ///< (throttling) maximum concurrent read ops
    CTL_MAX_CON_WRITE_OP_COUNT = 3, ///< (throttling) maximum concurrent write ops
} dsn_ctrl_code_t;

/*!
 open file

 \param file_name filename of the file.
 \param flag      flags such as O_RDONLY | O_BINARY used by ::open 
 \param pmode     permission mode used by ::open

 \return file handle
 */
extern DSN_API dsn_handle_t  dsn_file_open(
                                const char* file_name, 
                                int flag, 
                                int pmode
                                );

/*! close the file handle */
extern DSN_API dsn_error_t   dsn_file_close(
                                dsn_handle_t file
                                );

/*! flush the buffer of the given file */
extern DSN_API dsn_error_t   dsn_file_flush(
                                dsn_handle_t file
                                );

/*! get native handle: HANDLE for windows, int for non-windows */
extern DSN_API void*         dsn_file_native_handle(dsn_handle_t file);

/*!
 read file asynchronously

 \param file             file handle
 \param buffer           read buffer
 \param count            byte size of the read buffer
 \param offset           offset in the file to start reading

 \param code             task code
 \param cb               callback to be executed
 \param context          context used by cb
 \param hash             specify which thread to execute cb if target pool is partitioned
 */
extern DSN_API void         dsn_file_read(
                                dsn_handle_t file, 
                                char* buffer, 
                                int count, 
                                uint64_t offset, 
                                dsn_task_code_t code,
                                dsn_aio_handler_t cb,
                                void* context,
                                int hash DEFAULT(0)
                                );

/*!
 write file asynchronously

 \param file             file handle
 \param buffer           write buffer
 \param count            byte size of the to-be-written content
 \param offset           offset in the file to start write
 \param code             task code
 \param cb               callback to be executed
 \param context          context used by cb
 \param hash             specify which thread to execute cb if target pool is partitioned
 */
extern DSN_API void         dsn_file_write(
                                dsn_handle_t file, 
                                const char* buffer, 
                                int count, 
                                uint64_t offset, 
                                dsn_task_code_t code,
                                dsn_aio_handler_t cb,
                                void* context,
                                int hash DEFAULT(0)
                                );

/*!
 write file asynchronously with vector buffers

 \param file          file handle
 \param buffers       write buffers
 \param buffer_count  number of write buffers
 \param offset        offset in the file to start write
 \param code          task code
 \param cb            callback to be executed
 \param context       context used by cb
 \param hash          specify which thread to execute cb if target pool is partitioned
 */
extern DSN_API void         dsn_file_write_vector(
                                dsn_handle_t file,
                                const dsn_file_buffer_t* buffers,
                                int buffer_count,
                                uint64_t offset,
                                dsn_task_code_t code,
                                dsn_aio_handler_t cb,
                                void* context,
                                int hash DEFAULT(0)
                                );

/*!
 copy remote directory to the local machine

 \param remote     address of the remote nfs server
 \param source_dir source dir on remote server
 \param dest_dir   destination dir on local server
 \param overwrite  true to overwrite, false to preserve.
 \param code       task code
 \param cb         callback to be executed
 \param context    context used by cb
 \param hash       specify which thread to execute cb if target pool is partitioned
 */
extern DSN_API void         dsn_file_copy_remote_directory(
                                dsn_address_t remote, 
                                const char* source_dir, 
                                const char* dest_dir,
                                bool overwrite, 
                                dsn_task_code_t code,
                                dsn_aio_handler_t cb,
                                void* context,
                                int hash DEFAULT(0)
                                );

/*!
 copy remote files to the local machine

 \param remote       address of the remote nfs server
 \param source_dir   source dir on remote server
 \param source_files zero-ended file string array within the source dir on remote server,
                     when it contains no files, all files within source_dir are copied
 \param dest_dir     destination dir on local server
 \param overwrite    true to overwrite, false to preserve.
 \param code         task code
 \param cb           callback to be executed
 \param context      context used by cb
 \param hash         specify which thread to execute cb if target pool is partitioned
 */
extern DSN_API void         dsn_file_copy_remote_files(
                                dsn_address_t remote,
                                const char* source_dir, 
                                const char** source_files, 
                                const char* dest_dir, 
                                bool overwrite, 
                                dsn_task_code_t code,
                                dsn_aio_handler_t cb,
                                void* context,
                                int hash DEFAULT(0)
                                );

/*@}*/

/*!
@defgroup env Environment

Non-deterministic Environment Input

Note developers can easily plugin their own implementation to
replace the underneath implementation of these primitives.
@{
*/
extern DSN_API uint64_t dsn_now_ns();

/*! return [min, max] */
extern DSN_API uint64_t dsn_random64(uint64_t min, uint64_t max);

__inline uint64_t dsn_now_us() { return dsn_now_ns() / 1000; }
__inline uint64_t dsn_now_ms() { return dsn_now_ns() / 1000000; }

/*! return [min, max] */
__inline uint32_t dsn_random32(uint32_t min, uint32_t max)
{
    return (uint32_t)(dsn_random64(min, max)); 
}

__inline double   dsn_probability()
{
    return (double)(dsn_random64(0, 1000000000)) / 1000000000.0; 
}

/*@}*/

/*!
@defgroup sync Thread Synchornization

Thread Synchornization Primitives

Note developers can easily plugin their own implementation to
replace the underneath implementation of these primitives.
@{
*/

/*!
@defgroup sync-exlock Exlusive Locks
Exlusive Locks
@{
*/

/*! create a recursive? or not exlusive lock*/
extern DSN_API dsn_handle_t  dsn_exlock_create(bool recursive);
extern DSN_API void          dsn_exlock_destroy(dsn_handle_t l);
extern DSN_API void          dsn_exlock_lock(dsn_handle_t l);
extern DSN_API bool          dsn_exlock_try_lock(dsn_handle_t l);
extern DSN_API void          dsn_exlock_unlock(dsn_handle_t l);
/*@}*/

/*!
@defgroup sync-rwlock Non-recursive Read-Write Locks
Non-recursive Read-Write Locks
@{
*/
extern DSN_API dsn_handle_t  dsn_rwlock_nr_create();
extern DSN_API void          dsn_rwlock_nr_destroy(dsn_handle_t l);
extern DSN_API void          dsn_rwlock_nr_lock_read(dsn_handle_t l);
extern DSN_API void          dsn_rwlock_nr_unlock_read(dsn_handle_t l);
extern DSN_API bool          dsn_rwlock_nr_try_lock_read(dsn_handle_t l);
extern DSN_API void          dsn_rwlock_nr_lock_write(dsn_handle_t l);
extern DSN_API void          dsn_rwlock_nr_unlock_write(dsn_handle_t l);
extern DSN_API bool          dsn_rwlock_nr_try_lock_write(dsn_handle_t l);
/*@}*/

/*!
@defgroup sync-sema Semaphore
Semaphore
@{
*/
/*! create a semaphore with initial count equals to inital_count */
extern DSN_API dsn_handle_t dsn_semaphore_create(int initial_count);
extern DSN_API void          dsn_semaphore_destroy(dsn_handle_t s);
extern DSN_API void          dsn_semaphore_signal(dsn_handle_t s, int count);
extern DSN_API void          dsn_semaphore_wait(dsn_handle_t s);
extern DSN_API bool          dsn_semaphore_wait_timeout(
    dsn_handle_t s,
    int timeout_milliseconds
    );
/*@}*/

/*@}*/

/*@}*/

# ifdef __cplusplus
/*! make sure type sizes match as we simply use uint64_t across language boundaries */
inline void dsn_address_size_checker()
{
    static_assert (sizeof(dsn_address_t) == sizeof(uint64_t),
        "sizeof(dsn_address_t) must equal to sizeof(uint64_t)");

    static_assert (sizeof(dsn_gpid) == sizeof(uint64_t),
        "sizeof(dsn_gpid) must equal to sizeof(uint64_t)");    
}

}
# endif
