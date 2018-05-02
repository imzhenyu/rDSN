
/*
 * Description:
 *     What is this file about?
 *
 * Revision history:
 *     xxxx-xx-xx, author, first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# include <dsn/tool-api/thread_profiler.h>
# include <dsn/utility/misc.h>
# include <sstream>

# ifdef THR_PROF

namespace dsn
{
    struct thread_perf_local_data
    {
        int      magic;
        int      last_index;
        uint64_t last_ts;
        uint64_t last_report_ts;

        std::vector<const char*> range_names;
        std::vector<uint64_t>    range_sums;
        uint64_t                 count;
    };

    static thread_local thread_perf_local_data s_local_perf;

    void thr_prof_report();
    void thr_prof_init()
    {
        if (s_local_perf.magic != 0xdeadbeef)
        {
            s_local_perf.magic = 0xdeadbeef;
            s_local_perf.count = 0;
            s_local_perf.last_report_ts = utils::get_current_rdtsc();
        }
        else
        {
            thr_prof_mark("<begin>");
        }

        auto nts = utils::get_current_rdtsc();
        s_local_perf.last_ts = nts;
        s_local_perf.last_index = 0;
        ++s_local_perf.count;

        // ~ 5 seconds
        if (nts > s_local_perf.last_report_ts + 10000000000ULL)
        {
            s_local_perf.last_report_ts = nts;
            thr_prof_report();
            s_local_perf.last_ts = utils::get_current_rdtsc();
        }
    }

    void thr_prof_mark(const char* name)
    {
        if (s_local_perf.magic != 0xdeadbeef)
            return;

        auto index = s_local_perf.last_index;
        if (index + 1 > s_local_perf.range_sums.size())
        {
            s_local_perf.range_sums.resize(index + 1);
            s_local_perf.range_names.resize(index + 1);

            s_local_perf.range_names[index] = name;
            s_local_perf.range_sums[index] = 0;
        }
        else if (name != s_local_perf.range_names[index])
        {
            printf("WARNING: different paths are profiled, skipped\n");
            return;
        }

        ++s_local_perf.last_index;
        
        auto nts = utils::get_current_rdtsc();
        auto duration = nts - s_local_perf.last_ts;

        s_local_perf.range_sums[index] += duration;
        s_local_perf.last_ts = nts;
    }

    void thr_prof_report()
    {
        std::stringstream ss;
        ss << std::endl 
            << "=========== thread profiling for thread " 
            << utils::get_current_tid() 
            << " ============" 
            << std::endl;
        
        for (int i = 0; i < s_local_perf.range_names.size(); i++)
        {
            ss  << "exec# = " << s_local_perf.count << ", duration(avg) = "
                << (double)s_local_perf.range_sums[i] / (double)s_local_perf.count << " rdtsc clks, name = ["
                << (i == 0 ? "<begin>" : s_local_perf.range_names[i-1]) << ", "
                << s_local_perf.range_names[i] << ")"
                << std::endl;
        }
        ss << std::endl;

        printf("%s\n", ss.str().c_str());
    }
}

# endif // THR_PROF