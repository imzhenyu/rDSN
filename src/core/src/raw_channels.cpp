
/*
* Description:
*     What is this file about?
*
* Revision history:
*     xxxx-xx-xx, author, first version
*     xxxx-xx-xx, author, fix bug about xxx
*/

# include "raw_channel.h"
# include "rpc_engine.h"
# include <sstream>
# include <dsn/tool-api/thread_profiler.h>

namespace dsn
{
    raw_channel::raw_channel()
        : _hdr_fmt(NET_HDR_INVALID)
    {
        _net = nullptr;
        _s = nullptr;
        _counter_total_calls_enabled = false;
        _counter_cps_enabled = false;
    }

    raw_channel::~raw_channel()
    {
    }

    error_code raw_channel::open(
        const char* target,
        net_channel channel,
        net_header_format hdr_fmt,
        bool is_async
    )
    {
        dassert (_s == nullptr, 
            "a raw channel cannot be open multiple times");
        
        {
            std::stringstream ss;
            ss << target << ".call# @" << (void*)this;
            _counter_total_calls.init(
                "channels",
                ss.str().c_str(),
                COUNTER_TYPE_NUMBER,
                "total number of RPC calls through this channel"
            );
        }

        {
            std::stringstream ss;
            ss << target << ".call#/s @" << (void*)this;
            _counter_cps.init(
                "channels",
                ss.str().c_str(),
                COUNTER_TYPE_RATE,
                "RPC call# per second through this channel"
            );
        }

        _hdr_fmt = hdr_fmt;
        _target = target;
        _is_async = is_async;

        // parse target address 
        if (_target.find("raw://") != 0)
        {
            derror("target address '%s' does not begin with 'raw://'", 
                _target.c_str());
            return ERR_INVALID_PARAMETERS;
        }

        auto it = _target.find_last_of('/');
        if (it <= strlen("raw://"))
        {
            _service_name = "";
            _target_address = rpc_address(target + strlen("raw://"));
        }
        else
        {
            _service_name = _target.substr(it + 1);

            auto addr = _target.substr(strlen("raw://"), it - strlen("raw://"));
            _target_address = rpc_address(addr.c_str());
        }
        
        if (_target_address.port() == 0)
        {
            derror("invalid target address '%s'", target);
            return ERR_INVALID_PARAMETERS;
        }

        _net = ::dsn::task::get_current_rpc()->get_client_network(channel);
        if (_net == nullptr)
        {
            derror("cannot find network provider for transport type '%s' when creating '%s'",
                channel.to_string(), target
            );
            return ERR_NOT_SUPPORTED;
        }

        dinfo("open rpc channel %s with transport %s, protocol %s, is_async %s",
            target,
            channel.to_string(),
            hdr_fmt.to_string(),
            is_async ? "true" : "false"
            );
        // create first session
        _s = _net->create_client_session(hdr_fmt, _target_address, _is_async);
        _s->add_ref(); // released in close() 
        _s->connect();
        return ERR_OK;
    }

    // not thread safe
    void raw_channel::close()
    {
        dinfo("close rpc channel %s", _target_address.to_string());
        _s->disconnect_and_release();
        delete this;
    }

    // thread safe
    void raw_channel::call(
        message_ex* request,
        rpc_response_task* call
    )
    {
        //
        // fill message headers and context
        //
        request->from_address = _net->address();
        request->to_address = _target_address;
        request->u.client.call = call;
        request->dheader.service_name = _service_name;
        
        auto& hdr = *request->header;
      
        // automatic reconnect if necessary
        if (_s->is_disconnected())
        {
            bool connect = false;
            _lock.lock();
            if (_s->is_disconnected())
            {
                connect = true;
            }
            _lock.unlock();

            if (connect)
            {
                _s->connect();
            }
        }

        // join point and possible fault injection
        auto sp = task_spec::get(request->local_rpc_code);
        if (!sp->on_rpc_call.execute(task::get_current_task(), request, call, true))
        {
            ddebug("rpc request %s is dropped (fault inject), trace_id = %016" PRIx64,
                request->dheader.rpc_name,
                request->header->trace_id
            );

            if (call != nullptr)
            {
                call->set_delay(hdr.client.timeout_ms);
                call->enqueue(RPC_ERR_TIMEOUT, nullptr);
            }

            // request not used any more
            delete request;
            return;
        }

        TPF_MARK("rpc_session::send_message");
        _s->send_message(request);

        if (_counter_total_calls_enabled) _counter_total_calls.increment();
        if (_counter_cps_enabled) _counter_cps.increment();
    }

    message_ex* raw_channel::recv_block()
    {
        return _s->recv_message();   
    }

    void raw_channel::ioctl(int code, void* ctl_block, int block_size)
    {
        switch (code)
        {
        case CH_RAW_COUNTER_CALL_ON:
            _counter_total_calls_enabled = true;
            break;
        case CH_RAW_COUNTER_CALL_OFF:
            _counter_total_calls_enabled = false;
            break;
        case CH_RAW_COUNTER_CPS_ON:
            _counter_cps_enabled = true;
            break;
        case CH_RAW_COUNTER_CPS_OFF:
            _counter_cps_enabled = false;
            break;
        }
    }


    DSN_API channel* channel::from_dsn_handle(dsn_channel_t ch)
    {
        return static_cast<channel*>(ch);
    }
}
