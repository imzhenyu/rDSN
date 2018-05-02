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

# include <dsn/service_api_c.h>
# include <dsn/tool_api.h>
# include <dsn/utility/enum_helper.h>
# include <dsn/cpp/auto_codes.h>
# include <dsn/cpp/serialization.h>
# include <dsn/tool-api/task_spec.h>
# include <dsn/tool-api/zlock_provider.h>
# include <dsn/tool-api/nfs.h>
# include <dsn/tool-api/env_provider.h>
# include <dsn/utility/factory_store.h>
# include <dsn/tool-api/task.h>
# include <dsn/tool-api/channel.h>
# include <dsn/utility/singleton_store.h>
# include <dsn/utility/misc.h> 
# include <dsn/tool-api/thread_profiler.h>
# include <dsn/cpp/zlocks.h>
# include <dsn/tool-api/view_point.h>

# include <dsn/utility/configuration.h>
# include "command_manager.h"
# include "service_engine.h"
# include "rpc_engine.h"
# include "disk_engine.h"
# include "task_engine.h"
# include "coredump.h"
# include "crc.h"
# include "transient_memory.h"
# include "library_utils.h"
# include <fstream>

# ifndef _WIN32
# include <signal.h>
# include <unistd.h>
# else
# include <TlHelp32.h>
# endif

# ifdef __TITLE__
# undef __TITLE__
# endif
# define __TITLE__ "service_api_c"

//------------------------------------------------------------------------------
//
// common types
//
//------------------------------------------------------------------------------
struct dsn_error_placeholder {};
class error_code_mgr : public ::dsn::utils::customized_id_mgr < dsn_error_placeholder >
{
public:
    error_code_mgr()
    {
        auto err = register_id("ERR_OK"); // make sure ERR_OK is always registered first
        dassert(0 == err, "");
    }
};

DSN_API dsn_error_t dsn_error_register(const char* name)
{
    return static_cast<dsn_error_t>(error_code_mgr::instance().register_id(name));
}

DSN_API const char* dsn_error_to_string(dsn_error_t err)
{
    return error_code_mgr::instance().get_name(static_cast<int>(err));
}

DSN_API dsn_error_t dsn_error_from_string(const char* s, dsn_error_t default_err)
{
    auto r = error_code_mgr::instance().get_id(s);
    return r == -1 ? default_err : r;
}

DSN_API volatile int* dsn_task_queue_virtual_length_ptr(
    dsn_task_code_t code,
    int hash
    )
{
    return dsn::task::get_current_node()->computation()->get_task_queue_virtual_length_ptr(code, hash);
}

// use ::dsn::threadpool_code2; for parsing purpose
DSN_API dsn_threadpool_code_t dsn_threadpool_code_register(const char* name)
{
    return static_cast<dsn_threadpool_code_t>(
        ::dsn::utils::customized_id_mgr< ::dsn::threadpool_code2_>::instance().register_id(name)
        );
}

DSN_API const char* dsn_threadpool_code_to_string(dsn_threadpool_code_t pool_code)
{
    return ::dsn::utils::customized_id_mgr< ::dsn::threadpool_code2_>::instance().get_name(static_cast<int>(pool_code));
}

DSN_API dsn_threadpool_code_t dsn_threadpool_code_from_string(const char* s, dsn_threadpool_code_t default_code)
{
    auto r = ::dsn::utils::customized_id_mgr< ::dsn::threadpool_code2_>::instance().get_id(s);
    return r == -1 ? default_code : r;
}

DSN_API int dsn_threadpool_code_max()
{
    return ::dsn::utils::customized_id_mgr< ::dsn::threadpool_code2_>::instance().max_value();
}

DSN_API int dsn_threadpool_get_current_tid()
{
    return ::dsn::utils::get_current_tid();
}

