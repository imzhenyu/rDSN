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
#pragma once

# include <dsn/tool_api.h>
# include <unordered_set>
namespace dsn {
    namespace tools {

        class hpc_logger : public logging_provider
        {
        public:
            hpc_logger();
            virtual ~hpc_logger(void);

            virtual void dsn_logv(const char *file,
                const char *function,
                const int line,
                dsn_log_level_t log_level,
                const char* title,
                const char *fmt,
                va_list args
                );

            virtual void flush();

			static int throughput_count();
			bool stop_thread;

        private:
            std::string search(const char* keyword, int back_seconds, int back_start_seconds, std::unordered_set<int>& target_threads);
            
        private:
            int _per_thread_buffer_bytes;

			std::thread t_log;
			::dsn::utils::ex_lock_nr_spin m_lock;


			//::dsn::utils::notify_event re;
			//std::mutex m_lock;
			void log_thread();



        };
    }
}
