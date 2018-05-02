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
 * Description:
 *     Rpc performance test
 *
 * Revision history:
 *     2016-01-05, Tianyi Wang, first version
 */
#include <gtest/gtest.h>
#include <dsn/cpp/test_utils.h>
#include <dsn/service_api_cpp.h>

struct c_context
{
    std::atomic<uint64_t> io_count;
    std::atomic<uint64_t> cb_flying_count;
    volatile bool exit;
    std::string req;
    std::vector<dsn_channel_t> chs;
    uint32_t block_size;
} s_rpc_testcase_c_context;

bool rpc_testcase_c_response_callback(
    dsn_rpc_error_t err, ///< usually, it is ok, or timeout, or busy
    dsn_message_t resp,   ///< incoming rpc response
    void* ctx);

void rpc_testcase_c_callback(int index)
{
    TPF_MARK("cb.begin");
    if (!s_rpc_testcase_c_context.exit)
    {
        s_rpc_testcase_c_context.io_count++;
        s_rpc_testcase_c_context.cb_flying_count++;

        TPF_MARK("cb.stat.done");

        // prepare msg
        dsn_message_t req = dsn_msg_create_request(RPC_TEST_HASH);

        TPF_MARK("cb.msg.created");

        void* ptr;
        size_t size;
        dsn_msg_write_next(req, &ptr, &size, s_rpc_testcase_c_context.block_size + sizeof(uint32_t));
        *(uint32_t*)ptr = s_rpc_testcase_c_context.block_size;
        memcpy((char*)ptr + sizeof(uint32_t), (const void*)s_rpc_testcase_c_context.req.c_str(),
            s_rpc_testcase_c_context.block_size
        );
        dsn_msg_write_commit(req, s_rpc_testcase_c_context.block_size + sizeof(uint32_t));

        TPF_MARK("cb.msg.commit");

        // rpc call
        dsn_rpc_call(
            s_rpc_testcase_c_context.chs[index], req, 
            rpc_testcase_c_response_callback,
            (void*)(uintptr_t)index
        );
    }
    TPF_MARK("cb.end");
}
 
bool rpc_testcase_c_response_callback(
    dsn_rpc_error_t err, ///< usually, it is ok, or timeout, or busy
    dsn_message_t resp,   ///< incoming rpc response
    void* ctx)
{
    TPF_INIT();
    if (RPC_ERR_OK == err)
        rpc_testcase_c_callback((int)(uintptr_t)ctx);
    s_rpc_testcase_c_context.cb_flying_count--;
    return true;
}

