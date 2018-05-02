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
 *     Rpc performance test
 *
 * Revision history:
 *     2016-01-05, Tianyi Wang, first version
 */

#include <gtest/gtest.h>
#include <dsn/service_api_cpp.h>
#include <dsn/tool_api.h>
#include <dsn/utility/priority_queue.h>
#include <dsn/cpp/test_utils.h>
#include <mutex>
#include <condition_variable>
#include <sstream>

//worker = 1
DEFINE_THREAD_POOL_CODE(THREAD_POOL_TEST_TASK_QUEUE_1);
//worker = 1
DEFINE_THREAD_POOL_CODE(THREAD_POOL_TEST_TASK_QUEUE_2);
DEFINE_TASK_CODE(LPC_TEST_TASK_QUEUE_1, TASK_PRIORITY_HIGH, THREAD_POOL_TEST_TASK_QUEUE_1)
DEFINE_TASK_CODE(LPC_TEST_TASK_QUEUE_2, TASK_PRIORITY_HIGH, THREAD_POOL_TEST_TASK_QUEUE_2)

struct auto_timer 
{
    std::string prefix;
    uint64_t delivery;
    decltype(std::chrono::steady_clock::now()) start_time;
    ::dsn::zsemaphore sema;
    int waited_task_count;

    auto_timer(const std::string& pre, ::dsn::task_code code, uint64_t delivery) 
        : delivery(delivery)
    {
        waited_task_count = 0;
        std::stringstream ss;
        ss << pre << " (worker# = " << ::dsn::tools::spec().threadpool_specs[::dsn::task_spec::get(code)->pool_code].worker_count << ")";
        start_time = std::chrono::steady_clock::now();
    }

    void reset_start()
    {
        start_time = std::chrono::steady_clock::now();
    }

    void add_task()
    {
        waited_task_count++;
    }

    void signal()
    {
        sema.signal();
    }

    ~auto_timer()
    {
        for (int i = 0; i < waited_task_count; i++)
        {
            sema.wait();
        }
        auto end_time = std::chrono::steady_clock::now();
        std::cout << prefix << "throughput = " << delivery * 1000 * 1000 / std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count() << std::endl;
    }
};

void empty_cb(void*)
{
    
}
struct self_iterate_context
{
    std::mutex mut;
    std::condition_variable cv;
    bool done = false;
    std::vector<task_c*> tsks;
    std::vector<task_c*>::iterator it;
};
void iterate_over_preallocated_tasks(void* ctx)
{
    auto context = reinterpret_cast<self_iterate_context*>(ctx);
    if (context->it != context->tsks.end())
    {
        auto it_save = context->it;
        ++context->it;
        (*it_save)->enqueue();
    }
    else
    {
        {
            std::lock_guard<std::mutex> _(context->mut);
            context->done = true;
        }
        context->cv.notify_one();
    }
}

void external_flooding(const int enqueue_time)
{
    auto_timer duration("inter-thread flooding test:", LPC_TEST_TASK_QUEUE_1, enqueue_time);

    for (int i = 0; i < enqueue_time; i++)
    {
        duration.add_task();
        ::dsn::tasking::enqueue(LPC_TEST_TASK_QUEUE_1, [&] {duration.signal(); });
    }
}

void signal_cb(void* tm)
{
    ((auto_timer*)tm)->signal();
}

void signal_cb2(void* tm)
{
    ((zevent*)tm)->set();
}

void external_flooding2(const int enqueue_time)
{
    auto_timer timer("inter-thread blocking test(no task-create):", LPC_TEST_TASK_QUEUE_1, enqueue_time);

    std::vector<task*> tasks;
    for (int i = 0; i < enqueue_time; i++)
    {
        timer.add_task();
        tasks.push_back(new task_c(LPC_TEST_TASK_QUEUE_1, signal_cb, &timer, 0));
    }

    timer.reset_start();
    {
        for (auto tsk : tasks)
        {
            tsk->enqueue();
        }
    }
 }

void self_flooding(const int enqueue_time)
{
    auto_timer timer("self-flooding test:", LPC_TEST_TASK_QUEUE_1, enqueue_time);

    std::vector<task*> tasks;
    for (int i = 0; i < enqueue_time; i++)
    {
        timer.add_task();
        tasks.push_back(new task_c(LPC_TEST_TASK_QUEUE_1, signal_cb, &timer, 0));
    }

    tasking::enqueue(LPC_TEST_TASK_QUEUE_1, [&]()
    {
        for (auto tsk : tasks)
        {
            tsk->enqueue();
        }

        timer.reset_start();
    });
}

void external_blocking(const int enqueue_time)
{
    auto_timer timer("inter-thread blocking test:", LPC_TEST_TASK_QUEUE_1, enqueue_time);
    zevent evt;

    std::vector<task*> tasks;
    for (int i = 0; i < enqueue_time; i++)
    {
        tasks.push_back(new task_c(LPC_TEST_TASK_QUEUE_1, signal_cb2, &evt, 0));
    }

    for (auto tsk : tasks)
    {
        tsk->enqueue();
        evt.wait();
    }
}

TEST(perf_core, task_queue)
{
    const int enqueue_time = 10000000;
    external_flooding(enqueue_time);
    external_flooding2(enqueue_time);
    self_flooding(enqueue_time);
    external_blocking(enqueue_time / 10);
}