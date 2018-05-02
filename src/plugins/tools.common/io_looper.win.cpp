/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 * 
 * -=- Robust Distributed System Nucleus (rdsn) -=- 
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

# ifdef _WIN32

# include "io_looper.h"
# define NON_IO_TASK_NOTIFICATION_KEY 2

namespace dsn
{
    namespace tools
    {
        error_code io_looper::bind_io_handle(
            int fd,
            io_loop_callback* cb,
            int mask,
            bool edge_trigger
            )
        {
            mask;
            edge_trigger;
            if (NULL == ::CreateIoCompletionPort((HANDLE)fd, _sys_looper, (ULONG_PTR)cb, 0))
            {
                derror("bind io handler to completion port failed, err = %d", ::GetLastError());
                return ERR_BIND_IOCP_FAILED;
            }
            else
                return ERR_OK;
        }

        void io_looper::unbind_io_handle(int fd, int mask, io_loop_callback* cb)
        {
            // nothing to do
        }

        void io_looper::add_lpc(const char* name, lpc_callback cb, void* data, void* data2)
        {
            dinfo("io_looper add lpc %s, data = %p, data2 = %p, cb = %p", name, data, data2, cb);

            _lpc_queue.enqueue(lpc_entry{ name, cb, data, data2 });
            if (!::PostQueuedCompletionStatus(_sys_looper, 0, NON_IO_TASK_NOTIFICATION_KEY, NULL))
            {
                dassert(false, "PostQueuedCompletionStatus failed, err = %d", ::GetLastError());
            }
        }

        void io_looper::create_completion_queue()
        {
            _sys_looper = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        }

        void io_looper::close_completion_queue()
        {
            if (_sys_looper != 0)
            {
                ::CloseHandle(_sys_looper);
                _sys_looper = 0;
            }
        }

        void io_looper::loop()
        {
            DWORD io_size;
            uintptr_t completion_key;
            LPOVERLAPPED lolp;
            DWORD error;
            uint64_t last_timer_ms = now_ms_steady();
            uint64_t now_ms = last_timer_ms;

            _loop_stop = false; 
            while (!_loop_stop)
            {
                uint32_t next_timer_delay_ms = (uint32_t)lpTimerHeapTopNextMilliseconds(_timers, now_ms);
                BOOL r = ::GetQueuedCompletionStatus(
                    _sys_looper,
                    &io_size,
                    &completion_key,
                    &lolp,
                    next_timer_delay_ms
                    );

                now_ms = now_ms_steady();
                if (now_ms >= last_timer_ms + next_timer_delay_ms)
                {
                    exec_timer_tasks(now_ms);
                    last_timer_ms = now_ms;
                }

                if (r)
                {
                    error = ERROR_SUCCESS;
                }

                // failed or timeout
                else
                {
                    error = ::GetLastError();
                    if (error == ERROR_ABANDONED_WAIT_0)
                    {
                        derror("completion port loop exits");
                        break;
                    }
                    
                    if (lolp == nullptr)
                        continue;
                }

                if (NON_IO_TASK_NOTIFICATION_KEY == completion_key)
                {
                    bool r = handle_local_queue_once();
                    dassert(r, "io_looper dequeue lpc failed");
                }
                else
                {
                    io_loop_callback* cb = (io_loop_callback*)completion_key;
                    (*cb)((int)error, io_size, lolp);
                }
            }
        }
    }
}

# endif