void rpc_testcase_c(rpc_address server, int cc, uint64_t block_size, size_t concurrency, int seconds = 10)
{
    s_rpc_testcase_c_context.req.resize(block_size, 'x');
    s_rpc_testcase_c_context.exit = false;
    s_rpc_testcase_c_context.cb_flying_count = 0;
    s_rpc_testcase_c_context.io_count = 0;
    s_rpc_testcase_c_context.block_size = block_size;

    for (int i = 0; i < cc; i++)
    {
        auto ch = dsn_rpc_channel_open(server.to_string());
        s_rpc_testcase_c_context.chs.emplace_back(ch);
    }
    
    // start
    auto tic = std::chrono::steady_clock::now();
    for (int j = 0; j < cc; j++)
    for (int i = 0; i < concurrency; i++)
    {
        rpc_testcase_c_callback(j);
    }

    // run for seconds
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    auto ioc = s_rpc_testcase_c_context.io_count.load();
    auto bytes = ioc * block_size;
    auto toc = std::chrono::steady_clock::now();

    std::cout
        << "block_size = " << block_size
        << ", channel.count = " << cc
        << ", channel.concurrency = " << concurrency
        << ", iops = " << (double)ioc / (double)std::chrono::duration_cast<std::chrono::microseconds>(toc - tic).count() * 1000000.0 << " #/s"
        << ", throughput = " << (double)bytes / std::chrono::duration_cast<std::chrono::microseconds>(toc - tic).count() << " mB/s"
        << ", avg_latency = " << (double)std::chrono::duration_cast<std::chrono::microseconds>(toc - tic).count() / (double)(ioc / (concurrency * cc)) << " us"
        << std::endl;

    // safe exit
    s_rpc_testcase_c_context.exit = true;

    while (s_rpc_testcase_c_context.cb_flying_count.load() > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    for (int i = 0; i < cc; i++)
    {
        dsn_rpc_channel_close(s_rpc_testcase_c_context.chs[i]);
    }
}

void rpc_testcase(rpc_address server, int cc, uint64_t block_size, size_t concurrency, int seconds = 10)
{
    std::atomic<uint64_t> io_count(0);
    std::atomic<uint64_t> cb_flying_count(0);
    volatile bool exit = false;
    std::function<void(int)> cb;    
    std::string req;
    req.resize(block_size, 'x');

    std::vector<dsn_channel_t> chs;
    for (int i = 0; i < cc; i++)
    {
        auto ch = dsn_rpc_channel_open(server.to_string());
        chs.emplace_back(ch);
    }

    cb = [&](int index)
    {
        TPF_MARK("cb.begin");
        if (!exit)
        {
            io_count++;            
            cb_flying_count++;

            rpc::call(
                chs[index],
                RPC_TEST_HASH,
                req,
                [idx = index, &cb, &cb_flying_count](error_code err, std::string&& result)
                {
                    TPF_INIT();
                    if (ERR_OK == err)
                        cb(idx);
                    cb_flying_count--;
                }
            );
        }
        TPF_MARK("cb.end");
    };

    // start
    auto tic = std::chrono::steady_clock::now();
    for (int j = 0; j < cc; j++)
    for (int i = 0; i < concurrency; i++)
    {
        cb(j);
    }

    // run for seconds
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    auto ioc = io_count.load();
    auto bytes = ioc * block_size;
    auto toc = std::chrono::steady_clock::now();

    std::cout
        << "block_size = " << block_size
        << ", channel.count = " << cc
        << ", channel.concurrency = " << concurrency
        << ", iops = " << (double)ioc / (double)std::chrono::duration_cast<std::chrono::microseconds>(toc - tic).count() * 1000000.0 << " #/s"
        << ", throughput = " << (double)bytes / std::chrono::duration_cast<std::chrono::microseconds>(toc - tic).count() << " mB/s"
        << ", avg_latency = " << (double)std::chrono::duration_cast<std::chrono::microseconds>(toc - tic).count() / (double)(ioc / (concurrency * cc)) << " us"
        << std::endl;

    // safe exit
    exit = true;

    while (cb_flying_count.load() > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    for (int i = 0; i < cc; i++)
    {
        dsn_rpc_channel_close(chs[i]);
    }
}

TEST(perf_core, rpc)
{
    rpc_address server("localhost", 20101);

    std::string test_server = dsn_config_get_value_string("apps.client", "test_server", "", 
        "rpc test server address, i.e., host:port"
        );
    if (test_server.length() > 0)
    {
        rpc_address addr(test_server.c_str());
        server.assign_ipv4(addr.ip(), addr.port());
    }

    for (auto channel_count : { 1, 2, 10, 50, 100, 200, 500  })
        for (auto concurrency : { 1, 2, 4,10,50 })
            for (auto blk_size_bytes : { 1, 256, 512, 1024, 4 * 1024 })
                rpc_testcase(server, channel_count, blk_size_bytes, concurrency);
}

TEST(perf_core, interlock)
{
    uint64_t count = 10000000;
    uint64_t array_size = 1;
    int* ints = new int[array_size];

    auto start = std::chrono::steady_clock::now();
    uint64_t sum = 0;
    for (uint64_t i = 0; i < count; i++)
    {
        sum += ints[i % array_size];
    }
    auto end = std::chrono::steady_clock::now();
    auto duration_us = (end - start).count();

    start = std::chrono::steady_clock::now();
    std::atomic<uint64_t> sum2(0);
    for (uint64_t i = 0; i < count; i++)
    {
        sum2.fetch_add((unsigned long long)ints[i % array_size]);
    }
    end = std::chrono::steady_clock::now();
    auto duration2_us = (end - start).count();

    delete[] ints;

    std::cout
        << "run sum += (int) for " << count << " times, (simple +) versus (atomic::fetch_add) = "
        << duration_us << " vs " << duration2_us
        << std::endl;
}

TEST(perf_core, rpc_4k)
{
    rpc_address server("localhost", 20101);

    std::string test_server = dsn_config_get_value_string("apps.client", "test_server", "",
        "rpc test server address, i.e., host:port"
    );
    if (test_server.length() > 0)
    {
        rpc_address addr(test_server.c_str());
        server.assign_ipv4(addr.ip(), addr.port());
    }

    for (auto concurrency : { 1 })
        for (auto blk_size_bytes : { 4 * 1024 })
            rpc_testcase(server, 1, blk_size_bytes, concurrency, 60);

}


TEST(perf_core, rpc_c)
{
    rpc_address server("localhost", 20101);

    std::string test_server = dsn_config_get_value_string("apps.client", "test_server", "",
        "rpc test server address, i.e., host:port"
    );
    if (test_server.length() > 0)
    {
        rpc_address addr(test_server.c_str());
        server.assign_ipv4(addr.ip(), addr.port());
    }

    for (auto channel_count : { 1, 2, 10, 24, 50, 100, 200, 500 })
        for (auto concurrency : { 1, 2, 4,10,50 })
            for (auto blk_size_bytes : { 1, 256, 512, 1024, 4 * 1024 })
                rpc_testcase_c(server, channel_count, blk_size_bytes, concurrency);
}


void lpc_testcase(size_t concurrency)
{
    std::atomic<uint64_t> io_count(0);
    std::atomic<uint64_t> cb_flying_count(0);
    volatile bool exit = false;
    std::function<void(int)> cb;

    cb = [&](int index)
    {
        TPF_MARK("cb.begin");
        if (!exit)
        {
            io_count++;
            cb_flying_count++;

            tasking::enqueue(
                LPC_TEST_HASH,
                [idx = index, &cb, &cb_flying_count]()
                {
                    TPF_INIT();
                    cb(idx);
                    cb_flying_count--;
                }, 
                index
            );
        }
        TPF_MARK("cb.end");
    };

    // start
    auto tic = std::chrono::steady_clock::now();
    for (int i = 0; i < concurrency; i++)
    {
        cb(i);
    }

    // run for seconds
    std::this_thread::sleep_for(std::chrono::seconds(10));
    auto ioc = io_count.load();
    auto toc = std::chrono::steady_clock::now();

    std::cout
        << "concurrency = " << concurrency
        << ", iops = " << (double)ioc / (double)std::chrono::duration_cast<std::chrono::microseconds>(toc - tic).count() * 1000000.0 << " #/s"
        << ", avg_latency = " << (double)std::chrono::duration_cast<std::chrono::microseconds>(toc - tic).count() / (double)(ioc / concurrency) << " us"
        << std::endl;

    // safe exit
    exit = true;

    while (cb_flying_count.load() > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

TEST(perf_core, lpc)
{
    for (auto concurrency : { 1, 2, 4,10,50,100,200 })
        lpc_testcase(concurrency);
}
