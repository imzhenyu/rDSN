/*
* Description:
*     handle continuation frame in http2
*
* Revision history:
*     Dec. 27, 2016, Zhenyu Guo, first version
*     xxxx-xx-xx, author, fix bug about xxx
*/

# include "http2_message_parser.h"

namespace dsn
{
    error_code http2_message_parser::handle_continuation_frame(http2_frame* fm, uint32_t size)
    {
        auto err = ERR_OK;
        dwarn("%s not implemented, skipped for now", __FUNCTION__);
        return err;
    }
}
