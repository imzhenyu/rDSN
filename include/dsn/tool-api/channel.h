


# pragma once 

# include <dsn/tool-api/network.h>

namespace dsn 
{
    /*!
    @addtogroup tool-api-providers
    @{
    */
    /*!
        a channel abstracts the communication between two end-points (e.g., client and server)
        messages using the same channel should use the same header formart (see `open' below)
    */
    class channel 
    {
    public:
        //
        // network factory prototype
        //
        template <typename T> static channel* create()
        {
            return new T();
        }
        
        typedef channel* (*factory)();

    public:
        channel() : _is_async(true) {}
        virtual ~channel() {}

        virtual error_code open(
            const char* target, 
            net_channel channel,
            net_header_format hdr_fmt,
            bool is_async
            ) = 0;

        // close should also handle destruction of this channel
        virtual void close() = 0;

        // when dynamic policy control is needed
        virtual void ioctl(int code, void* ctl_block, int block_size) {}

        // call can be empty for one-way RPC
        virtual void call(
            message_ex* request,
            rpc_response_task* call
        ) = 0;

        // blocking recv next message, only usable for channel with is_async = false
        virtual message_ex* recv_block() = 0;

        bool is_async() const { return _is_async; }

    public:
        DSN_API static channel* from_dsn_handle(dsn_channel_t ch);

    protected:
        bool _is_async;
    };

    /*@}*/
}