struct task_code_placeholder { };
DSN_API dsn_task_code_t dsn_task_code_register(
    const char* name, 
    dsn_task_type_t type,
    dsn_task_priority_t pri,
    dsn_threadpool_code_t pool
    )
{
    auto r = static_cast<dsn_task_code_t>(::dsn::utils::customized_id_mgr<task_code_placeholder>::instance().register_id(name));
    ::dsn::task_spec::register_task_code(r, type, pri, pool);
    //dinfo("%s register %s (%d)", __FUNCTION__, name, r);
    return r;
}

DSN_API void dsn_task_code_query(dsn_task_code_t code, dsn_task_type_t *ptype, dsn_task_priority_t *ppri, dsn_threadpool_code_t *ppool)
{
    auto sp = ::dsn::task_spec::get(code);
    dassert(sp != nullptr, "");
    if (ptype) *ptype = sp->type;
    if (ppri) *ppri = sp->priority;
    if (ppool) *ppool = sp->pool_code;
}

DSN_API void dsn_task_code_set_threadpool(dsn_task_code_t code, dsn_threadpool_code_t pool)
{
    auto sp = ::dsn::task_spec::get(code);
    dassert(sp != nullptr, "");
    sp->pool_code = pool;
}

DSN_API void dsn_task_code_set_priority(dsn_task_code_t code, dsn_task_priority_t pri)
{
    auto sp = ::dsn::task_spec::get(code);
    dassert(sp != nullptr, "");
    sp->priority = pri;
}

DSN_API const char* dsn_task_code_to_string(dsn_task_code_t code)
{
    return ::dsn::utils::customized_id_mgr<task_code_placeholder>::instance().get_name(static_cast<int>(code));
}

DSN_API dsn_task_code_t dsn_task_code_from_string(const char* s, dsn_task_code_t default_code)
{
    auto r = ::dsn::utils::customized_id_mgr<task_code_placeholder>::instance().get_id(s);
    if (r == -1)
    {
        dwarn("invalid task code parsed: '%s'", s);
        return default_code;
    }
    else
        return r;
}

DSN_API int dsn_task_code_max()
{
    return ::dsn::utils::customized_id_mgr<task_code_placeholder>::instance().max_value();
}

DSN_API const char* dsn_task_type_to_string(dsn_task_type_t tt)
{
    return enum_to_string(tt);
}

DSN_API const char* dsn_task_priority_to_string(dsn_task_priority_t tt)
{
    return enum_to_string(tt);
}

DSN_API void dsn_coredump()
{
    ::dsn::utils::coredump::write(); 
    ::abort();
}

DSN_API uint32_t dsn_crc32_compute(const void* ptr, size_t size, uint32_t init_crc)
{
    return ::dsn::utils::crc32::compute(ptr, size, init_crc);
}

DSN_API uint32_t dsn_crc32_concatenate(uint32_t xy_init, uint32_t x_init, uint32_t x_final, size_t x_size, uint32_t y_init, uint32_t y_final, size_t y_size)
{
    return ::dsn::utils::crc32::concatenate(
        0,
        x_init, x_final, (uint64_t)x_size,
        y_init, y_final, (uint64_t)y_size
        );
}


DSN_API uint64_t dsn_crc64_compute(const void* ptr, size_t size, uint64_t init_crc)
{
    return ::dsn::utils::crc64::compute(ptr, size, init_crc);
}

DSN_API uint64_t dsn_crc64_concatenate(uint32_t xy_init, uint64_t x_init, uint64_t x_final, size_t x_size, uint64_t y_init, uint64_t y_final, size_t y_size)
{
    return ::dsn::utils::crc64::concatenate(
        0,
        x_init, x_final, (uint64_t)x_size,
        y_init, y_final, (uint64_t)y_size
        );
}

DSN_API void dsn_task_lpc(
                dsn_task_code_t code,
                dsn_task_handler_t cb,
                void* context,
                int hash,
                int delay_milliseconds
        )
{
    auto t = new ::dsn::task_c(code, cb, context, hash);
    t->spec().on_task_create.execute(::dsn::task::get_current_task(), t);
    t->set_delay(delay_milliseconds);
    t->enqueue();
}

