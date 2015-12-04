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

# include <iostream>
# include "gtest/gtest.h"
# include <dsn/service_api_cpp.h>

# ifndef _WIN32
# include <sys/types.h>
# include <signal.h>
# endif

int g_test_count = 0;
int g_test_ret = 0;

extern void dsn_core_init();
extern void lock_test_init();

class test_client : public ::dsn::service_app
{
public:
    ::dsn::error_code start(int argc, char** argv)
    {
        testing::InitGoogleTest(&argc, argv);
        g_test_ret = RUN_ALL_TESTS();
        g_test_count = 1;
/*
        // exit without any destruction
# if defined(_WIN32)
        ::ExitProcess(0);
# else
        kill(getpid(), SIGKILL);
# endif
*/
        return ::dsn::ERR_OK;
    }

    void stop(bool cleanup = false)
    {

    }
};

GTEST_API_ int main(int argc, char **argv) 
{
    // register all possible services
    dsn::register_app<test_client>("test");
    dsn_core_init();
    lock_test_init();
    
    // specify what services and tools will run in config file, then run
    dsn_run_config("config-test.ini", false);
    while (g_test_count == 0)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return g_test_ret;
}
