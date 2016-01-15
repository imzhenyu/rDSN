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
 *     c++ client side service API
 *
 * Revision history:
 *     Sep., 2015, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# pragma once

# include <dsn/cpp/task_helper.h>
# include <dsn/cpp/function_traits.h>

namespace dsn 
{
    //
    // clientlet is the base class for RPC service and client
    // there can be multiple clientlet in the system
    //
    class clientlet
    {
    public:
        clientlet(int task_bucket_count = 13);
        virtual ~clientlet();

        dsn_task_tracker_t tracker() const { return _tracker; }
        rpc_address primary_address() { return dsn_primary_address(); }

        static uint32_t random32(uint32_t min, uint32_t max) { return dsn_random32(min, max); }
        static uint64_t random64(uint64_t min, uint64_t max) { return dsn_random64(min, max); }
        static uint64_t now_ns() { return dsn_now_ns(); }
        static uint64_t now_us() { return dsn_now_us(); }
        static uint64_t now_ms() { return dsn_now_ms(); }
        
    protected:
        void check_hashed_access();

    private:
        int                            _access_thread_id;
        bool                           _access_thread_id_inited;
        dsn_task_tracker_t             _tracker;
    };

    // common APIs

    namespace tasking 
    {
        task_ptr enqueue(
            dsn_task_code_t evt,
            clientlet* svc,
            task_handler callback,
            int hash = 0,
            int delay_milliseconds = 0,
            int timer_interval_milliseconds = 0
            );

        void enqueue(
            /*our*/ task_ptr* ptask, // null for not returning task handle
            dsn_task_code_t evt,
            clientlet* svc,
            task_handler callback,
            int hash = 0,
            int delay_milliseconds = 0,
            int timer_interval_milliseconds = 0
            );

        template<typename T> // where T : public virtual clientlet
        inline task_ptr enqueue(
            dsn_task_code_t evt,
            T* owner,
            void (T::*callback)(),
            //TParam param,
            int hash = 0,
            int delay_milliseconds = 0,
            int timer_interval_milliseconds = 0
            )
        {
            task_handler h = std::bind(callback, owner);
            return enqueue(
                evt,
                owner,
                h,
                hash,
                delay_milliseconds,
                timer_interval_milliseconds
                );
        }

        template<typename T> // where T : public virtual clientlet
        inline void enqueue(
            /*out*/ task_ptr* ptask,
            dsn_task_code_t evt,
            T* owner,
            void (T::*callback)(),
            //TParam param,
            int hash = 0,
            int delay_milliseconds = 0,
            int timer_interval_milliseconds = 0
            )
        {
            task_handler h = std::bind(callback, owner);
            return enqueue(
                ptask,
                evt,
                owner,
                h,
                hash,
                delay_milliseconds,
                timer_interval_milliseconds
                );
        }

        template<typename THandler>
        inline safe_late_task<THandler>* create_late_task(
            dsn_task_code_t evt,
            THandler callback,
            int hash = 0,
            clientlet* svc = nullptr
            )
        {
            dsn_task_t t;
            auto tsk = new safe_late_task<THandler>(callback);

            tsk->add_ref(); // released in exec callback
            t = dsn_task_create_ex(evt, 
                safe_late_task<THandler>::exec,
                safe_late_task<THandler>::on_cancel,
                tsk, hash, svc ? svc->tracker() : nullptr);

            tsk->set_task_info(t);
            return tsk;
        }
    }

    namespace rpc
    {
        template<typename TCallback>
        //where TCallback = void(error_code, dsn_message, dsn_message_t)
        task_ptr create_rpc_response_task(
            dsn_message_t request,
            clientlet* svc,
            TCallback&& callback,
            int reply_hash = 0)
        {
            task_ptr tsk = new safe_task<TCallback>(std::forward<TCallback>(callback));

            tsk->add_ref(); // released in exec_rpc_response

            auto t = dsn_rpc_create_response_task_ex(
                request,
                safe_task<TCallback >::exec_rpc_response,
                safe_task<TCallback >::on_cancel,
                tsk.get(),
                reply_hash,
                svc ? svc->tracker() : nullptr
                );
            tsk->set_task_info(t);
            return tsk;
        }

        template<typename TCallback>
        //where TCallback = void(error_code, TResponse&&)
        //  where TResponse = DefaultConstructible && DSNSerializable
        task_ptr create_rpc_response_task_typed(
            dsn_message_t request,
            clientlet* svc,
            TCallback&& callback,
            int reply_hash = 0)
        {

            using callback_inspect_t = dsn::function_traits<TCallback>;
            static_assert(callback_inspect_t::arity == 2, "invalid callback function");
            using response_t = typename std::decay<typename callback_inspect_t::arg_t<1>>::type;
            static_assert(std::is_default_constructible<response_t>::value, "cannot wrap a non-trival-constructible type");

            return create_rpc_response_task(
                request,
                svc,
                [cb_fwd = std::forward<TCallback>(callback)](error_code err, dsn_message_t req, dsn_message_t resp)
                {
                    response_t response;
                    if (err == ERR_OK)
                    {
                        ::unmarshall(resp, response);
                    }
                    cb_fwd(err, std::move(response));
                },
                reply_hash);
        }

