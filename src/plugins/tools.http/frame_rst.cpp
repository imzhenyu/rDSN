/*
* Description:
*     handle rst frame in http2
*
* Revision history:
*     Dec. 27, 2016, Zhenyu Guo, first version
*     xxxx-xx-xx, author, fix bug about xxx
*/

# include "http2_message_parser.h"

namespace dsn
{
    error_code http2_message_parser::handle_rst_frame(http2_frame* fm, uint32_t size)
    {
        auto err = ERR_OK;
        auto frame = (http2_rst_frame*)fm;

        dassert (frame->stream_id(), "settings id must be 0");

        return err;
    }
}
