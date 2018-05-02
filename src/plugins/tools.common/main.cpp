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


# include <dsn/utility/module_init.cpp.h>

# include "lockp.std.h"
# include "native_aio_provider.posix.h"
# include "native_aio_provider.linux.h"
# include "simple_perf_counter.h"
# include "simple_perf_counter_v2_atomic.h"
# include "simple_perf_counter_v2_fast.h"
# include "simple_task_queue.h"
# include "simple_logger.h"
# include "empty_aio_provider.h"
# include "dsn_message_parser.h"
# include "raw_message_parser.h"

# include "hpc_task_queue.h"
# include "hpc_tail_logger.h"
# include "hpc_logger.h"
# include "hpc_aio_provider.h"
# include "hpc_network_provider.h"
# include "hpc_env_provider.h"
# include "hpc_timer_service.h"

# include "nativerun.h"
# include "tracer.h"
# include "profiler.h"
# include "fault_injector.h"

# ifdef _WIN32
# pragma comment(lib, "ws2_32.lib")
# endif

namespace dsn {
    namespace tools {
        void register_common_providers()
        {
            register_component_provider<env_provider>("dsn::env_provider");
            register_component_provider<memory_provider>("dsn::default_memory_provider");
            register_component_provider<task_worker>("dsn::task_worker");
            register_component_provider<screen_logger>("dsn::tools::screen_logger");
            register_component_provider<simple_logger>("dsn::tools::simple_logger");
            register_component_provider<std_lock_provider>("dsn::tools::std_lock_provider");
            register_component_provider<std_lock_nr_provider>("dsn::tools::std_lock_nr_provider");
            register_component_provider<std_rwlock_nr_provider>("dsn::tools::std_rwlock_nr_provider");
            register_component_provider<std_semaphore_provider>("dsn::tools::std_semaphore_provider");            
            register_component_provider<simple_task_queue>("dsn::tools::simple_task_queue");
            
            register_message_header_parser<dsn_message_parser>(NET_HDR_DSN, {"RDSN"});
            //register_message_header_parser<thrift_message_parser>(NET_HDR_THRIFT, {"THFT"});
            register_message_header_parser<raw_message_parser>(NET_HDR_RAW, {"_RAW"});

#if defined(_WIN32)
#elif defined(__linux__)
            register_component_provider<native_linux_aio_provider>("dsn::tools::native_aio_provider");
            register_component_provider<native_posix_aio_provider>("dsn::tools::posix_aio_provider");
#else
            register_component_provider<native_posix_aio_provider>("dsn::tools::native_aio_provider");
#endif
            register_component_provider<empty_aio_provider>("dsn::tools::empty_aio_provider");        

            register_component_provider<hpc_tail_logger>("dsn::tools::hpc_tail_logger");
            register_component_provider<hpc_logger>("dsn::tools::hpc_logger");
            register_component_provider<hpc_task_queue>("dsn::tools::hpc_task_queue");
            register_component_provider<hpc_task_priority_queue>("dsn::tools::hpc_task_priority_queue");            
            //register_component_provider<hpc_concurrent_task_queue>("dsn::tools::hpc_concurrent_task_queue");
            register_component_provider<hpc_env_provider>("dsn::tools::hpc_env_provider");
            
            register_component_provider<io_looper>("dsn::tools::io_looper"); // task worker
            register_component_provider<io_looper_queue>("dsn::tools::io_looper_queue"); // task queue
            register_component_provider<hpc_aio_provider>("dsn::tools::hpc_aio_provider");
            register_component_provider<hpc_network_provider>("dsn::tools::hpc_network_provider");
            register_component_provider<hpc_timer_service>("dsn::tools::hpc_timer_service");

            ::dsn::tools::internal_use_only::register_component_provider(
                "dsn::tools::simple_perf_counter",
                simple_perf_counter_factory,
                PROVIDER_TYPE_MAIN
                );
            ::dsn::tools::internal_use_only::register_component_provider(
                "dsn::tools::simple_perf_counter_v2_atomic",
                simple_perf_counter_v2_atomic_factory,
                PROVIDER_TYPE_MAIN
                );
            ::dsn::tools::internal_use_only::register_component_provider(
                "dsn::tools::simple_perf_counter_v2_fast",
                simple_perf_counter_v2_fast_factory,
                PROVIDER_TYPE_MAIN
                );
        }
    }
}

MODULE_INIT_BEGIN(tools_common)
    dsn::tools::register_common_providers();
    dsn::tools::register_tool<dsn::tools::nativerun>("nativerun");
    dsn::tools::register_toollet<dsn::tools::tracer>("tracer");
    dsn::tools::register_toollet<dsn::tools::profiler>("profiler");
    dsn::tools::register_toollet<dsn::tools::fault_injector>("fault_injector");
MODULE_INIT_END
