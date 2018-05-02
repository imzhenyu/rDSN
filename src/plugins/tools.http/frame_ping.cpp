/*
* Description:
*     handle ping frame in http2
*
* Revision history:
*     Dec. 27, 2016, Zhenyu Guo, first version
*     xxxx-xx-xx, author, fix bug about xxx
*/

# include "http2_message_parser.h"

namespace dsn
{
    error_code http2_message_parser::handle_ping_frame(http2_frame* fm, uint32_t size)
    {
        auto err = ERR_OK;
        auto frame = (http2_ping_frame*)fm;

        dassert (frame->stream_id() == 0, "HTTP2_FRAME_PING sid is not 0!");
        dassert (frame->length() == 8, "HTTP2_FRAME_PING length is not 8!");

        if (!(frame->flags & HTTP2_FLAG_ACK))
        {
            _need_ping_ack_opaque_vec.push_back(frame->ping_opaque_data);
            trigger_protocol_running(_received_messages);
        }

        return err;
    }
}
