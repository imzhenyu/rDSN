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
 *     c++ service API (not including rpc service)
 *
 * Revision history:
 *     Sep., 2015, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# pragma once

# include <dsn/cpp/address.h>
# include <dsn/cpp/task_helper.h>
# include <dsn/cpp/function_traits.h>
# include <dsn/cpp/serialization.h>

namespace dsn
{
    namespace tasking
    {
        /*!
        @addtogroup tasking
        @{
        */

        /*! TCallback(closure): void (){} */
        template<typename TCallback>
        void enqueue(
            dsn_task_code_t evt,
            TCallback&& callback,
            int hash = 0,
            std::chrono::milliseconds delay = std::chrono::milliseconds(0))
        {
            using callback_storage_t = typename std::remove_reference<TCallback>::type;
            dsn_task_lpc(
                evt,
                cpp_task<callback_storage_t>::exec,
                new cpp_task<callback_storage_t>(std::forward<TCallback>(callback)),
                hash,
                static_cast<int>(delay.count())
                );
        }

        /*! TCallback(closure): void (){} */
        template<typename TCallback>
        dsn_timer_t start_timer(
            dsn_task_code_t evt,
            TCallback&& callback,
            std::chrono::milliseconds timer_interval,
            int hash = 0,
            std::chrono::milliseconds delay = std::chrono::milliseconds(0))
        {
            using callback_storage_t = typename std::remove_reference<TCallback>::type;
            return dsn_task_create_timer(
                evt,
                timer_safe_task<callback_storage_t>::exec_timer,
                timer_safe_task<callback_storage_t>::delete_timer, 
                new timer_safe_task<callback_storage_t>(std::forward<TCallback>(callback)),
                hash,
                static_cast<int>(timer_interval.count()),
                static_cast<int>(delay.count())
            );
        }

        inline void stop_timer(dsn_timer_t timer)
        {
            dsn_task_stop_timer(timer);
        }
        /*@}*/
    }

    namespace rpc
    {

        /*!
        @addtogroup rpc-client
        @{
        */

        //
        // for TRequest/TResponse, we assume that the following routines are defined:
        //    marshall(binary_writer& writer, const T& val);
        //    unmarshall(binary_reader& reader, /*out*/ T& val);
        // either in the namespace of ::dsn::utils or T
        // developers may write these helper functions by their own, or use tools
        // such as protocol-buffer, thrift, or bond to generate these functions automatically
        // for their TRequest and TResponse
        //

        /*! callback: bool (dsn_rpc_error_t, dsn_message_t response) */
        template<typename TFunction, class Enable = void> struct is_raw_rpc_callback
        {
            constexpr static bool const value = false;
        };

        template<typename TFunction>
        struct is_raw_rpc_callback<TFunction, typename std::enable_if<function_traits<TFunction>::arity == 2>::type>
        {
            using inspect_t = function_traits<TFunction>;
            constexpr static bool const value =
                std::is_same<typename inspect_t::template arg_t<0>, dsn_rpc_error_t>::value
                && std::is_same<typename inspect_t::template arg_t<1>, dsn_message_t>::value
                && std::is_same<typename inspect_t::return_t, bool>::value
                ;
        };

        /*! callback: void (error_code, TResponse&& response) */
        template<typename TFunction, class Enable = void> struct is_typed_rpc_callback
        {
            constexpr static bool const value = false;
        };
        template<typename TFunction>
        struct is_typed_rpc_callback<TFunction, typename std::enable_if<function_traits<TFunction>::arity == 2>::type>
        {
            //todo: check if response_t is marshallable
            using inspect_t = function_traits<TFunction>;
            constexpr static bool const value =
                std::is_same<typename inspect_t::template arg_t<0>, dsn::error_code>::value
                && std::is_same<typename inspect_t::return_t, void>::value
                && std::is_default_constructible<typename std::decay<typename inspect_t::template arg_t<1>>::type>::value;
            using response_t = typename std::decay<typename inspect_t::template arg_t<1>>::type;
        };

