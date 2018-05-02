/*
 * Description:
 *     io looper implementation
 *
 * Revision history:
 *     Jan., 2017, @imzhenyu (Zhenyu Guo)
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# include "io_looper.h"

namespace dsn
{
    namespace tools
    {
        io_looper::io_looper(task_worker_pool* pool, int index, task_worker* inner_provider)
            : task_worker(pool, index, inner_provider)
        {
            _sys_looper = 0;
            _lpc_fd = -1;
            _safe_ptr_mgr = new safe_pointer_manager(102400, "io_looper");
            _timers = lpCreateTimerHeap(1024000);
            _loop_stop = false;
        }

        io_looper::~io_looper(void)
        {
            close_completion_queue();
            delete _safe_ptr_mgr;
            lpDestroyTimerHeap(_timers);
        }

        void io_looper::start()
        {
            create_completion_queue();
            task_worker::start();
        }

        void io_looper::stop()
        {
            close_completion_queue();
            task_worker::stop();
        }

        void io_looper::transient_stop() 
        {
            _loop_stop = true;
        }

        void io_looper::reset() 
        {
            lpDestroyTimerHeap(_timers);
            close_completion_queue();

            create_completion_queue();
            _timers = lpCreateTimerHeap(1024000);
        }

        static void io_looper_lpc_cb(event_loop*, void* data, void* d2)
        {
            auto cb = (lpf*)data;
            (*cb)();

            auto deletor = (lpf_deletor)d2;
            if (deletor)
                (*deletor)(cb);
        }

        void io_looper::add_lpc(const char* name, lpf* callback, lpf_deletor deletor)
        {
            add_lpc(name, io_looper_lpc_cb, (void*)callback, (void*)deletor);
        }

        bool io_looper::handle_local_queue_once()
        {
            lpc_entry cb;
            if (_lpc_queue.try_dequeue(cb))
            {
                dinfo("io_looper exec lpc %s, data = %p, data2 = %p", cb.name, cb.data, cb.data2);
                (*cb.cb)((event_loop*)this, cb.data, cb.data2);
                return true;
            }
            else
            {
                return false;
            }
        }

        void io_looper::exec_timer_tasks(uint64_t now_ms)
        {
            lpProcessTimers(_timers, now_ms);
        }

        void __add_timer_lpc(event_loop*, void* looper, void* heap_timer)
        {
            ((io_looper*)looper)->local_add_timer((lpTimeEvent*)heap_timer);
        }

        void io_looper::local_add_timer(lpTimeEvent* e)
        {
            dbg_dassert(is_local(), "only accessible from local looper thread");
            lpAddToTimerHeap(_timers, e);
        }

        void io_looper::add_timer(const char* name, lpc_callback cb, void* data, void* data2, int milliseconds)
        {
            lpTimeEvent* e = lpCreateHeapTimer(name, (lpTimeProc)cb, (event_loop*)this, data, data2, now_ms_steady() + milliseconds);
            if (is_local())
            {
                local_add_timer(e);
            }
            else
            {
                add_lpc("__add_timer_lpc", __add_timer_lpc, this, e);
            }
        }

        class io_looper_holder : public utils::singleton < io_looper_holder >
        {
        private:
            class per_node_loopers
            {
            public:
                per_node_loopers(service_node* node)
                {
                    auto& sp = tools::spec().threadpool_specs[THREAD_POOL_IO];
                    for (int i = 0; i < sp.worker_count; i++)
                    {
                        auto looper = (io_looper*)task_worker::remote(node, THREAD_POOL_IO, i);
                        _loopers.push_back(looper);
                    }
                    _next = 0;
                }

                io_looper* fetch_next()
                {
                    return _loopers[++_next % _loopers.size()];
                }

            private:
                std::vector<io_looper*> _loopers;
                std::atomic<int>        _next;
            };

            std::unordered_map<service_node*, per_node_loopers*> _loopers;

        public:
            io_looper* get_io_looper(service_node* node, bool is_local)
            {
                if (is_local)
                {
                    auto worker = task::get_current_worker2();
                    if (worker != nullptr)
                    {
                        auto worker2 = task_worker::remote(node, THREAD_POOL_IO, worker->index());
                        if (worker != nullptr && worker == worker2)
                            return static_cast<io_looper*>(worker);
                    }
                }

                dassert(node, "node is not given");
                auto it = _loopers.find(node);
                if (it == _loopers.end())
                {
                    auto loopers = new per_node_loopers(node);
                    _loopers[node] = loopers;
                    return loopers->fetch_next();
                }
                else
                    return it->second->fetch_next();
            }
        };

        io_looper* get_io_looper(service_node* node, bool is_local)
        {
            return io_looper_holder::instance().get_io_looper(node, is_local);
        }

        event_loop* event_loop_create(bool per_thread /*= false*/)
        {
            if (per_thread) 
            {
                io_looper* looper = dynamic_cast<io_looper*>(::dsn::task::get_current_worker2());
                if (looper != nullptr)
                   return (event_loop*)looper;
                else {
                    dfatal("invalid per thread looper create as the current thread does not have a io_looper yet\n"
                        "you probably want to set the properties of the current thread pool as follows:\n"
                        "[threadpool.%s]\n"
                        "worker_factory_name = dsn::tools::io_looper\n"
                        "queue_factory_name = dsn::tools::io_looper_queue\n",
                        dsn_threadpool_code_to_string(::dsn::task::get_current_pool_id())
                    );
                    return nullptr;
                }
            }
            else 
            {
                auto looper = new io_looper(nullptr, 0, nullptr);
                looper->create_completion_queue();
                return (event_loop*)(looper);
            }
        }

        bool event_loop_running(event_loop* loop)
        {
            return !((io_looper*)loop)->is_stopped();
        }

        void event_loop_run(event_loop* loop)
        {
            auto looper = (io_looper*)loop;
            if (looper == dynamic_cast<io_looper*>(::dsn::task::get_current_worker2())) {
                ::dsn::task::get_current_worker2()->run_internal();
            } else {
                looper->loop();
            }
        }
        
        void event_loop_destroy(event_loop* loop)
        {
            auto looper = (io_looper*)loop;
            if (looper == dynamic_cast<io_looper*>(::dsn::task::get_current_worker2())) 
            {
                looper->reset();
            }
            else 
            {
                looper->close_completion_queue();
                delete looper;
            }
        }

        error_code event_loop_bind_fd(
            event_loop* loop,
            int fd, 
            int mask,
            io_loop_callback cb,
            void* ctx,
            bool edge_trigger
            ) 
        {
            auto looper = (io_looper*)loop;
            return looper->bind_io_handle(fd, cb, ctx, mask, edge_trigger);
        }

        // cb is not necessary on windows
        void event_loop_unbind_fd(
            event_loop* loop,
            int fd, 
            int mask
            )
        {
            auto looper = (io_looper*)loop;
            looper->unbind_io_handle(fd, mask); 
        }

        void event_loop_add_oneshot_timer(
            event_loop* loop,
            const char* name, 
            lpc_callback cb, 
            void* data, 
            void* data2, 
            int milliseconds
            )
        {
            auto looper = (io_looper*)loop;
            looper->add_timer(name, cb, data, data2, milliseconds);
        }


        void event_loop_stop(event_loop* loop)
        {
            auto looper = (io_looper*)loop;
            looper->transient_stop();
        }


        int event_loop_events(event_loop* l, int fd) 
        {
            auto looper = (io_looper*)l;
            return looper->events(fd);
        }
    }
}