        template<typename TCallback>
        //where TCallback = void(error_code, dsn_message_t, dsn_message_t)
        task_ptr call(
            ::dsn::rpc_address server,
            dsn_message_t request,
            clientlet* svc,
            TCallback&& callback,
            int reply_hash = 0
            )
        {
            task_ptr t = create_rpc_response_task(request, svc, std::forward<TCallback>(callback), reply_hash);
            dsn_rpc_call(server.c_addr(), t->native_handle());
            return t;
        }


        template<typename TRequest, typename TCallback>
        //where TCallback = void(error_code, TResponse&&)
        task_ptr call_typed(
            ::dsn::rpc_address server,
            dsn_task_code_t code,
            TRequest&& req,
            clientlet* owner,
            TCallback&& callback,
            int request_hash = 0,
            int timeout_milliseconds = 0,
            int reply_hash = 0)
        {
            using callback_inspect_t = dsn::function_traits<TCallback>;
            static_assert(callback_inspect_t::arity == 2, "invalid callback function");
            using response_t = typename std::decay<typename callback_inspect_t::arg_t<1>>::type;
            static_assert(std::is_default_constructible<response_t>::value, "cannot wrap a non-trival-constructible type");

            dsn_message_t msg = dsn_msg_create_request(code, timeout_milliseconds, request_hash);
            ::marshall(msg, std::forward<TRequest>(req));
            auto task = create_rpc_response_task_typed(msg, owner, std::forward<TCallback>(callback), reply_hash);
            dsn_rpc_call(server.c_addr(), task->native_handle());
            return task;
        }


        //
        // for TRequest/TResponse, we assume that the following routines are defined:
        //    marshall(binary_writer& writer, const T& val); 
        //    unmarshall(binary_reader& reader, /*out*/ T& val);
        // either in the namespace of ::dsn::utils or T
        // developers may write these helper functions by their own, or use tools
        // such as protocol-buffer, thrift, or bond to generate these functions automatically
        // for their TRequest and TResponse
        //

        // no callback
        template<typename TRequest>
        void call_one_way_typed(
            ::dsn::rpc_address server,
            dsn_task_code_t code,
            const TRequest& req,
            int hash = 0
            );

        template<typename TRequest>
        ::dsn::error_code call_typed_wait(
            /*out*/ ::dsn::rpc_read_stream* response,
            ::dsn::rpc_address server,
            dsn_task_code_t code,
            const TRequest& req,
            int hash = 0,
            int timeout_milliseconds = 0
            );
    }
    
    namespace file
    {
        task_ptr read(
            dsn_handle_t fh,
            char* buffer,
            int count,
            uint64_t offset,
            dsn_task_code_t callback_code,
            clientlet* svc,
            aio_handler callback,
            int hash = 0
            );

        task_ptr write(
            dsn_handle_t fh,
            const char* buffer,
            int count,
            uint64_t offset,
            dsn_task_code_t callback_code,
            clientlet* svc,
            aio_handler callback,
            int hash = 0
            );

        task_ptr write_vector(
            dsn_handle_t fh,
            const dsn_file_buffer_t* buffers,
            int buffer_count,
            uint64_t offset,
            dsn_task_code_t callback_code,
            clientlet* svc,
            aio_handler callback,
            int hash = 0
            );

        template<typename T> // where T : public virtual clientlet
        inline task_ptr read(
            dsn_handle_t fh,
            char* buffer,
            int count,
            uint64_t offset,
            dsn_task_code_t callback_code,
            T* owner,
            void(T::*callback)(error_code, uint32_t),
            int hash = 0
            )
        {
            aio_handler h = std::bind(callback, owner, std::placeholders::_1, std::placeholders::_2);
            return file::read(fh, buffer, count, offset, callback_code, owner, h, hash);
        }

        template<typename T> // where T : public virtual clientlet
        inline task_ptr write(
            dsn_handle_t fh,
            const char* buffer,
            int count,
            uint64_t offset,
            dsn_task_code_t callback_code,
            T* owner,
            void(T::*callback)(error_code, uint32_t),
            int hash = 0
            )
        {
            aio_handler h = std::bind(callback, owner, std::placeholders::_1, std::placeholders::_2);
            return file::write(fh, buffer, count, offset, callback_code, owner, h, hash);
        }

