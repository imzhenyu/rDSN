/*
* Description:
*     handle window_update frame in http2
*
* Revision history:
*     Dec. 27, 2016, Zhenyu Guo, first version
*     xxxx-xx-xx, author, fix bug about xxx
*/

# include "http2_message_parser.h"

namespace dsn
{
    error_code http2_message_parser::handle_window_frame(http2_frame* fm, uint32_t size)
    {
        auto err = ERR_OK;
        auto frame = (http2_window_update_frame*)fm;

        dassert(frame->length() == 4,"window_frame length must be 4 byte");

        if (frame->stream_id() == 0)
            _send_bytes_since_last_window_update += frame->window_size_increment();
        else
        {
            auto it = _streams_info.find(frame->stream_id());
            if (it != _streams_info.end())
                it->second.get()->send_bytes_delta_for_stream += frame->window_size_increment();
        }

        trigger_protocol_running(_received_messages);
        return err;
    }
}