# ifndef _WIN32

namespace dsn
{
    namespace tools
    {
        error_code io_looper::bind_io_handle(
                int fd,
                io_loop_callback cb, 
                void* ctx,
                int mask,
                bool edge_trigger /*= true*/
                )
        {
            if (is_local())
                return local_bind_io_handle(fd, cb, ctx, mask, edge_trigger);
            else {
                lpf* pf = new lpf([=]() {
                    local_bind_io_handle(fd, cb, ctx, mask, edge_trigger);
                });
                add_lpc("bind_io_handle", pf, [](lpf* f) {delete f;});
                return ERR_OK;
            }
        }

        // cb is not necessary on windows
        void io_looper::unbind_io_handle(int fd, int mask)
        {
            if (is_local())
                local_unbind_io_handle(fd, mask);
            else {
                lpf* cb = new lpf([=]() {
                    local_unbind_io_handle(fd, mask);
                });
                add_lpc("unbind_io_handle", cb, [](lpf* f) { delete f;});
            }
        }


        int  io_looper::events(int fd) const
        {
            dassert (fd < _fd_entries.size(), "invalid given fd");
            return _fd_entries[fd].mask;
        }
        
# if HAVE_KQUEUE

        error_code io_looper::local_bind_io_handle(
            int fd,
            io_loop_callback cb,
            void* ctx,
            int mask,
            bool edge_trigger
            )
        {
            if (fd + 1 > _fd_entries.size()) 
                _fd_entries.resize(fd * 2 + 2);

            fd_entry& entry = _fd_entries[fd];
            void* epx = (void*)(uintptr_t)(fd);
            struct kevent ke;
            int flag = edge_trigger ? (EV_ADD | EV_CLEAR) : EV_ADD;

            if (mask & FD_READABLE) 
            {
                EV_SET(&ke, fd, EVFILT_READ, flag, 0, 0, epx);
                if (kevent(_sys_looper, &ke, 1, NULL, 0, NULL) == -1) goto err;
            }
            if (mask & FD_WRITABLE) 
            {
                EV_SET(&ke, fd, EVFILT_WRITE, flag, 0, 0, epx);
                if (kevent(_sys_looper, &ke, 1, NULL, 0, NULL) == -1) goto err;
            }
            if (mask & FD_USER) 
            {
                EV_SET(&ke, fd, EVFILT_USER, flag, 0, 0, epx);
                if (kevent(_sys_looper, &ke, 1, NULL, 0, NULL) == -1) goto err;
            }

            // on success
            entry.mask |= mask;

            if (mask & FD_READABLE) 
            {
                entry.read_callback = cb;
                entry.read_context = ctx;
            }
            if (mask & FD_WRITABLE) 
            {
                entry.write_callback = cb;
                entry.write_context = ctx;
            }
            if (mask & FD_USER) 
            {
                entry.user_callback = cb;
                entry.user_context = ctx;
            }

            dinfo("bind io handler to looper(%d) ok, mask = %x, current_mask = %x, fd = %d", 
                        _sys_looper, mask, entry.mask, fd);
            return ERR_OK;

        err:
            derror("bind io handler to looper(%d) failed, err = %s, mask = %x, current_mask = %x, fd = %d", 
                        _sys_looper, strerror(errno), mask, entry.mask, fd);
            return ERR_BIND_IOCP_FAILED;
        }
        
