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


/************************************************************
*   hpc_logger (High-Performance Computing logger)
*
*	Editor: Chang Lou (v-chlou@microsoft.com)
*
*
*	The structure of the logger is like the following graph.
*
*	For each thread:
*	-------------------------              -------------------------
*   |    new single log     |     ->       |                       |
*	-------------------------              | --------------------- |
*                                          |                       |
*                                          | --------------------- |
*                                          |        buffer         |
*                                          |     (per thread)      |
*                                          | --------------------- |
*                                          |                       |
*                                          | --------------------- |
*                                                    ...
*                                          | --------------------- |
*                                          |                       |
*                                          -------------------------
*                                                      |
*                                                      |
*                                                      |   when the buffer is full, push into global buffer list
*                                                      ----------------------------------------------------------
*                                                                                                               |
*                                            -------------------------------------------------------------      |
*                                             buf1  |  buf2  |  buf3  | ...                             <--------
*                                            -------------------------------------------------------------
*                                                           global buffer list
*                                                                   |
*                                                                   |
*                                                                   |   when the buffer list is full (the item number reaches MAX_BUFFER_IN_LIST)
*                                                                   |       notify daemon thread using std::condition_variable
*	                                                                V
*
*                                                            Daemon thread (hpc-logger)
*
*                                                                   ||
*                                                                   ===========>     hpc_log.x.txt
*
*	Some other facts:
*	1. The log file size is restricted, when line number exceeds 200000, a new log file will be established.
*	2. When exiting, the logger flushes, in other words, print out the retained log info in buffers of each thread and buffers in the buffer list.

************************************************************/

# include "hpc_logger.h"
# include <dsn/internal/singleton_store.h>
# include <dsn/cpp/utils.h>
# include <dsn/internal/command.h>
# include <cstdlib>
# include <sstream>
# include <fstream>
# include <iostream>

# include <thread>
# include <mutex>
# include <condition_variable>

#define MAX_BUFFER_IN_LIST 10
namespace dsn
{
	namespace tools
	{

		std::condition_variable_any  cv;

		struct tail_log_hdr
		{
			uint32_t log_break; // '\0'
			uint32_t magic;
			int32_t  length;
			uint64_t ts;
			tail_log_hdr* prev;

			bool is_valid() { return magic == 0xdeadbeef; }
		};

		struct __tail_log_info__
		{
			uint32_t magic;
			char*    buffer;
			char*    next_write_ptr;
			tail_log_hdr *last_hdr;
		};

		typedef ::dsn::utils::safe_singleton_store<int, struct __tail_log_info__*> tail_log_manager;

		static __thread struct __tail_log_info__* s_tail_log_info_ptr = nullptr;

		static void hpc_tail_logs_dumpper();

		//count throughput
		typedef ::dsn::utils::safe_singleton_store<int, int> throughput_manager;

		static __thread int s_throughput;

		static int _start_index;
		static int _index;
		static int _line;

		struct tail_log
		{
			int tid;
			__tail_log_info__* tail_log_info;
		};

		std::list<tail_log> tail_log_list;

		static void hpc_tail_log_push(int tid, __tail_log_info__* saved_log);

		//automatic log per thread
		static void hpc_indiv_tail_logs_dumpper();

		void hpc_logger::log_thread()
		{
			while (!stop_thread)
			{
				m_lock.lock();
				cv.wait(m_lock, []{return tail_log_list.size() >= MAX_BUFFER_IN_LIST; });
				hpc_indiv_tail_logs_dumpper();

				m_lock.unlock();
			}
		}

		int hpc_logger::throughput_count()
		{
			std::vector<int> threads;
			tail_log_manager::instance().get_all_keys(threads);

			int res = 0;
			for (auto& tid : threads)
			{
				int count = 0;
				if (!throughput_manager::instance().get(tid, count))
					continue;

				res += count;
			}

			return res;
		}

