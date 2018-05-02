/*
* Description:
*     handle data frame in http2
*
* Revision history:
*     Dec. 27, 2016, Zhenyu Guo, first version
*     xxxx-xx-xx, author, fix bug about xxx
*/

# include "http2_message_parser.h"

namespace dsn
{
    error_code http2_message_parser::handle_data_frame(http2_frame* fm, uint32_t size, ::dsn::blob& buffer)
    {
        auto err = ERR_OK;
        auto frame = (http2_data_frame*)fm;
        auto hdr = _recving_msg->header;
        
        auto sz = frame->data_length();
        auto offset = frame->data_start_offset();

        // first data frame
        if (hdr->body_length == 0)
        {
            auto msg = frame->msg_ptr();
            auto msg_len = msg->msg_length();

            _recving_msg->io_session_secret = msg_len; // borrowed and recovered to zero later

            sz -= msg->msg_offset();
            offset += msg->msg_offset();

            if (msg->compressed != 0)
            {
                dassert (false, "compressed message is not supported yet");
            }
        }

        // remaing bytes to be attached to msg 
        if (sz > _recving_msg->io_session_secret)
        {
            sz = _recving_msg->io_session_secret;
            _recving_msg->io_session_secret = 0;
        }
        else
        {
            _recving_msg->io_session_secret -= sz;
        }

        auto data = buffer.range(offset, sz);
        _recving_msg->buffers.push_back(data);
        hdr->body_length += sz;

        _recv_bytes_since_last_window_update += size;
        auto it = _streams_info.find(frame->stream_id());
        if (it != _streams_info.end())
            it->second.get()->recv_bytes_for_stream += size;

        if (_recving_msg->io_session_secret != 0)
        {
            dassert (!(fm->flags & HTTP2_DATA_FLAG_END_STREAM), "more data to be got");
        }

        return err;
    }
}
