/*
* Description:
*     handle data frame in http2
*
* Revision history:
*     Jul. 25, 2017, Zhenyu Guo, first version
*     xxxx-xx-xx, author, fix bug about xxx
*/
# include "http2_message_parser.h"

namespace dsn
{
    void http2_message_parser::delete_http2_stream(uint32_t sid)
    {
        auto it_info = _streams_info.find(sid);
        if (it_info != _streams_info.end())
            _streams_info.erase(it_info);
        auto it_m = _mapped_between_mid_with_sid.find(sid);
        if (it_m != _mapped_between_mid_with_sid.end())
            _mapped_between_mid_with_sid.erase(it_m);
    }

    void http2_message_parser::create_http2_stream(uint32_t mid, uint32_t sid)
    {
        _mapped_between_mid_with_sid.emplace(mid,sid);
        _h2_cur_stream = new http2_stream();
        _h2_cur_stream->pms.msg_id = mid;
        _streams_info.emplace(sid,std::unique_ptr<http2_stream>(_h2_cur_stream));
    }
}