DSN_API dsn_timer_t dsn_task_create_timer(
                dsn_task_code_t code,
                dsn_task_handler_t cb,
                dsn_task_handler_t deletor,
                void* context,
                int hash,
                int interval_milliseconds, ///< must be > 0
                int delay_milliseconds
            )
{
    dassert(interval_milliseconds > 0, "given interval_milliseconds must be larger than 0");

    auto t = new ::dsn::timer_task(code, cb, deletor, context, interval_milliseconds, hash);
    t->spec().on_task_create.execute(::dsn::task::get_current_task(), t);
    t->set_delay(delay_milliseconds);
    t->enqueue();
    return t;
}

DSN_API void dsn_task_stop_timer(dsn_timer_t task)
{
    auto timer = (::dsn::timer_task*)task;
    timer->stop();
}

//------------------------------------------------------------------------------
//
// synchronization - concurrent access and coordination among threads
//
//------------------------------------------------------------------------------
DSN_API dsn_handle_t dsn_exlock_create(bool recursive)
{
    if (recursive)
    {
        ::dsn::lock_provider* last = ::dsn::utils::factory_store< ::dsn::lock_provider>::create(
            ::dsn::service_engine::fast_instance().spec().lock_factory_name.c_str(), ::dsn::PROVIDER_TYPE_MAIN, nullptr);

        // TODO: perf opt by saving the func ptrs somewhere
        for (auto& s : ::dsn::service_engine::fast_instance().spec().lock_aspects)
        {
            last = ::dsn::utils::factory_store< ::dsn::lock_provider>::create(s.c_str(), ::dsn::PROVIDER_TYPE_ASPECT, last);
        }

        return (dsn_handle_t)dynamic_cast< ::dsn::ilock*>(last);
    }
    else
    {
        ::dsn::lock_nr_provider* last = ::dsn::utils::factory_store< ::dsn::lock_nr_provider>::create(
            ::dsn::service_engine::fast_instance().spec().lock_nr_factory_name.c_str(), ::dsn::PROVIDER_TYPE_MAIN, nullptr);

        // TODO: perf opt by saving the func ptrs somewhere
        for (auto& s : ::dsn::service_engine::fast_instance().spec().lock_nr_aspects)
        {
            last = ::dsn::utils::factory_store< ::dsn::lock_nr_provider>::create(s.c_str(), ::dsn::PROVIDER_TYPE_ASPECT, last);
        }

        return (dsn_handle_t)dynamic_cast< ::dsn::ilock*>(last);
    }
}

DSN_API void dsn_exlock_destroy(dsn_handle_t l)
{
    delete (::dsn::ilock*)(l);
}

DSN_API void dsn_exlock_lock(dsn_handle_t l)
{
    ((::dsn::ilock*)(l))->lock();
    ::dsn::lock_checker::zlock_exclusive_count++;
}

DSN_API bool dsn_exlock_try_lock(dsn_handle_t l)
{
    auto r = ((::dsn::ilock*)(l))->try_lock();
    if (r)
    {
        ::dsn::lock_checker::zlock_exclusive_count++;
    }
    return r;
}

DSN_API void dsn_exlock_unlock(dsn_handle_t l)
{
    ::dsn::lock_checker::zlock_exclusive_count--;
    ((::dsn::ilock*)(l))->unlock();
}

