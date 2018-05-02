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
 *     cpp development library atop zion's c service api
 *
 * Revision history:
 *     Mar., 2015, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# pragma once

// the C++ (cpp) service API is based atop of C API
# include <dsn/service_api_c.h>

// task, thread, error code C++ wrappers
# include <dsn/cpp/auto_codes.h>

// rpc address C++ wrappers
# include <dsn/cpp/address.h>

// rpc stream reader and writer C++ wrappers
# include <dsn/cpp/rpc_stream.h>

// task, timer, rpc, aio, C++ wrappers
# include <dsn/cpp/service_api.h>

// rpc server C++ wrappers
# include <dsn/cpp/rpc_service.h>

// lock, wrlock, semaphore, event, etc., C++ wrappers
# include <dsn/cpp/zlocks.h>

// app model C++ wrappers
# include <dsn/cpp/service_app.h>

// configuration macro wrappers
# include <dsn/cpp/config_helper.h>

// performance test C++ wrappers
# include <dsn/cpp/perf_test_helper.h>