		hpc_logger::hpc_logger() :stop_thread(false)
		{

			_per_thread_buffer_bytes = config()->get_value<int>(
				"tools.hpc_logger",
				"per_thread_buffer_bytes",
				10 * 1024, // 10 MB by default
				"buffer size for per-thread logging"
				);



			t_log = std::thread(&hpc_logger::log_thread, this);
			s_throughput = 0;
			_start_index = 0;
			_index = 0;
			_line = 0;

			// check existing log files
			std::vector<std::string> sub_list;
			std::string path = "./";
			if (!dsn::utils::filesystem::get_subfiles(path, sub_list, false))
			{
				dassert(false, "Fail to get subfiles in %s.", path.c_str());
			}
			for (auto& fpath : sub_list)
			{
				auto&& name = dsn::utils::filesystem::get_file_name(fpath);
				if (name.length() <= 10 ||
					name.substr(0, 8) != "hpc_log.")
					continue;

				int index;
				if (1 != sscanf(name.c_str(), "hpc_log.%d.txt", &index))
					continue;

				if (index > _index)
					_index = index;

				if (_start_index == 0 || index < _start_index)
					_start_index = index;
			}
			sub_list.clear();

			if (_start_index == 0)
				_start_index = _index;

			_index++;
		}

		hpc_logger::~hpc_logger(void)
		{
			stop_thread = true;
			t_log.join();
		}

		void hpc_logger::flush()
		{
			//print retained log in the buffer list
			hpc_indiv_tail_logs_dumpper();

			//print retained log in the buffers of threads
			hpc_tail_logs_dumpper();

		}

		static void hpc_tail_logs_dumpper()
		{
			std::stringstream log;
			log << "hpc_log." << _index << ".txt";

			std::ofstream olog(log.str().c_str(), std::ofstream::out | std::ofstream::app);


			std::vector<int> threads;
			tail_log_manager::instance().get_all_keys(threads);

			for (auto& tid : threads)
			{
				__tail_log_info__* log;
				if (!tail_log_manager::instance().get(tid, log))
					continue;

				tail_log_hdr *hdr = log->last_hdr, *tmp = log->last_hdr;
				do
				{
					if (!tmp->is_valid())
						break;

					char* llog = (char*)(tmp)-tmp->length;
					olog << llog << std::endl;

					// try previous log
					tmp = tmp->prev;
				} while (tmp != nullptr && tmp != hdr);
				std::vector<int> tmpa;
				tail_log_manager::instance().get_all_keys(tmpa);
				tail_log_manager::instance().remove(tid);

			}

			olog.close();
		}