        void io_looper::local_unbind_io_handle(int fd, int mask)
        {
            fd_entry& entry = _fd_entries[fd];
            if (0 == entry.mask) return;

            void* epx = (void*)(uintptr_t)(fd);
            struct kevent ke;

            if (mask & FD_READABLE & entry.mask) 
            {
                EV_SET(&ke, fd, EVFILT_READ, EV_DELETE, 0, 0, epx);
                kevent(_sys_looper, &ke, 1, NULL, 0, NULL);
            }
            if (mask & FD_WRITABLE & entry.mask) 
            {
                EV_SET(&ke, fd, EVFILT_WRITE, EV_DELETE, 0, 0, epx);
                kevent(_sys_looper, &ke, 1, NULL, 0, NULL);
            }
            if (mask & FD_USER & entry.mask) 
            {
                EV_SET(&ke, fd, EVFILT_USER, EV_DELETE, 0, 0, epx);
                kevent(_sys_looper, &ke, 1, NULL, 0, NULL);
            }

            // on success
            entry.mask &= ~mask;

            dinfo("unbind io handler to looper(%d), mask = %x, current_mask = %x, fd = %d", 
                        _sys_looper, mask, entry.mask, fd);
            return;
        }

        # define LPC_NOTIFICATION_FD (0)

        void __lpc_callback(event_loop* lp, int fd, void* ctx, int events)
        {
            auto looper = (io_looper*)lp;
            while (looper->handle_local_queue_once()) {}
        }

        void io_looper::add_lpc(const char* name, lpc_callback cb, void* data, void* data2)
        {
            dinfo("io_looper add lpc %s, data = %p, cb = %p", name, data, cb);

            _lpc_queue.enqueue(lpc_entry{name, cb, data, data2});

            void* epx = (void*)(uintptr_t)(LPC_NOTIFICATION_FD);
            struct kevent e;
            EV_SET(&e, _lpc_fd, EVFILT_USER, EV_ONESHOT, (NOTE_FFCOPY | NOTE_TRIGGER), 0, epx);

            if (kevent(_sys_looper, &e, 1, nullptr, 0, nullptr) == -1)
            {
                dassert(false, "post local notification via kevent failed, err = %s", strerror(errno));
            }
        }

        void io_looper::create_completion_queue()
        {
            int mask;
            _sys_looper = kqueue();
            dassert(-1 != _sys_looper, "create kqueue failed, err = %s", strerror(errno));
            mask = FD_USER;
            _lpc_fd = LPC_NOTIFICATION_FD;

            int err = local_bind_io_handle(_lpc_fd, __lpc_callback, nullptr, mask);
            dassert (err == ERR_OK, "bind io handler to looper failed, fd = %d", _lpc_fd);
        }