        /*! TCallback: bool (dsn_rpc_error_t err, dsn_message_t resp) {} */
        template<typename TCallback>
        typename std::enable_if<is_raw_rpc_callback<TCallback>::value, void>::type
        call_cpp_task(
                dsn_channel_t ch,
                dsn_message_t msg,
                int reply_thread_hash, 
                TCallback&& callback)
        {
            using callback_storage_t = typename std::remove_reference<TCallback>::type;
            dsn_rpc_call(
                ch,
                msg,
                cpp_task<callback_storage_t >::exec_rpc_response,
                new cpp_task<callback_storage_t>(std::forward<TCallback>(callback)),
                reply_thread_hash
            );
        }

        /*! TCallback: void (error_code err, TResponse&& resp){} */
        template<typename TCallback>
        typename std::enable_if<is_typed_rpc_callback<TCallback>::value, void>::type
        call_cpp_task(
                dsn_channel_t ch,
                dsn_message_t msg,
                int reply_thread_hash,
                TCallback&& callback)
        {
            return call_cpp_task(
                ch, msg, reply_thread_hash,
                [cb_fwd = std::forward<TCallback>(callback)](dsn_rpc_error_t err, dsn_message_t resp) mutable
                {
                    typename is_typed_rpc_callback<TCallback>::response_t response;
                    if (err == ERR_OK)
                    {
                        ::dsn::unmarshall(resp, response);
                    }
                    cb_fwd(err, std::move(response));
                    return true;
                }
                );
        }

        /*! TCallback(closure): void (error_code err, TResponse&& resp){}, or 
            bool (dsn_rpc_error_t err, dsn_message_t resp) {} */
        template<typename TRequest, typename TCallback>
        void call(
            dsn_channel_t ch,
            dsn_task_code_t code,
            TRequest&& req,
            TCallback&& callback,
            std::chrono::milliseconds timeout = std::chrono::milliseconds(0),
            int thread_hash = 0, ///< if thread_hash == 0 && partition_hash != 0, thread_hash is computed from partition_hash
            uint64_t partition_hash = 0,
            int reply_thread_hash = 0
            )
        {
            dsn_message_t msg = dsn_msg_create_request(code, 
                static_cast<int>(timeout.count()), thread_hash, partition_hash);
            ::dsn::marshall(msg, std::forward<TRequest>(req));
            call_cpp_task(ch, msg, reply_thread_hash, std::forward<TCallback>(callback));
        }

        /*! no callback */
        template<typename TRequest>
        void call_one_way_typed(
            dsn_channel_t ch,
            dsn_task_code_t code,
            const TRequest& req,
            int thread_hash = 0,///< if thread_hash == 0 && partition_hash != 0, thread_hash is computed from partition_hash
            uint64_t partition_hash = 0
            )
        {
            dsn_message_t msg = dsn_msg_create_request(code, 0, thread_hash, partition_hash);
            ::dsn::marshall(msg, req);
            dsn_rpc_call_one_way(ch, msg);
        }
        
        template<typename TResponse, typename TRequest>
        std::pair< ::dsn::error_code, TResponse> call_wait(
            dsn_channel_t ch,
            dsn_task_code_t code,
            const TRequest& req,
            std::chrono::milliseconds timeout = std::chrono::milliseconds(0),
            int thread_hash = 0,
            uint64_t partition_hash = 0
            )
        {
            dsn_message_t msg = dsn_msg_create_request(code, static_cast<int>(timeout.count()), thread_hash, partition_hash);
            ::dsn::marshall(msg, req);
            
            std::pair< ::dsn::error_code, TResponse> result;
            auto resp = dsn_rpc_call_wait(ch, msg);
            if (resp)
            {
                result.first = ::dsn::ERR_OK;
                ::dsn::unmarshall(resp, result.second);
                dsn_msg_destroy(resp);
            }
            else
            {
                result.first = ::dsn::ERR_TIMEOUT;
            }
            return result;
        }
        /*@}*/
    }

    namespace file
    {
        /*!
        @addtogroup file
        @{
        */

        /*! callback(error_code, int io_size) */
        template<typename TFunction, class Enable = void> struct is_aio_callback
        {
            constexpr static bool const value = false;
        };
        template<typename TFunction>
        struct is_aio_callback<TFunction, typename std::enable_if<function_traits<TFunction>::arity == 2>::type>
        {
            using inspect_t = function_traits<TFunction>;
            constexpr static bool const value =
                std::is_same<typename inspect_t::template arg_t<0>, dsn::error_code>::value
                && std::is_convertible<typename inspect_t::template arg_t<1>, uint64_t>::value;
        };