		void hpc_logger::dsn_logv(const char *file,
			const char *function,
			const int line,
			dsn_log_level_t log_level,
			const char* title,
			const char *fmt,
			va_list args
			)
		{
			s_throughput++;
			throughput_manager::instance().remove(::dsn::utils::get_current_tid());
			throughput_manager::instance().put(::dsn::utils::get_current_tid(), s_throughput);

			// init log buffer if necessary
			/*		if (s_tail_log_info_ptr->magic != 0xdeadbeef)
			{
			s_tail_log_info_ptr->buffer = (char*)malloc(_per_thread_buffer_bytes);
			s_tail_log_info_ptr->next_write_ptr = s_tail_log_info_ptr->buffer;
			s_tail_log_info_ptr->last_hdr = nullptr;
			memset(s_tail_log_info_ptr->buffer, '\0', _per_thread_buffer_bytes);

			tail_log_manager::instance().put(::dsn::utils::get_current_tid(), s_tail_log_info_ptr);
			s_tail_log_info_ptr->magic = 0xdeadbeef;
			}*/
			if (s_tail_log_info_ptr == nullptr)
			{
				s_tail_log_info_ptr = (__tail_log_info__*)malloc(sizeof(__tail_log_info__));
				s_tail_log_info_ptr->buffer = (char*)malloc(_per_thread_buffer_bytes);
				s_tail_log_info_ptr->next_write_ptr = s_tail_log_info_ptr->buffer;
				s_tail_log_info_ptr->last_hdr = nullptr;
				s_tail_log_info_ptr->magic = 0xdeadbeef;
				memset(s_tail_log_info_ptr->buffer, '\0', _per_thread_buffer_bytes);

				tail_log_manager::instance().remove(::dsn::utils::get_current_tid());
				tail_log_manager::instance().put(::dsn::utils::get_current_tid(), s_tail_log_info_ptr);

			}



			// get enough write space >= 1K
			if (s_tail_log_info_ptr->next_write_ptr + 1024 > s_tail_log_info_ptr->buffer + _per_thread_buffer_bytes)
			{
				//critical section begin
				m_lock.lock();

				hpc_tail_log_push(::dsn::utils::get_current_tid(), s_tail_log_info_ptr);

				if (tail_log_list.size() >= MAX_BUFFER_IN_LIST) cv.notify_one();
				m_lock.unlock();
				//critical section end

				__tail_log_info__* new_log = (__tail_log_info__*)malloc(sizeof(__tail_log_info__));

				s_tail_log_info_ptr = new_log;
				s_tail_log_info_ptr->buffer = (char*)malloc(_per_thread_buffer_bytes);
				s_tail_log_info_ptr->next_write_ptr = s_tail_log_info_ptr->buffer;
				s_tail_log_info_ptr->last_hdr = nullptr;
				s_tail_log_info_ptr->magic = 0xdeadbeef;
				memset(s_tail_log_info_ptr->buffer, '\0', _per_thread_buffer_bytes);

				tail_log_manager::instance().remove(::dsn::utils::get_current_tid());
				tail_log_manager::instance().put(::dsn::utils::get_current_tid(), s_tail_log_info_ptr);

			}
			char* ptr = s_tail_log_info_ptr->next_write_ptr;
			char* ptr0 = ptr; // remember it
			size_t capacity = static_cast<size_t>(s_tail_log_info_ptr->buffer + _per_thread_buffer_bytes - ptr);

			// print verbose log header    
			uint64_t ts = 0;
			int tid = ::dsn::utils::get_current_tid();
			if (::dsn::tools::is_engine_ready())
				ts = dsn_now_ns();
			char str[24];
			::dsn::utils::time_ms_to_string(ts / 1000000, str);
			auto wn = sprintf(ptr, "%s (%llu %04x) ", str, static_cast<long long unsigned int>(ts), tid);
			ptr += wn;
			capacity -= wn;

			task* t = task::get_current_task();
			if (t)
			{
				if (nullptr != task::get_current_worker())
				{
					wn = sprintf(ptr, "%6s.%7s%u.%016llx: ",
						task::get_current_node_name(),
						task::get_current_worker()->pool_spec().name.c_str(),
						task::get_current_worker()->index(),
						static_cast<long long unsigned int>(t->id())
						);
				}
				else
				{
					wn = sprintf(ptr, "%6s.%7s.%05d.%016llx: ",
						task::get_current_node_name(),
						"io-thrd",
						tid,
						static_cast<long long unsigned int>(t->id())
						);
				}
			}
			else
			{
				wn = sprintf(ptr, "%6s.%7s.%05d: ",
					task::get_current_node_name(),
					"io-thrd",
					tid
					);
			}

			ptr += wn;
			capacity -= wn;

			// print body
			wn = std::vsnprintf(ptr, capacity, fmt, args);
			ptr += wn;
			capacity -= wn;

			// set binary entry header on tail
			tail_log_hdr* hdr = (tail_log_hdr*)ptr;
			hdr->log_break = 0;
			hdr->length = 0;
			hdr->magic = 0xdeadbeef;
			hdr->ts = ts;
			hdr->length = static_cast<int>(ptr - ptr0);
			hdr->prev = s_tail_log_info_ptr->last_hdr;
			s_tail_log_info_ptr->last_hdr = hdr;

			ptr += sizeof(tail_log_hdr);
			capacity -= sizeof(tail_log_hdr);

			// set next write ptr
			s_tail_log_info_ptr->next_write_ptr = ptr;

			// dump critical logs on screen
			if (log_level >= LOG_LEVEL_WARNING)
			{
				std::cout << ptr0 << std::endl;
			}
		}


		//log operation

		static void hpc_tail_log_push(int tid, __tail_log_info__* saved_log)
		{

			tail_log* new_tail_log = (tail_log*)malloc(sizeof(tail_log));
			new_tail_log->tid = tid;
			new_tail_log->tail_log_info = saved_log;
			tail_log_list.push_back(*new_tail_log);
		}

		static void hpc_indiv_tail_logs_dumpper()
		{


			std::stringstream log;
			log << "hpc_log." << _index << ".txt";
			std::ofstream olog(log.str().c_str(), std::ofstream::out | std::ofstream::app);

			while (!tail_log_list.empty())
			{
				tail_log new_tail_log = tail_log_list.back();

				tail_log_hdr *hdr = (new_tail_log.tail_log_info)->last_hdr, *tmp = (new_tail_log.tail_log_info)->last_hdr;
				do
				{
					if (!tmp->is_valid())
						break;

					char* llog = (char*)(tmp)-tmp->length;
					olog << llog << std::endl;

					// try previous log
					tmp = tmp->prev;

				} while (tmp != nullptr && tmp != hdr);

				free((new_tail_log.tail_log_info)->buffer);
				free(new_tail_log.tail_log_info);

				tail_log_list.pop_back();

			}
			olog.close();

			if (++_line >= 200000)
			{
				_line = 0;
				_index++;
			}
		}
	}
}