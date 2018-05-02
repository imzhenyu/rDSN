/*
 * Description:
 *     rpc client matcher 
 *
 * Revision history:
 *     Mar., 2015, @imzhenyu (Zhenyu Guo), first version
 *     Jan., 2017, Zhenyu Guo, refined in separated header file 
 */


# pragma once 

# include <dsn/tool-api/task.h>
# include <dsn/tool-api/rpc_message.h>
# include <dsn/utility/synchronize.h>

namespace dsn 
{
//
// client matcher for matching RPC request and RPC response, and handling timeout
// 
#define MATCHER_BUCKET_NR 101
class network;
class rpc_client_matcher
{
public:
    rpc_client_matcher() : _flying_call_count(0) {}
    DSN_API ~rpc_client_matcher();

    //
    // when a two-way RPC call is made, register the requst id and the callback
    // which also registers a timer for timeout tracking
    //
    DSN_API void on_call(message_ex* request, rpc_response_task* call);

    //
    // when a RPC response is received, call this function to trigger calback
    //  key - message.header.id
    //  reply - rpc response message
    //  delay_ms - sometimes we want to delay the delivery of the message for certain purposes
    //
    // we may receive an empty reply to early terminate the rpc
    //
    DSN_API bool on_recv_reply(network* net, uint32_t key, message_ex* reply, int delay_ms);

    long flying_call_count() const { return _flying_call_count;  }

private:
    typedef std::unordered_map<uint32_t, rpc_response_task*> rpc_requests;
    rpc_requests                   _requests[MATCHER_BUCKET_NR];
    long                           _flying_call_count;
    //::dsn::utils::ex_lock_nr_spin _requests_lock[MATCHER_BUCKET_NR];
};

}