        void io_looper::close_completion_queue()
        {
            if (_sys_looper != 0)
            {
                unbind_io_handle(_lpc_fd, FD_USER);
                close(_sys_looper);
                _sys_looper = 0;
            }
        }

        void io_looper::loop()
        {
            uint64_t last_timer_ms = now_ms_steady();
            uint64_t now_ms = last_timer_ms;

            _loop_stop = false; 
            while (!_loop_stop)
            {
                struct kevent events[128];
                uint32_t next_timer_delay_ms = (uint32_t)lpTimerHeapTopNextMilliseconds(_timers, now_ms);

                struct timespec timeout;
                timeout.tv_sec = next_timer_delay_ms / 1000;
                timeout.tv_nsec = (next_timer_delay_ms % 1000) * 1000000;

                int retval = kevent(_sys_looper, NULL, 0, events, sizeof(events) / sizeof(struct kevent), &timeout);
                now_ms = now_ms_steady();
                if (now_ms >= last_timer_ms + next_timer_delay_ms)
                {
                    exec_timer_tasks(now_ms);
                    last_timer_ms = now_ms;
                }

                if (retval > 0) // events fired
                {
                    for (int j = 0; j < retval; j++)
                    {
                        struct kevent *e = &events[j];
                        int fd = (int)(intptr_t)(e->udata);
                        fd_entry& entry = _fd_entries[fd];

                        if (e->filter == EVFILT_READ && (entry.mask & FD_READABLE))
                        {
                            entry.read_callback((event_loop*)this, fd, entry.read_context, FD_READABLE);
                        }

                        if (e->filter == EVFILT_WRITE && (entry.mask & FD_WRITABLE))
                        {
                            entry.write_callback((event_loop*)this, fd, entry.write_context, FD_WRITABLE);
                        }

                        if (e->filter == EVFILT_USER && (entry.mask & FD_USER))
                        {
                            entry.user_callback((event_loop*)this, fd, entry.user_context, FD_READABLE);
                        }
                    }
                }
                else if (retval < 0)
                {
                    derror("kevent failed with err = %s", strerror(errno));
                }
            }
        }

# elif HAVE_EPOLL

        error_code io_looper::local_bind_io_handle(
            int fd,
            io_loop_callback cb,
            void* ctx,
            int mask,
            bool edge_trigger
            )
        {
            dassert (mask && cb, "invalid arguments");
            if (fd + 1 > _fd_entries.size())
                 _fd_entries.resize(fd * 2 + 2);

            fd_entry& entry = _fd_entries[fd];
            void* epx = (void*)(uintptr_t)(fd);
            int flag = entry.mask == 0 ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
            int mmask = (entry.mask | mask);

            struct epoll_event ee = { 0 }; /* avoid valgrind warning */            
            ee.events = edge_trigger ? EPOLLET : 0;
            ee.data.ptr = epx;

            if (mmask & FD_READABLE)
                ee.events |= EPOLLIN;
            if (mmask & FD_WRITABLE)
                ee.events |= EPOLLOUT;
            
            if (epoll_ctl(_sys_looper, flag, fd, &ee) == -1)
            {
                derror("bind io handler to looper(%d) failed, flag = %x, err = %s, merged mask = %x, fd = %d", 
                           _sys_looper, flag, strerror(errno), mmask, fd);
                return ERR_BIND_IOCP_FAILED;
            }
            else
            {
                // on success
                entry.mask = mmask;

                if (mask & FD_READABLE) 
                {
                    entry.read_callback = cb;
                    entry.read_context = ctx;
                }

                if (mask & FD_WRITABLE) 
                {
                    entry.write_callback = cb;
                    entry.write_context = ctx;
                }

                dinfo("bind io handler to looper(%d) ok, mask = %x, current_mask = %x, fd = %d", _sys_looper, mask, entry.mask, fd);
                return ERR_OK;
            }
        }

        void io_looper::local_unbind_io_handle(int fd, int mask)
        {
            fd_entry& entry = _fd_entries[fd];
            if (0 == entry.mask) return;

            void* epx = (void*)(uintptr_t)(fd);
            int mmask = (entry.mask & ~mask);
            int flag = mmask == 0 ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;

            struct epoll_event ee = { 0 }; /* avoid valgrind warning */
            ee.events = 0;
            ee.data.ptr = epx;

            if (mmask & FD_READABLE) ee.events |= EPOLLIN;
            if (mmask & FD_WRITABLE) ee.events |= EPOLLOUT;
            
            epoll_ctl(_sys_looper, flag, fd, &ee);
            entry.mask = mmask;

            dinfo("unbind io handler to looper(%d), mask = %x, current_mask = %x, fd = %d", _sys_looper, mask, entry.mask, fd);
        }