        /*! callback: void (error_code, int io_size) */
        template<typename TCallback>
        void read(
            dsn_handle_t fh,
            char* buffer,
            int count,
            uint64_t offset,
            dsn_task_code_t callback_code,
            TCallback&& callback,
            int hash = 0
            )
        {
            static_assert(is_aio_callback<TCallback>::value, "invalid aio callback");
            using callback_storage_t = typename std::remove_reference<TCallback>::type;
            dsn_file_read(fh, buffer, count, offset, 
                callback_code,
                cpp_task<callback_storage_t>::exec_aio,
                new cpp_task<callback_storage_t>(std::forward<TCallback>(callback)),
                hash
            );
        }

        /*! callback: void (error_code, int io_size) */
        template<typename TCallback>
        void write(
            dsn_handle_t fh,
            const char* buffer,
            int count,
            uint64_t offset,
            dsn_task_code_t callback_code,
            TCallback&& callback,
            int hash = 0
            )
        {
            static_assert(is_aio_callback<TCallback>::value, "invalid aio callback");
            using callback_storage_t = typename std::remove_reference<TCallback>::type;
            dsn_file_write(fh, buffer, count, offset,
                callback_code,
                cpp_task<callback_storage_t>::exec_aio,
                new cpp_task<callback_storage_t>(std::forward<TCallback>(callback)),
                hash
            );
        }

        /*! callback: void (error_code, int io_size) */
        template<typename TCallback>
        void write_vector(
            dsn_handle_t fh,
            const dsn_file_buffer_t* buffers,
            int buffer_count,
            uint64_t offset,
            dsn_task_code_t callback_code,
            TCallback&& callback,
            int hash = 0
            )
        {
            static_assert(is_aio_callback<TCallback>::value, "invalid aio callback");
            using callback_storage_t = typename std::remove_reference<TCallback>::type;
            dsn_file_write_vector(fh, buffers, buffer_count, offset,
                callback_code,
                cpp_task<callback_storage_t>::exec_aio,
                new cpp_task<callback_storage_t>(std::forward<TCallback>(callback)),
                hash
            );
        }

        /*! callback: void (error_code, int io_size) */
        template<typename TCallback>
        void copy_remote_files(
            ::dsn::rpc_address remote,
            const std::string& source_dir,
            const std::vector<std::string>& files,  // empty for all
            const std::string& dest_dir,
            bool overwrite,
            dsn_task_code_t callback_code,
            TCallback&& callback,
            int hash = 0
            )
        {
            static_assert(is_aio_callback<TCallback>::value, "invalid aio callback");
            using callback_storage_t = typename std::remove_reference<TCallback>::type;
            if (files.size() == 0)
            {
                dsn_file_copy_remote_directory(remote.c_addr(), source_dir.c_str(), dest_dir.c_str(),
                    overwrite,
                    callback_code,
                    cpp_task<callback_storage_t>::exec_aio,
                    new cpp_task<callback_storage_t>(std::forward<TCallback>(callback)),
                    hash
                );
            }
            else
            {
                const char** ptr = (const char**)alloca(sizeof(const char*) * (files.size() + 1));
                const char** ptr_base = ptr;
                for (auto& f : files)
                {
                    *ptr++ = f.c_str();
                }
                *ptr = nullptr;

                dsn_file_copy_remote_files(
                    remote.c_addr(), source_dir.c_str(), ptr_base,
                    dest_dir.c_str(), overwrite,
                    overwrite,
                    callback_code,
                    cpp_task<callback_storage_t>::exec_aio,
                    new cpp_task<callback_storage_t>(std::forward<TCallback>(callback)),
                    hash
                );
            }
        }

        /*! callback: void (error_code, int io_size) */
        template<typename TCallback>
        void copy_remote_directory(
            ::dsn::rpc_address remote,
            const std::string& source_dir,
            const std::string& dest_dir,
            bool overwrite,
            dsn_task_code_t callback_code,
            TCallback&& callback,
            int hash = 0
            )
        {
            copy_remote_files(
                remote, source_dir, {}, dest_dir, overwrite,
                callback_code, std::forward<TCallback>(callback), hash
                );
        }
        /*@}*/
    }
    // ------------- inline implementation ----------------

} // end namespace
