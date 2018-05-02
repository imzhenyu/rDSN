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


# include <gtest/gtest.h>
# include <dsn/service_api_cpp.h>
# include <dsn/cpp/test_utils.h>

struct test_context
{
    std::string result;
    ::dsn::zevent evt;
};

void on_lpc_test(void* p)
{
    auto c = (test_context*)p;
    c->result = ::dsn::task::get_current_worker()->name().c_str();
    c->evt.set();
}

void on_lpc_test2(void* p)
{
    auto c = (test_context*)p;
    c->evt.set();
}

TEST(core, lpc)
{
    test_context tc;
    dsn_task_lpc(
        LPC_TEST_HASH,
        on_lpc_test,
        &tc
    );
    tc.evt.wait();
    EXPECT_TRUE(tc.result.substr(0, tc.result.length() - 2) == "client.DEFAULT");
}
