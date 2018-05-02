

/*
* Description:
*     helper class to ensure certain context are always accessed
*     from a single thread (e.g., rpc-session)
*
* Revision history:
*     Mar., 2017, @imzhenyu (Zhenyu Guo), first version
*     xxxx-xx-xx, author, fix bug about xxx
*/

# pragma once

# include <dsn/service_api_c.h>
# include <dsn/utility/misc.h>

namespace dsn
{
    class single_thread_context
    {
    private:
        int _tid;

    public:
        single_thread_context() : _tid(0) {}

        void check_thread_access() 
        { 
            dassert(_tid == 0 || _tid == utils::get_current_tid(), 
                "this context must be accessed from thread %d", _tid); 
        }

        void init_thread_checker()
        {
            check_thread_access();
            _tid = utils::get_current_tid();
        }

        void init_thread_checker(int rtid)
        {
            check_thread_access();
            _tid = rtid;
        }
    };

    class single_thread_ref_counter
    {
    public:
        single_thread_ref_counter() : _counter(0)
        {
        }

        virtual ~single_thread_ref_counter() {}

        void add_ref()
        {
            ++_counter;
            //dinfo("add_ref: %p: %d", this, (int)_counter);
        }

        void release_ref()
        {
            if (--_counter == 0)
            {
                delete this;
            }
            //dinfo("rel_ref: %p: %d", this, (int)_counter);
        }

        long get_count() const
        {
            return _counter;
        }

    private:
        long _counter;

    public:
        single_thread_ref_counter(const single_thread_ref_counter&) = delete;
        single_thread_ref_counter& operator=(const single_thread_ref_counter&) = delete;
    };
}

