/*
 * Description:
 *     per thread profiler
 *
 * Revision history:
 *     Mar. 7th, 2015, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# pragma once

# include <dsn/utility/ports.h>
# include <dsn/utility/dlib.h>

//# define THR_PROF 1
# ifdef THR_PROF

# define TPF_INIT()   ::dsn::thr_prof_init()
# define TPF_MARK(x)  ::dsn::thr_prof_mark(x)

# else

# define TPF_INIT()
# define TPF_MARK(x)

# endif

namespace dsn 
{
    DSN_API void thr_prof_init();
    DSN_API void thr_prof_mark(const char* last_range_name);
}