        void io_looper::add_lpc(const char* name, lpc_callback cb, void* data, void* data2)
        {
            dinfo("io_looper add lpc %s, data = %p, data2 = %p, cb = %p", name, data, data2, cb);

            _lpc_queue.enqueue(lpc_entry{ name, cb, data, data2 });

            // write on event fd only support 64-bit integer
            int64_t c = 1;
            if (write(_lpc_fd, &c, sizeof(c)) < 0)
            {
                dassert(false, "post local notification via eventfd failed, err = %s", strerror(errno));
            }
        }

        void __lpc_callback(event_loop* lp, int fd, void* ctx, int events)
        {
            auto looper = (io_looper*)lp;
            looper->lpc_handler(events);
        }

        void io_looper::lpc_handler(int mask) 
        {
            int64_t notify_count = 0;

            if (read(_lpc_fd, &notify_count, sizeof(notify_count)) != sizeof(notify_count))
            {
                return;
            } 
            
            for (int64_t i = 0; i < notify_count; i++)
            {
                bool r = handle_local_queue_once();
                dassert(r, "io_looper dequeue lpc failed");
            }
        }

        void io_looper::create_completion_queue()
        {
            _sys_looper = epoll_create(1024);
            dassert(-1 != _sys_looper, "epoll_create failed, err = %s", strerror(errno));

            _lpc_fd = eventfd(0, EFD_NONBLOCK);
            dinfo("io looper create ok looper fd = %d, lpc fd = %d", _sys_looper, _lpc_fd);

            int err = local_bind_io_handle(_lpc_fd, __lpc_callback, nullptr, FD_READABLE);
            dassert(err == ERR_OK, "bind io handler to looper failed, fd = %d", _lpc_fd);
        }

        void io_looper::close_completion_queue()
        {
            if (_sys_looper != 0)
            {
                local_unbind_io_handle(_lpc_fd, FD_READABLE);
                close(_lpc_fd);
                close(_sys_looper);
                dinfo("io looper close ok looper fd = %d, lpc fd = %d", _sys_looper, _lpc_fd);
                _sys_looper = 0;
            }
        }

        void io_looper::loop()
        {
            uint64_t last_timer_ms = now_ms_steady();
            uint64_t now_ms = last_timer_ms;

            _loop_stop = false; 
            while (!_loop_stop)
            {
                struct epoll_event events[128];
                uint32_t next_timer_delay_ms = (uint32_t)lpTimerHeapTopNextMilliseconds(_timers, now_ms);

                int retval = epoll_wait(_sys_looper, events, (int)sizeof(events) / sizeof(struct epoll_event), next_timer_delay_ms);
                now_ms = now_ms_steady();
                if (now_ms >= last_timer_ms + next_timer_delay_ms)
                {
                    exec_timer_tasks(now_ms);
                    last_timer_ms = now_ms;
                }

                if (retval > 0) // events fired
                {
                    for (int j = 0; j < retval; j++)
                    {
                        int mask = 0;
                        struct epoll_event *e = &events[j];
                        int fd = (int)(intptr_t)(e->data.ptr);
                        fd_entry& entry = _fd_entries[fd];

                        if (e->events & EPOLLIN) mask |= FD_READABLE;
                        if (e->events & EPOLLOUT) mask |= FD_WRITABLE;
                        if (e->events & EPOLLERR) mask |= FD_WRITABLE;
                        if (e->events & EPOLLHUP) mask |= FD_WRITABLE;

                        dinfo("epoll_wait for fd = %d, mask = %x", fd, mask);

                        bool read_executed = false;
                        if (mask & FD_READABLE & entry.mask) 
                        {
                            read_executed = true;
                            entry.read_callback((event_loop*)this, fd, entry.read_context, mask);
                        }
                        if (mask & FD_WRITABLE & entry.mask) 
                        {
                           if (!read_executed || entry.read_callback != entry.write_callback)
                               entry.write_callback((event_loop*)this, fd, entry.write_context, mask);
                        }
                    }
                }
                else if (retval < 0)
                {
                    derror("epoll_wait failed with err = %s", strerror(errno));
                }
            }
        }
# endif
        
    }
}

# endif 
