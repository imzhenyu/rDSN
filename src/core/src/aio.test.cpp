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


# include <dsn/service_api_cpp.h>
# include <gtest/gtest.h>
# include <dsn/cpp/test_utils.h>
# include <dsn/cpp/zlocks.h>

using namespace ::dsn;

# if !defined(__APPLE__)

struct io_context
{
    error_code err;
    int size;
    zevent evt;
};

TEST(core, aio)
{
    // if in dsn_mimic_app() and disk_io_mode == IOE_PER_QUEUE
    if (task::get_current_disk() == nullptr) return;

    const char* buffer = "hello, world";
    int len = (int)strlen(buffer);

    // write
    auto fp = dsn_file_open("tmp", O_RDWR | O_CREAT | O_BINARY, 0666);
    
    io_context tasks[200];
    uint64_t offset = 0;

    for (int i = 0; i < 200; i++)
    {
        EXPECT_FALSE(tasks[i].evt.wait(1));
    }

    // new write
    for (int i = 0; i < 100; i++)
    {
        ::dsn::file::write(fp, buffer, len, offset, LPC_AIO_TEST,
            [&tasks, i](error_code, int) { tasks[i].evt.set(); });
        offset += len;
    }

    for (int i = 0; i < 100; i++)
    {
        tasks[i].evt.wait();
    }

    for (int i = 0; i < 200; i++)
    {
        EXPECT_FALSE(tasks[i].evt.wait(1));
    }

    // overwrite 
    offset = 0;
    for (int i = 0; i < 100; i++)
    {
        ::dsn::file::write(fp, buffer, len, offset, LPC_AIO_TEST, 
            [&tasks, i](error_code, int sz) { tasks[i].size = sz; tasks[i].evt.set(); });
        offset += len;
    }

    for (int i = 0; i < 100; i++)
    {
        tasks[i].evt.wait();
        EXPECT_TRUE(tasks[i].size == len);
    }

    for (int i = 0; i < 200; i++)
    {
        EXPECT_FALSE(tasks[i].evt.wait(1));
    }

    // vector write
    std::unique_ptr<dsn_file_buffer_t[]> buffers(new dsn_file_buffer_t[100]);
    for (int i = 0; i < 10; i ++)
    {
        buffers[i].buffer = reinterpret_cast<void*>(const_cast<char*>(buffer));
        buffers[i].size = len;
    }

    for (int i = 0; i < 10; i ++)
    {
        ::dsn::file::write_vector(fp, buffers.get(), 10, offset, LPC_AIO_TEST,
            [&tasks, i](error_code, int sz) { tasks[i].size = sz; tasks[i].evt.set(); });
        offset += 10 * len;
    }

    for (int i = 0; i < 10; i++)
    {
        tasks[i].evt.wait();
        EXPECT_EQ(tasks[i].size, 10 * len);
    }

    for (int i = 0; i < 200; i++)
    {
        EXPECT_FALSE(tasks[i].evt.wait(1));
    }

    auto err = dsn_file_close(fp);
    EXPECT_TRUE(err == ERR_OK);

    // read
    char* buffer2 = (char*)alloca((size_t)len);
    fp = dsn_file_open("tmp", O_RDONLY | O_BINARY, 0);

    // concurrent read
    offset = 0;
    for (int i = 0; i < 100; i++)
    {
        ::dsn::file::read(fp, buffer2, len, offset, LPC_AIO_TEST,
            [&tasks, i](error_code, int sz) { tasks[i].size = sz; tasks[i].evt.set(); });
        offset += len;
    }

    for (int i = 0; i < 100; i++)
    {
        tasks[i].evt.wait();
        EXPECT_EQ(tasks[i].size, len);
    }

    for (int i = 0; i < 200; i++)
    {
        EXPECT_FALSE(tasks[i].evt.wait(1));
    }

    // sequential read
    offset = 0;
    for (int i = 0; i < 200; i++)
    {
        buffer2[0] = 'x';
        ::dsn::file::read(fp, buffer2, len, offset, LPC_AIO_TEST, 
            [&tasks, i](error_code, int sz) { tasks[i].size = sz; tasks[i].evt.set(); });

        offset += len;
        tasks[i].evt.wait();

        EXPECT_FALSE(tasks[i].evt.wait(1));
        EXPECT_TRUE(tasks[i].size == (size_t)len);
        EXPECT_TRUE(memcmp(buffer, buffer2, len) == 0);
    }

    err = dsn_file_close(fp);
    EXPECT_TRUE(err == ERR_OK);

    utils::filesystem::remove_path("tmp");
}

TEST(core, aio_share)
{
    // if in dsn_mimic_app() and disk_io_mode == IOE_PER_QUEUE
    if (task::get_current_disk() == nullptr) return;

    auto fp = dsn_file_open("tmp", O_WRONLY | O_CREAT | O_BINARY, 0666);
    EXPECT_TRUE(fp != nullptr);

    auto fp2 = dsn_file_open("tmp", O_RDONLY | O_BINARY, 0);
    EXPECT_TRUE(fp2 != nullptr);

    dsn_file_close(fp);
    dsn_file_close(fp2);

    utils::filesystem::remove_path("tmp");
}

TEST(core, aio_operation_failed)
{
    //auto fp = dsn_file_open("tmp_test_file", O_WRONLY, 0600);
    //EXPECT_TRUE(fp == nullptr);

    ::dsn::error_code err;
    size_t count;
    zevent evt;
    auto io_callback = [&](::dsn::error_code e, int n) {
        err = e;
        count = n;
        evt.set();
    };

    auto fp = dsn_file_open("tmp_test_file", O_WRONLY|O_CREAT|O_BINARY, 0666);
    EXPECT_TRUE(fp != nullptr);
    char buffer[512];
    const char* str = "hello file";
    
    ::dsn::file::write(fp, str, strlen(str), 0, LPC_AIO_TEST, io_callback, 0);
    evt.wait();
    EXPECT_TRUE(err == ERR_OK && count==strlen(str));

    ::dsn::file::read(fp, buffer, 512, 0, LPC_AIO_TEST,  io_callback, 0);
    evt.wait();
    EXPECT_TRUE(err == ERR_FILE_OPERATION_FAILED);

    auto fp2 = dsn_file_open("tmp_test_file", O_RDONLY|O_BINARY, 0);
    EXPECT_TRUE(fp2 != nullptr);

    ::dsn::file::read(fp2, buffer, 512, 0, LPC_AIO_TEST, io_callback, 0);
    evt.wait();
    EXPECT_TRUE(err == ERR_OK && count==strlen(str));

    ::dsn::file::read(fp2, buffer, 5, 0, LPC_AIO_TEST, io_callback, 0);
    evt.wait();
    EXPECT_TRUE(err == ERR_OK && count==5);

    ::dsn::file::read(fp2, buffer, 512, 100, LPC_AIO_TEST, io_callback, 0);
    evt.wait();
    ddebug("error code: %s", err.to_string());
    dsn_file_close(fp);
    dsn_file_close(fp2);

    EXPECT_TRUE(utils::filesystem::remove_path("tmp_test_file"));
}

# endif
