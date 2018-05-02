


# pragma once 

# include <dsn/tool-api/channel.h>
# include <dsn/utility/synchronize.h>
# include <dsn/cpp/perf_counter_.h>

namespace dsn
{
    //
    // raw://hostname:port
    // raw://hostname:port/service_name
    //

    # define CH_RAW_COUNTER_CALL_ON  1
    # define CH_RAW_COUNTER_CALL_OFF 2
    # define CH_RAW_COUNTER_CPS_ON  3
    # define CH_RAW_COUNTER_CPS_OFF 4
    class raw_channel : public channel
    {
    public:
        raw_channel();
        virtual ~raw_channel();

        error_code open(
            const char* target, ///< raw://hostname:port/service_name
            net_channel channel,
            net_header_format hdr_fmt,
            bool is_async
        ) override;

        void close() override;

        void call(
            message_ex* request,
            rpc_response_task* call
        ) override;

        message_ex* recv_block() override;

        void ioctl(
            int code,
            void* ctl_block,
            int block_size
        ) override;

    private:
        network            *_net;
        rpc_session        *_s;
        net_header_format  _hdr_fmt;
        rpc_address        _target_address;
        utils::ex_lock_nr  _lock;

        safe_string        _target;
        safe_string        _service_name;

        bool               _counter_total_calls_enabled;
        perf_counter_      _counter_total_calls;
        bool               _counter_cps_enabled;
        perf_counter_      _counter_cps;
    };
}