// non-recursive rwlock
DSN_API dsn_handle_t dsn_rwlock_nr_create()
{
    ::dsn::rwlock_nr_provider* last = ::dsn::utils::factory_store< ::dsn::rwlock_nr_provider>::create(
        ::dsn::service_engine::fast_instance().spec().rwlock_nr_factory_name.c_str(), ::dsn::PROVIDER_TYPE_MAIN, nullptr);

    // TODO: perf opt by saving the func ptrs somewhere
    for (auto& s : ::dsn::service_engine::fast_instance().spec().rwlock_nr_aspects)
    {
        last = ::dsn::utils::factory_store< ::dsn::rwlock_nr_provider>::create(s.c_str(), ::dsn::PROVIDER_TYPE_ASPECT, last);
    }
    return (dsn_handle_t)(last);
}

DSN_API void dsn_rwlock_nr_destroy(dsn_handle_t l)
{
    delete (::dsn::rwlock_nr_provider*)(l);
}

DSN_API void dsn_rwlock_nr_lock_read(dsn_handle_t l)
{
    ((::dsn::rwlock_nr_provider*)(l))->lock_read();
    ::dsn::lock_checker::zlock_shared_count++;
}

DSN_API void dsn_rwlock_nr_unlock_read(dsn_handle_t l)
{
    ::dsn::lock_checker::zlock_shared_count--;
    ((::dsn::rwlock_nr_provider*)(l))->unlock_read();
}

DSN_API bool dsn_rwlock_nr_try_lock_read(dsn_handle_t l)
{
    auto r = ((::dsn::rwlock_nr_provider*)(l))->try_lock_read();
    if (r) ::dsn::lock_checker::zlock_shared_count++;
    return r;
}

DSN_API void dsn_rwlock_nr_lock_write(dsn_handle_t l)
{
    ((::dsn::rwlock_nr_provider*)(l))->lock_write();
    ::dsn::lock_checker::zlock_exclusive_count++;
}

DSN_API void dsn_rwlock_nr_unlock_write(dsn_handle_t l)
{
    ::dsn::lock_checker::zlock_exclusive_count--;
    ((::dsn::rwlock_nr_provider*)(l))->unlock_write();
}

DSN_API bool dsn_rwlock_nr_try_lock_write(dsn_handle_t l)
{
    auto r = ((::dsn::rwlock_nr_provider*)(l))->try_lock_write();
    if (r) ::dsn::lock_checker::zlock_exclusive_count++;
    return r;
}

DSN_API dsn_handle_t dsn_semaphore_create(int initial_count)
{
    ::dsn::semaphore_provider* last = ::dsn::utils::factory_store< ::dsn::semaphore_provider>::create(
        ::dsn::service_engine::fast_instance().spec().semaphore_factory_name.c_str(), ::dsn::PROVIDER_TYPE_MAIN, initial_count, nullptr);

    // TODO: perf opt by saving the func ptrs somewhere
    for (auto& s : ::dsn::service_engine::fast_instance().spec().semaphore_aspects)
    {
        last = ::dsn::utils::factory_store< ::dsn::semaphore_provider>::create(
            s.c_str(), ::dsn::PROVIDER_TYPE_ASPECT, initial_count, last);
    }
    return (dsn_handle_t)(last);
}

DSN_API void dsn_semaphore_destroy(dsn_handle_t s)
{
    delete (::dsn::semaphore_provider*)(s);
}

DSN_API void dsn_semaphore_signal(dsn_handle_t s, int count)
{
    ((::dsn::semaphore_provider*)(s))->signal(count);
}

DSN_API void dsn_semaphore_wait(dsn_handle_t s)
{
    ::dsn::lock_checker::check_wait_safety();
    ((::dsn::semaphore_provider*)(s))->wait();
}

DSN_API bool dsn_semaphore_wait_timeout(dsn_handle_t s, int timeout_milliseconds)
{
    return ((::dsn::semaphore_provider*)(s))->wait(timeout_milliseconds);
}

//------------------------------------------------------------------------------
//
// rpc
//
//------------------------------------------------------------------------------

// rpc calls

