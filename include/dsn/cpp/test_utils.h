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


# pragma once

# include <dsn/service_api_cpp.h>
# include <dsn/tool-api/task.h>
# include <dsn/tool-api/task_worker.h>
# include <dsn/tool-api/thread_profiler.h>
# include <gtest/gtest.h>
# include <iostream>

using namespace ::dsn;

#ifndef TEST_PORT_BEGIN
#define TEST_PORT_BEGIN 20201
#define TEST_PORT_END 20203
#endif

DEFINE_THREAD_POOL_CODE(THREAD_POOL_TEST_SERVER)

DEFINE_TASK_CODE_RPC(RPC_TEST_HASH, TASK_PRIORITY_COMMON, THREAD_POOL_TEST_SERVER)
DEFINE_TASK_CODE_RPC(RPC_TEST_HASH1, TASK_PRIORITY_COMMON, THREAD_POOL_TEST_SERVER)
DEFINE_TASK_CODE_RPC(RPC_TEST_HASH2, TASK_PRIORITY_COMMON, THREAD_POOL_TEST_SERVER)
DEFINE_TASK_CODE_RPC(RPC_TEST_HASH3, TASK_PRIORITY_COMMON, THREAD_POOL_TEST_SERVER)
DEFINE_TASK_CODE_RPC(RPC_TEST_HASH4, TASK_PRIORITY_COMMON, THREAD_POOL_TEST_SERVER)
DEFINE_TASK_CODE_RPC(RPC_TEST_STRING_COMMAND, TASK_PRIORITY_COMMON, THREAD_POOL_TEST_SERVER)

DEFINE_TASK_CODE_RPC(RPC_TEST_BIG_PAYLOAD, TASK_PRIORITY_COMMON, THREAD_POOL_TEST_SERVER)

DEFINE_TASK_CODE_AIO(LPC_AIO_TEST, TASK_PRIORITY_COMMON, THREAD_POOL_DEFAULT)
DEFINE_TASK_CODE(LPC_TEST_HASH, TASK_PRIORITY_COMMON, THREAD_POOL_DEFAULT)
DEFINE_TASK_CODE_AIO(LPC_AIO_TEST_READ, TASK_PRIORITY_COMMON, THREAD_POOL_DEFAULT)
DEFINE_TASK_CODE_AIO(LPC_AIO_TEST_WRITE, TASK_PRIORITY_COMMON, THREAD_POOL_DEFAULT)
DEFINE_TASK_CODE_AIO(LPC_AIO_TEST_NFS, TASK_PRIORITY_COMMON, THREAD_POOL_DEFAULT)

extern void run_all_unit_tests_when_necessary();

static const char* big_payload = "kljfdlksdjf;sdjfsdfj;sdj21jl4j2l4jl242;";

class test_client :
    public ::dsn::rpc_service<test_client>,
    public ::dsn::service_app    
{
public:
    test_client(dsn_gpid gpid)
        : ::dsn::rpc_service<test_client>("", 7), ::dsn::service_app(gpid)
    {
    }

    void on_rpc_test(const std::string& test_id, ::dsn::rpc_replier<std::string>& replier)
    {
        TPF_MARK("on_rpc_test.begin");
        std::string r = dsn::task::get_current_node_name();
        replier(std::move(r));
        TPF_MARK("on_rpc_test.end");
    }

    bool on_rpc_big_payload(dsn_message_t req)
    {
        int meta_info_big_payload_size;
        ::dsn::unmarshall(req, meta_info_big_payload_size);

        void* ptr;
        size_t sz;
        dsn_msg_read_next(req, &ptr, &sz);

        dassert(sz >= (size_t)meta_info_big_payload_size,
            "left message read buffer size must be greater than meta_info_big_payload_size");


        bool succ = (0 == memcmp((const void*)big_payload, ptr, meta_info_big_payload_size));

        dsn_msg_read_commit(req, (size_t)meta_info_big_payload_size);

        reply(req, succ);
        return true; // no async use of req, return req's ownership to the framework
    }

    bool on_rpc_string_test(dsn_message_t message) {
        std::string command;
        ::dsn::unmarshall(message, command);

        if (command == "expect_no_reply") {
            if (dsn::service_app::primary_address().port() == TEST_PORT_END) {
                ddebug("test_client_server, talk_with_reply: %s", dsn::service_app::primary_address().to_std_string().c_str());
                reply(message, dsn::service_app::primary_address().to_std_string());
            }
        }
        else if (command.substr(0, 5) == "echo ") {
            reply(message, command.substr(5));
        }
        else {
            derror("unknown command");
        }
        return true;
    }

    ::dsn::error_code start(int argc, char** argv)
    {
        // server
        if (argc == 1)
        {
            register_async_rpc_handler(RPC_TEST_HASH, "rpc.test.hash", &test_client::on_rpc_test);
            register_rpc_handler(RPC_TEST_BIG_PAYLOAD, "rpc.test.big.payload", &test_client::on_rpc_big_payload);

            //used for corrupted message test
            register_async_rpc_handler(RPC_TEST_HASH1, "rpc.test.hash1", &test_client::on_rpc_test);
            register_async_rpc_handler(RPC_TEST_HASH2, "rpc.test.hash2", &test_client::on_rpc_test);
            register_async_rpc_handler(RPC_TEST_HASH3, "rpc.test.hash3", &test_client::on_rpc_test);
            register_async_rpc_handler(RPC_TEST_HASH4, "rpc.test.hash4", &test_client::on_rpc_test);

            register_rpc_handler(RPC_TEST_STRING_COMMAND, "rpc.test.string.command", &test_client::on_rpc_string_test);
        }

        // client
        else
        {
            run_all_unit_tests_when_necessary();
        }
        
        return ::dsn::ERR_OK;
    }

    ::dsn::error_code stop(bool cleanup = false)
    {
        return ERR_OK;
    }
};
