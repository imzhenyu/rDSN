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
 *     helpers for easier task programing atop C api
 *
 * Revision history:
 *     Sep., 2015, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# pragma once

# include <dsn/service_api_c.h>
# include <dsn/cpp/auto_codes.h>
# include <dsn/utility/misc.h>
# include <dsn/cpp/rpc_stream.h>
# include <dsn/cpp/zlocks.h>
# include <dsn/cpp/callocator.h>
# include <dsn/cpp/optional.h>
# include <set>
# include <map>
# include <thread>

namespace dsn 
{
    template<typename THandler>
    class cpp_task : 
        public transient_object
    {
    public:
        explicit cpp_task(THandler&& h) : _handler(std::move(h))
        {
        }

        explicit cpp_task(const THandler& h) : _handler(h)
        {
        }

        static void exec(void* task)
        {
            auto t = static_cast<cpp_task*>(task);
            dbg_dassert(t->_handler.is_some(), "_handler is missing");
            t->_handler.unwrap()();
            t->_handler.reset();
            delete t;
        }
        
        static bool exec_rpc_response(dsn_rpc_error_t err, dsn_message_t resp, void* task)
        {
            auto t = static_cast<cpp_task*>(task);
            dbg_dassert(t->_handler.is_some(), "_handler is missing");
            bool r = t->_handler.unwrap()(err, resp);
            t->_handler.reset();
            delete t;
            return r;
        }

        static void exec_aio(dsn_error_t err, size_t sz, void* task)
        {
            auto t = static_cast<cpp_task*>(task);
            dbg_dassert(t->_handler.is_some(), "_handler is missing");
            t->_handler.unwrap()(err, sz);
            t->_handler.reset();
            delete t;
        }
            
    private:
        dsn::optional<THandler>    _handler;
    };

    template<typename THandler>
    class timer_safe_task
    {
    public:
        explicit timer_safe_task(THandler&& h) : _handler(std::move(h))
        {
        }

        explicit timer_safe_task(const THandler& h) : _handler(h)
        {
        }

        static void exec_timer(void* task)
        {
            auto t = static_cast<timer_safe_task*>(task);
            dbg_dassert(t->_handler.is_some(), "_handler is missing");
            t->_handler.unwrap()();
        }

        static void delete_timer(void* task)
        {
            auto t = static_cast<timer_safe_task*>(task);
            delete t;
        }
        
    private:
        dsn::optional<THandler>    _handler;
    };

    // ------- inlined implementation ----------
}