DSN_API dsn_channel_t dsn_rpc_channel_open(
        const char* target,
        const char* channel, // DEFAULT("NET_CHANNEL_TCP"),
        const char* header_format, // DEFAULT("NET_HDR_DSN")
        bool is_async // DEFAULT(true)
    )
{
    if (target == nullptr)
        return nullptr;

    char* target2 = (char*)alloca(strlen(target) + 1 + strlen("raw://"));
    auto s = strstr(target, "://");
    if (s == nullptr)
    {
        dinfo("cannot find '://' when open channel, switch to raw://%s ", target);
        strcpy(target2, "raw://");
        strcat(target2 + strlen("raw://"), target);
        target = target2;
        s = strstr(target, "://");
    }

    auto net_ch = ::dsn::net_channel::from_string(channel, ::dsn::NET_CHANNEL_INVALID);
    if (net_ch == ::dsn::NET_CHANNEL_INVALID)
    {
        derror("invalid network channel type: '%s' ", channel);
        return nullptr;
    }

    auto hdr_fmt = ::dsn::net_header_format::from_string(header_format, ::dsn::NET_HDR_INVALID);
    if (hdr_fmt == ::dsn::NET_HDR_INVALID)
    {
        derror("invalid network header format type: '%s'", header_format);
        return nullptr;
    }
    
    auto st = std::string(target).substr(0, s - target);
    ::dsn::channel* ch = 
        ::dsn::utils::factory_store<::dsn::channel>::create(
            st.c_str(), ::dsn::PROVIDER_TYPE_MAIN, nullptr
            );
    
    if (ch == nullptr)
    {
        derror("cannot find rpc channel type %s, create channel '%s' failed", st.c_str(), target);
        return nullptr;
    }
    
    auto err = ch->open(target, net_ch, hdr_fmt, is_async);
    if (err != ::dsn::ERR_OK)
    {
        derror("create channel '%s' failed, err = %s", target, err.to_string());
        return nullptr;
    }
    return ch;
}

DSN_API void dsn_rpc_channel_close(dsn_channel_t ch)
{
    auto rch = (::dsn::channel*)ch;
    rch->close();
    return;
}

DSN_API void dsn_rpc_call(
                dsn_channel_t ch,
                dsn_message_t request,
                dsn_rpc_response_handler_t cb,
                void* context,
                int reply_thread_hash
            )
{
    TPF_MARK("dsn_rpc_call");

    auto rtask = new ::dsn::rpc_response_task(
        (dsn::message_ex*)request, cb, context, reply_thread_hash);
    rtask->spec().on_task_create.execute(::dsn::task::get_current_task(), rtask);

    auto rch = (::dsn::channel*)ch;
    rch->call((::dsn::message_ex*)request, rtask);
}

struct wait_rpc_context
{
    dsn_rpc_error_t err;
    dsn_message_t   reply;
    void*            context;
    ::dsn::zevent   notify_event;
};

static bool __wait_rpc_callback(
    dsn_rpc_error_t err,
    dsn_message_t reply,
    void* context
)
{
    auto ctx = (wait_rpc_context*)context;
    ctx->err = err;
    ctx->reply = reply;
    ctx->context = context;
    ctx->notify_event.set();
    return false;
}

DSN_API dsn_message_t dsn_rpc_call_wait(dsn_channel_t ch, dsn_message_t request)
{
    auto rch = (::dsn::channel*)ch;
    auto msg = ((::dsn::message_ex*)request);

    if (rch->is_async()) 
    {
        static thread_local wait_rpc_context ctx;
        ::dsn::rpc_response_task* rtask = 
            new ::dsn::rpc_response_task(msg, __wait_rpc_callback, &ctx, 0);
        rtask->set_inline();
        rtask->spec().on_task_create.execute(::dsn::task::get_current_task(), rtask);
        
        rch->call(msg, rtask);
        ctx.notify_event.wait();
        INSTALL_VIEW_POINT("event.wait_back "+std::to_string(msg->header->trace_id));
        return ctx.reply;
    }
    else 
    {
        rch->call(msg, nullptr);
        return rch->recv_block();
    }
}

