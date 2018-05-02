/*
 * Description:
 *     define the base asynchonous io looper interface 
 *
 * Revision history:
 *     Jan., 2017, @imzhenyu (Zhenyu Guo)
 *     xxxx-xx-xx, author, fix bug about xxx
 */
#pragma once

# include <dsn/utility/ports.h>
# include <dsn/tool_api.h>
# include <dsn/utility/misc.h>
# include "concurrentqueue.h"
# include "heap_timer.h"
# include <dsn/cpp/safe_pointer.h>
# include <dsn/tool-api/rpc_client_matcher.h>
# include <dsn/plugin/looper.h>
# include <vector>

# ifndef _WIN32
# if defined(__APPLE__) || defined(__FreeBSD__)
    # include <sys/event.h>
    # define HAVE_KQUEUE 1
    #define FD_USER 4 
# elif defined(__linux__)
    # include <sys/eventfd.h>
    # include <sys/epoll.h>
    # define HAVE_EPOLL 1
# else
    #error Unsupported platform!
# endif
# endif

namespace dsn
{
    namespace tools
    {
        class io_looper;
        // find local thread io looper first if is_local = true
        // find remote io looper otherwise with round robin policy
        extern io_looper* get_io_looper(service_node* node, bool is_local);

        inline io_looper* get_io_looper(service_node* node, int index)
        {
            return (io_looper*)task_worker::remote(node, THREAD_POOL_IO, index);
        }

        typedef std::function<void()> lpf;
        typedef void (*lpf_deletor)(lpf*);

        //
        // io looper on completion queue
        // it is possible there are multiple loopers on the same io_queue
        //
        class io_looper : public task_worker
        {
        public:
            io_looper(task_worker_pool* pool, int index, task_worker* inner_provider);
            ~io_looper(void);

            // from task_worker
            virtual void start() override;
            virtual void stop() override;
            virtual void loop() override;

            // stop the loop, can continue the looper later by calling loop again
            // while the stop() above needs to have start() again
            void transient_stop();
            void reset();
            bool is_stopped() const { return _loop_stop; }
            int  events(int fd) const;

            // io service 
            error_code bind_io_handle(
                int fd,
                io_loop_callback cb, 
                void* ctx,
                int mask,
                bool edge_trigger = true
                );

            // cb is not necessary on windows
            void unbind_io_handle(int fd, int mask);

            // lpc service
            void add_timer(const char* name, lpc_callback cb, void* data, void* data2, int milliseconds);
            void add_lpc(const char* name, lpc_callback cb, void* data, void* data2 = nullptr);
            void add_lpc(const char* name, lpf* callback, lpf_deletor deletor);

            // utilities
            bool is_local() const { return native_tid() == ::dsn::utils::get_current_tid(); }
            safe_pointer_manager * safe_mgr() { return _safe_ptr_mgr; }
            rpc_client_matcher* matcher() { return &_client_matcher; }

            // used by event_loop_xxx
            void create_completion_queue();
            void close_completion_queue();

        private:
            bool handle_local_queue_once();
            void local_add_timer(lpTimeEvent* e);
            void exec_timer_tasks(uint64_t now_ms);
# ifndef _WIN32
            error_code local_bind_io_handle(
                int fd,
                io_loop_callback cb, 
                void* ctx,
                int mask,
                bool edge_trigger = true
                );

            // cb is not necessary on windows
            void local_unbind_io_handle(int fd, int mask);
# endif

# ifdef __linux__
            void lpc_handler(int mask); 
# endif

        private:
# ifdef _WIN32
            HANDLE           _sys_looper;
# else 
            int              _sys_looper; // kqueue or epoll fd
            struct fd_entry {
                int mask;
                io_loop_callback read_callback;
                io_loop_callback write_callback;
                io_loop_callback user_callback;
                void* read_context;
                void* write_context;
                void* user_context;

                fd_entry() { memset(this, 0, sizeof(*this)); }
            };
            std::vector<fd_entry> _fd_entries;
            friend void __lpc_callback(event_loop* lp, int fd, void* ctx, int events);
# endif
            bool             _loop_stop; // exit loop and can continue later with loop() again

            int              _lpc_fd;

            struct lpc_entry
            {
                const char* name;
                lpc_callback cb;
                void* data;
                void* data2;
            };
            moodycamel::ConcurrentQueue<lpc_entry > _lpc_queue;

            safe_pointer_manager *_safe_ptr_mgr;
            rpc_client_matcher    _client_matcher;
            lpTimerHeap          *_timers;
            friend void __add_timer_lpc(event_loop*, void* looper, void* heap_timer);
        };

        // only paired usage with io_looper as task_worker
        class io_looper_queue : public task_queue
        {
        private:
            io_looper *_looper;

        public:
            io_looper_queue(task_worker_pool* pool, int index, task_queue* inner_provider)
                : task_queue(pool, index, inner_provider) 
            {
                _looper = static_cast<io_looper*>(task_worker::remote(pool, index));
            }
            
            ~io_looper_queue() {}

            static void my_lpc_callback (event_loop*, void* tsk, void*)
            {
                auto t = (task*)tsk;
                t->exec_internal();
            }

            virtual void enqueue(task* task) override
            {
               _looper->add_lpc("io_looper_queue::enqueue", my_lpc_callback, task);
            }

            // not used
            task* dequeue(/*inout*/int& batch_size) override { dassert(false, "not reachable"); return nullptr; }
        };

        // --------------- inline implementation -------------------------
        inline uint64_t now_ms_steady()
        {
            return std::chrono::steady_clock::now().time_since_epoch().count() / 1000000;
        }
    }
}
