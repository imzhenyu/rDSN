/*
* Description:
*     handle settings frame in http2
*
* Revision history:
*     Dec. 27, 2016, Zhenyu Guo, first version
*     xxxx-xx-xx, author, fix bug about xxx
*/

# include "http2_message_parser.h"

namespace dsn
{
    error_code http2_message_parser::handle_settings_frame(http2_frame* fm, uint32_t size)
    {
        auto err = ERR_OK;
        auto frame = (http2_settins_frame*)fm;

        dassert (!frame->stream_id(), "settings id must be 0");

        if (frame->flags & HTTP2_FLAG_ACK)
            dassert(frame->length() == 0, "receive error settings_frame_ack");
        else
        {
            auto count = frame->entry_count();
            dassert (count < HTTP2_NUM_SETTINGS, "invalid settings count %d", (int)count);
            dassert (frame->length() % 6 == 0, "must a multiple of 6");

            for (int i = 0; i < count; ++i)
            {
                auto entry = &frame->entries[i];
                if (entry->id() >= HTTP2_NUM_SETTINGS)
                    continue;
                _settings_parameters[entry->id()].default_value = entry->value();
            }

            _need_settings_ack_times ++;
            trigger_protocol_running(_received_messages);
        }
        return err;
    }

}