DSN_API void dsn_rpc_call_one_way(dsn_channel_t ch, dsn_message_t request)
{
    auto msg = ((::dsn::message_ex*)request);
    auto rch = (::dsn::channel*)ch;
    rch->call(msg, nullptr);
}

DSN_API dsn_address_t dsn_primary_address()
{
    return ::dsn::task::get_current_rpc()->primary_address().c_addr();
}

DSN_API bool dsn_rpc_register_handler(
    const char* service_name,
    dsn_task_code_t code,
    const char* method_name, 
    dsn_rpc_request_handler_t cb, 
    void* param, 
    dsn_gpid gpid
    )
{
    ::dsn::rpc_handler_info* h(new ::dsn::rpc_handler_info(code));
    h->service_name = service_name;
    h->method_name = method_name;
    h->c_handler = cb;
    h->parameter = param;

    bool r = ::dsn::task::get_current_node()->rpc_register_handler(h, gpid);
    if (!r)
    {
        delete h;
    }
    return r;
}

DSN_API void* dsn_rpc_unregiser_handler(const char* service_name, dsn_task_code_t code, dsn_gpid gpid)
{
    auto h = ::dsn::task::get_current_node()->rpc_unregister_handler(code, gpid, service_name);
    void* param = nullptr;

    if (nullptr != h)
    {
        param = h->parameter;
        delete h;
    }

    return param;
}

DSN_API void dsn_rpc_reply(dsn_message_t response, dsn_rpc_error_t err)
{
    auto msg = ((::dsn::message_ex*)response);
    ::dsn::task::get_current_rpc()->reply(msg, err);
}

//------------------------------------------------------------------------------
//
// file operations
//
//------------------------------------------------------------------------------

DSN_API dsn_handle_t dsn_file_open(const char* file_name, int flag, int pmode)
{
    return ::dsn::task::get_current_disk()->open(file_name, flag, pmode);
}

DSN_API dsn_error_t dsn_file_close(dsn_handle_t file)
{
    return ::dsn::task::get_current_disk()->close(file);
}

DSN_API dsn_error_t dsn_file_flush(dsn_handle_t file)
{
    return ::dsn::task::get_current_disk()->flush(file);
}

// native HANDLE: HANDLE for windows, int for non-windows
DSN_API void* dsn_file_native_handle(dsn_handle_t file)
{
    auto dfile = (::dsn::disk_file*)file;
    return dfile->native_handle();
}

DSN_API void dsn_file_read(dsn_handle_t file, char* buffer, int count, uint64_t offset, 
                dsn_task_code_t code, dsn_aio_handler_t cb, void* context, int hash)
{
    auto callback = new ::dsn::aio_task(code, cb, context, hash);
    callback->spec().on_task_create.execute(::dsn::task::get_current_task(), callback);
    callback->aio()->buffer = buffer;
    callback->aio()->buffer_size = count;
    callback->aio()->engine = nullptr;
    callback->aio()->file = file;
    callback->aio()->file_offset = offset;
    callback->aio()->type = ::dsn::AIO_Read;

    ::dsn::task::get_current_disk()->read(callback);
}

DSN_API void dsn_file_write(dsn_handle_t file, const char* buffer, int count, uint64_t offset,
    dsn_task_code_t code, dsn_aio_handler_t cb, void* context, int hash)
{
    auto callback = new ::dsn::aio_task(code, cb, context, hash);
    callback->spec().on_task_create.execute(::dsn::task::get_current_task(), callback);
    callback->aio()->buffer = (char*)buffer;
    callback->aio()->buffer_size = count;
    callback->aio()->engine = nullptr;
    callback->aio()->file = file;
    callback->aio()->file_offset = offset;
    callback->aio()->type = ::dsn::AIO_Write;

    ::dsn::task::get_current_disk()->write(callback);
}