        task_ptr copy_remote_files(
            ::dsn::rpc_address remote,
            const std::string& source_dir,
            std::vector<std::string>& files,  // empty for all
            const std::string& dest_dir,
            bool overwrite,
            dsn_task_code_t callback_code,
            clientlet* svc,
            aio_handler callback,
            int hash = 0
            );

        inline task_ptr copy_remote_directory(
            ::dsn::rpc_address remote,
            const std::string& source_dir,
            const std::string& dest_dir,
            bool overwrite,
            dsn_task_code_t callback_code,
            clientlet* svc,
            aio_handler callback,
            int hash = 0
            )
        {
            std::vector<std::string> files;
            return copy_remote_files(
                remote, source_dir, files, dest_dir, overwrite,
                callback_code, svc, callback, hash
                );
        }
    }

    // ------------- inline implementation ----------------

    namespace rpc
    {
        namespace internal_use_only
        {
            template<typename TRequest, typename TResponse, typename TCallback, typename TService>
            void on_rpc_response1(TService* svc, TCallback cb, std::shared_ptr<TRequest>& req, ::dsn::error_code err, dsn_message_t request, dsn_message_t response)
            {
                if (err == ERR_OK)
                {
                    std::shared_ptr<TResponse> resp(new TResponse);
                    ::unmarshall(response, *resp);
                    (svc->*cb)(err, req, resp);
                }
                else
                {
                    std::shared_ptr<TResponse> resp(nullptr);
                    (svc->*cb)(err, req, resp);
                }
            }

            template<typename TRequest, typename TResponse, typename TCallback>
            void on_rpc_response2(TCallback& cb, std::shared_ptr<TRequest>& req, ::dsn::error_code err, dsn_message_t request, dsn_message_t response)
            {
                if (err == ERR_OK)
                {
                    std::shared_ptr<TResponse> resp(new TResponse);
                    ::unmarshall(response, *resp);
                    cb(err, req, resp);
                }
                else
                {
                    std::shared_ptr<TResponse> resp(nullptr);
                    cb(err, req, resp);
                }
            }

            template<typename TResponse, typename TCallback, typename TService>
            void on_rpc_response3(TService* svc, TCallback cb, void* param, ::dsn::error_code err, dsn_message_t request, dsn_message_t response)
            {
                TResponse resp;
                if (err == ERR_OK)
                {
                    ::unmarshall(response, resp);
                    (svc->*cb)(err, resp, param);
                }
                else
                {
                    (svc->*cb)(err, resp, param);
                }
            }

            template<typename TResponse, typename TCallback>
            void on_rpc_response4(TCallback& cb, void* param, ::dsn::error_code err, dsn_message_t request, dsn_message_t response)
            {
                TResponse resp;
                if (err == ERR_OK)
                {
                    ::unmarshall(response, resp);
                    cb(err, resp, param);
                }
                else
                {
                    cb(err, resp, param);
                }
            }

            inline task_ptr create_empty_rpc_call(
                dsn_message_t msg,
                int reply_hash
                )
            {
                task_ptr task = new safe_task_handle();
                auto t = dsn_rpc_create_response_task(
                    msg,
                    nullptr,
                    nullptr,
                    reply_hash
                    );
                task->set_task_info(t);
                return task;
            }

            template<typename T, typename TRequest, typename TResponse>
            inline task_ptr create_rpc_call(
                dsn_message_t msg,
                std::shared_ptr<TRequest>& req,
                T* owner,
                void (T::*callback)(error_code, std::shared_ptr<TRequest>&, std::shared_ptr<TResponse>&),
                int reply_hash
                )
            {
                if (callback != nullptr)
                {
                    rpc_reply_handler cb = std::bind(
                        &on_rpc_response1<
                        TRequest,
                        TResponse,
                        void (T::*)(::dsn::error_code, std::shared_ptr<TRequest>&, std::shared_ptr<TResponse>&),
                        T>,
                        owner,
                        callback,
                        req,
                        std::placeholders::_1,
                        std::placeholders::_2,
                        std::placeholders::_3
                        );

                    task_ptr task = new safe_task<rpc_reply_handler>(cb);

                    task->add_ref(); // released in exec_rpc_response
                    auto t = dsn_rpc_create_response_task_ex(
                        msg,
                        safe_task<rpc_reply_handler >::exec_rpc_response,
                        safe_task<rpc_reply_handler >::on_cancel,
                        (void*)task,
                        reply_hash,
                        owner ? owner->tracker() : nullptr
                        );
                    task->set_task_info(t);
                    return task;
                }
                else
                {
                    return create_empty_rpc_call(msg, reply_hash);
                }
            }

