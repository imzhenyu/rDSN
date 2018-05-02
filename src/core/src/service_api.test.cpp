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
 *     Unit-test for service_api.
 *
 * Revision history:
 *     Nov., 2015, @shengofsun (Weijie Sun), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

#include <dsn/cpp/service_api.h>
#include <gtest/gtest.h>
#include <functional>
#include <chrono>
#include <dsn/cpp/test_utils.h>

DEFINE_TASK_CODE(LPC_TEST_service_api, TASK_PRIORITY_COMMON, THREAD_POOL_TEST_SERVER)
using namespace dsn;

int global_value;
class test_service_api {
public:
    std::string str;
    int number;

public:
    test_service_api(): str("before called"), number(0) {
        global_value = 0;
    }
    void callback_function1()
    {
        str = "after called";
        ++global_value;
    }

    void callback_function2() {
        number = 0;
        for (int i=0; i<1000; ++i)
            number+=i;
        ++global_value;
    }

    void callback_function3() { ++global_value; }
};

class test_channel
{
public:
    test_channel(const char* host, uint16_t port)
    {
        rpc_address addr(host, port);
        _ch = dsn_rpc_channel_open(addr.to_string());
    }

    ~test_channel()
    {
        if (_ch != nullptr)
        {
            dsn_rpc_channel_close(_ch);
        }
    }

    dsn_channel_t get() { return _ch; }

private:
    dsn_channel_t _ch;
};

struct task_context
{
    error_code err;
    int io_size;
    ::dsn::message_ex* reply;
    ::dsn::zevent evt;
};

TEST(dev_cpp, service_api_task)
{
    /* normal lpc*/
    test_service_api *cl = new test_service_api();
    task_context tc;
    
    tasking::enqueue(LPC_TEST_service_api, [cl, &tc] {cl->callback_function1(); tc.evt.set(); });
    tc.evt.wait();
    EXPECT_TRUE(cl->str == "after called");
    delete cl;
}

TEST(dev_cpp, service_api_rpc)
{
    test_channel addr("localhost", 20101);
    test_channel addr2("localhost", TEST_PORT_END);
    test_channel addr3("localhost", 32767);

    rpc::call_one_way_typed(addr.get(), RPC_TEST_STRING_COMMAND, std::string("expect_no_reply"), 0);
    std::string command = "echo hello world";
    task_context tc[2];

    rpc::call(
        addr3.get(),
        RPC_TEST_STRING_COMMAND,
        command,
        [&](error_code ec, std::string&& resp)
        {
            if (ERR_OK == ec)
                EXPECT_TRUE(command.substr(5) == resp);
            tc[0].evt.set();
        }
    );
   
    rpc::call(
        addr2.get(),
        RPC_TEST_STRING_COMMAND,
        command,
        [&](dsn_rpc_error_t err, dsn_message_t resp)
        {
            EXPECT_TRUE(err == ERR_OK);
            tc[1].evt.set();
            return true;
        }
    );

    tc[0].evt.wait();
    tc[1].evt.wait();
}