DSN_API void dsn_file_write_vector(dsn_handle_t file, const dsn_file_buffer_t* buffers, int buffer_count, uint64_t offset,
    dsn_task_code_t code, dsn_aio_handler_t cb, void* context, int hash)
{
    auto callback = new ::dsn::aio_task(code, cb, context, hash);
    callback->spec().on_task_create.execute(::dsn::task::get_current_task(), callback);
    callback->aio()->buffer = nullptr;
    callback->aio()->buffer_size = 0;
    callback->aio()->engine = nullptr;
    callback->aio()->file = file;
    callback->aio()->file_offset = offset;
    callback->aio()->type = ::dsn::AIO_Write;
    for (int i = 0; i < buffer_count; i ++)
    {
        callback->_unmerged_write_buffers.push_back(buffers[i]);
        callback->aio()->buffer_size += buffers[i].size;
    }

    ::dsn::task::get_current_disk()->write(callback);
}

DSN_API void dsn_file_copy_remote_directory(dsn_address_t remote, const char* source_dir, 
    const char* dest_dir, bool overwrite,
    dsn_task_code_t code, dsn_aio_handler_t cb, void* context, int hash)
{
    auto callback = new ::dsn::aio_task(code, cb, context, hash);
    callback->spec().on_task_create.execute(::dsn::task::get_current_task(), callback);

    std::shared_ptr< ::dsn::remote_copy_request> rci(new ::dsn::remote_copy_request());
    rci->source = remote;
    rci->source_dir = source_dir;
    rci->files.clear();
    rci->dest_dir = dest_dir;
    rci->overwrite = overwrite;

    ::dsn::task::get_current_nfs()->call(rci, callback);
}

DSN_API void dsn_file_copy_remote_files(dsn_address_t remote, const char* source_dir, const char** source_files, const char* dest_dir, bool overwrite,
    dsn_task_code_t code, dsn_aio_handler_t cb, void* context, int hash)
{
    auto callback = new ::dsn::aio_task(code, cb, context, hash);
    callback->spec().on_task_create.execute(::dsn::task::get_current_task(), callback);

    std::shared_ptr< ::dsn::remote_copy_request> rci(new ::dsn::remote_copy_request());
    rci->source = remote;
    rci->source_dir = source_dir;

    rci->files.clear();
    const char** p = source_files;
    while (*p != nullptr && **p != '\0')
    {
        rci->files.push_back(*p);
        p++;

        dinfo("copy remote file %s from %s", 
            *(p-1),
            rci->source.to_string()
            );
    }

    rci->dest_dir = dest_dir;
    rci->overwrite = overwrite;

    ::dsn::task::get_current_nfs()->call(rci, callback);
}

//------------------------------------------------------------------------------
//
// env
//
//------------------------------------------------------------------------------
DSN_API uint64_t dsn_now_ns()
{
    //return ::dsn::task::get_current_env()->now_ns();
    return ::dsn::service_engine::instance().env()->now_ns();
}

DSN_API uint64_t dsn_random64(uint64_t min, uint64_t max) // [min, max]
{
    return ::dsn::service_engine::instance().env()->random64(min, max);
}

//------------------------------------------------------------------------------
//
// system
//
//------------------------------------------------------------------------------