            template<typename TRequest, typename TResponse>
            inline task_ptr create_rpc_call(
                dsn_message_t msg,
                std::shared_ptr<TRequest>& req,
                std::function<void(error_code, std::shared_ptr<TRequest>&, std::shared_ptr<TResponse>&)>& callback,
                int reply_hash,
                clientlet* owner
                )
            {
                if (callback != nullptr)
                {
                    rpc_reply_handler cb = std::bind(
                        &on_rpc_response2<
                        TRequest,
                        TResponse,
                        std::function<void(error_code, std::shared_ptr<TRequest>&, std::shared_ptr<TResponse>&)>
                        >,
                        callback,
                        req,
                        std::placeholders::_1,
                        std::placeholders::_2,
                        std::placeholders::_3
                        );

                    task_ptr task = new safe_task<rpc_reply_handler>(cb);

                    task->add_ref(); // released in exec_rpc_response
                    auto t = dsn_rpc_create_response_task_ex(
                        msg,
                        safe_task<rpc_reply_handler >::exec_rpc_response,
                        safe_task<rpc_reply_handler >::on_cancel,
                        (void*)task,
                        reply_hash,
                        owner ? owner->tracker() : nullptr
                        );
                    task->set_task_info(t);
                    return task;
                }
                else
                {
                    return create_empty_rpc_call(msg, reply_hash);
                }
            }

            template<typename T, typename TResponse>
            inline task_ptr create_rpc_call(
                dsn_message_t msg,
                T* owner,
                void(T::*callback)(error_code, const TResponse&, void*),
                void* context,
                int reply_hash
                )
            {
                if (callback != nullptr)
                {
                    rpc_reply_handler cb = std::bind(
                        &on_rpc_response3<
                        TResponse,
                        void(T::*)(::dsn::error_code, const TResponse&, void*),
                        T>,
                        owner,
                        callback,
                        context,
                        std::placeholders::_1,
                        std::placeholders::_2,
                        std::placeholders::_3
                        );

                    task_ptr task = new safe_task<rpc_reply_handler>(cb);

                    task->add_ref(); // released in exec_rpc_response
                    auto t = dsn_rpc_create_response_task_ex(
                        msg,
                        safe_task<rpc_reply_handler >::exec_rpc_response,
                        safe_task<rpc_reply_handler >::on_cancel,
                        (void*)task,
                        reply_hash,
                        owner ? owner->tracker() :  nullptr
                        );
                    task->set_task_info(t);
                    return task;
                }
                else
                {
                    return create_empty_rpc_call(msg, reply_hash);
                }
            }

            template<typename TResponse>
            inline task_ptr create_rpc_call(
                dsn_message_t msg,
                std::function<void(error_code, const TResponse&, void*)>& callback,
                void* context,
                int reply_hash,
                clientlet* owner
                )
            {
                if (callback != nullptr)
                {
                    rpc_reply_handler cb = std::bind(
                        &on_rpc_response4<
                        TResponse,
                        std::function<void(::dsn::error_code, const TResponse&, void*)>
                        >,
                        callback,
                        context,
                        std::placeholders::_1,
                        std::placeholders::_2,
                        std::placeholders::_3
                        );

                    task_ptr task = new safe_task<rpc_reply_handler>(cb);

                    task->add_ref(); // released in exec_rpc_response
                    auto t = dsn_rpc_create_response_task_ex(
                        msg,
                        safe_task<rpc_reply_handler >::exec_rpc_response,
                        safe_task<rpc_reply_handler >::on_cancel,
                        (void*)task,
                        reply_hash,
                        owner ? owner->tracker() : nullptr
                        );
                    task->set_task_info(t);
                    return task;
                }
                else
                {
                    return create_empty_rpc_call(msg, reply_hash);
                }
            }
        }

        template<typename TRequest>
        inline void call_one_way_typed(
            ::dsn::rpc_address server,
            dsn_task_code_t code,
            const TRequest& req,
            int hash
            )
        {
            dsn_message_t msg = dsn_msg_create_request(code, 0, hash);
            ::marshall(msg, req);
            dsn_rpc_call_one_way(server.c_addr(), msg);
        }

        template<typename TRequest>
        inline ::dsn::error_code call_typed_wait(
            /*out*/ ::dsn::rpc_read_stream* response,
            ::dsn::rpc_address server,
            dsn_task_code_t code,
            const TRequest& req,
            int hash,
            int timeout_milliseconds /*= 0*/
            )
        {
            dsn_message_t msg = dsn_msg_create_request(code, timeout_milliseconds, hash);
            ::marshall(msg, req);

            auto resp = dsn_rpc_call_wait(server.c_addr(), msg);
            if (resp != nullptr)
            {
                if (response)
                {
                    response->set_read_msg(resp);
                }   
                else
                    dsn_msg_release_ref(resp);
                return ::dsn::ERR_OK;
            }
            else
                return ::dsn::ERR_TIMEOUT;
        }
    }
    
} // end namespace