# ifdef _WIN32
static BOOL SuspendAllThreads()
{
    std::map<uint32_t, HANDLE> threads;
    uint32_t dwCurrentThreadId = ::GetCurrentThreadId();
    uint32_t dwCurrentProcessId = ::GetCurrentProcessId();
    HANDLE hSnapshot;
    bool bChange = TRUE;

    while (bChange) 
    {
        hSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) 
        {
            derror("CreateToolhelp32Snapshot failed, err = %d", ::GetLastError());
            return FALSE;
        }

        THREADENTRY32 ti;
        ZeroMemory(&ti, sizeof(ti));
        ti.dwSize = sizeof(ti);
        bChange = FALSE;

        if (FALSE == ::Thread32First(hSnapshot, &ti)) 
        {
            derror("Thread32First failed, err = %d", ::GetLastError());
            goto err;
        }

        do 
        {
            if (ti.th32OwnerProcessID == dwCurrentProcessId &&
                ti.th32ThreadID != dwCurrentThreadId &&
                threads.find(ti.th32ThreadID) == threads.end()) 
                {
                    HANDLE hThread = ::OpenThread(THREAD_ALL_ACCESS, FALSE, ti.th32ThreadID);
                    if (hThread == NULL) 
                    {
                        derror("OpenThread failed, err = %d", ::GetLastError());
                        goto err;
                    }
                    ::SuspendThread(hThread);
                    ddebug("thread %d find and suspended ...", ti.th32ThreadID);
                    threads.insert(std::make_pair(ti.th32ThreadID, hThread));
                    bChange = TRUE;
                }
        } while (::Thread32Next(hSnapshot, &ti));

        ::CloseHandle(hSnapshot);
    }

    return TRUE;

err:
    ::CloseHandle(hSnapshot);
    return FALSE;
}
#endif

NORETURN DSN_API void dsn_exit(int code)
{
    printf("dsn exit with code %d\n", code);
    fflush(stdout);
    ::dsn::tools::sys_exit.execute(::dsn::SYS_EXIT_NORMAL);

# if defined(_WIN32)
    // TODO: do not use std::map above, coz when suspend the other threads, they may stop
    // inside certain locks which causes deadlock
    // SuspendAllThreads();
    ::TerminateProcess(::GetCurrentProcess(), code);
# else    
    _exit(code);
 // kill(getpid(), SIGKILL);
# endif
}

DSN_API bool dsn_mimic_app(const char* app_name, int index)
{
    auto worker = ::dsn::task::get_current_worker2();
    dassert(worker == nullptr, "cannot call dsn_mimic_app in rDSN threads");

    auto cnode = ::dsn::task::get_current_node2();
    if (cnode != nullptr)
    {
        const auto& name = cnode->spec().name;
        if (cnode->spec().role_name == ::dsn::safe_string(app_name)
            && cnode->spec().index == index)
        {
            return true;
        }
        else
        {
            derror("current thread is already attached to another rDSN app %s", name.c_str());
            return false;
        }
    }

    auto nodes = ::dsn::service_engine::instance().get_all_nodes();
    for (auto& n : nodes)
    {
        if (n.second->spec().role_name == ::dsn::safe_string(app_name)
            && n.second->spec().index == index)
        {
            ::dsn::task::set_tls_dsn_context(n.second, nullptr);
            return true;
        }
    }

    derror("cannot find host app %s with index %d", app_name, index);
    return false;
}

DSN_API const char* dsn_get_app_data_dir(dsn_gpid gpid)
{
    auto info = dsn_get_app_info_ptr(gpid);
    return info ? info->data_dir : nullptr;
}

DSN_API bool dsn_get_current_app_info(/*out*/ dsn_app_info* app_info)
{
    auto info = dsn_get_app_info_ptr(dsn_gpid{ .value = 0 });
    if (info)
    {
        memcpy(app_info, info, sizeof(*info));
        return true;
    }
    else
        return false;
}

DSN_API dsn_app_info* dsn_get_app_info_ptr(dsn_gpid gpid)
{
    auto cnode = ::dsn::task::get_current_node2();
    if (cnode != nullptr)
    {
        if (gpid.value == 0)
            return cnode->get_l1_info();
        else
        {
            return cnode->get_l2_handler().get_app_info(gpid);
        }
    }
    else
        return nullptr;
}

::dsn::utils::notify_event s_loader_event;
DSN_API void dsn_app_loader_signal()
{
    s_loader_event.notify();
}

DSN_API void dsn_app_loader_wait()
{
    s_loader_event.wait();
}
