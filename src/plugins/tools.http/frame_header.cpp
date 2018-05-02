/*
* Description:
*     handle header frame in http2
*
* Revision history:
*     Dec. 27, 2016, Zhenyu Guo, first version
*     xxxx-xx-xx, author, fix bug about xxx
*/

# include "http2_message_parser.h"
# include "HPACK.h"

namespace dsn
{
    int  http2_message_parser::encode_headers(std::vector<header>& headers, message_ex* msg)
    {
        message_header* hdr = msg->header;
        int count = 0;
        if (hdr->context.u.is_request)
        {
            headers.emplace_back(header(":method", "POST"));
            headers.emplace_back(header(":scheme", "http"));
            headers.emplace_back(header(":path", msg->dheader.rpc_name));
            headers.emplace_back(header("te", "trailers"));
            headers.emplace_back(header(":authority", "localhost"));

            count = 5;
        }
        else
        {
            char buffer[16];
            sprintf(buffer, "%d", (int)hdr->context.u.server_error);
            headers.emplace_back(header(":status", "200"));
            headers.emplace_back(header("dsn-error", buffer));
            count = 2;
        }

        ++count;
        headers.emplace_back(header("content-type", "application/grpc"));

        ++count;
        headers.emplace_back(header("grpc-encoding", "identity"));

        char buffer[128];

        ++count;
        sprintf(buffer, "%" PRId64, hdr->trace_id);
        headers.emplace_back(header("dsn-trace-id", buffer));

        return count;
    }

    int http2_message_parser::decode_headers(const std::vector<header>& headers, message_ex* msg)
    {
        message_header* hdr = msg->header;
        int count = 0;
        for (auto& kv : headers)
        {
            if (kv.first == ":path")
            {
                ++count;

                hdr->context.u.is_request = 1;
                msg->local_rpc_code = dsn_task_code_from_string(kv.second.c_str(), TASK_CODE_INVALID);
                msg->dheader.rpc_name = task_spec::get(msg->local_rpc_code)->name.c_str();
            }
            else if (kv.first == ":status" && kv.second != "200")
            {
                dwarn("TODO: status (%s) to error-code translation, set to ERR_UNKNOWN error for the time bing", kv.second.c_str());
                hdr->context.u.server_error = RPC_ERR_UNKNOWN;
            }
            else if (kv.first == "grpc-status" && kv.second != "0")
            {
                dwarn("TODO: status (%s) to error-code translation, set to ERR_UNKNOWN error for the time bing", kv.second.c_str());
                hdr->context.u.server_error = RPC_ERR_UNKNOWN;
            }
            else if (kv.first == "dsn-error")
            {
                ++count;
                hdr->context.u.server_error = atoi(kv.second.c_str());
            }
            else if (kv.first == "dsn-trace-id")
            {
                ++count;
                
                hdr->trace_id = atoll(kv.second.c_str());
            }
            else 
            {
                ddebug("unused http2 header %s = %s", kv.first.c_str(), kv.second.c_str());
            }
        }

        // TODO: translate status and grpc-status to error code 
        

        return count;
    }

    error_code http2_message_parser::handle_header_frame(http2_frame* fm, uint32_t size)
    {
        auto err = ERR_OK;
        auto frame = (http2_header_frame*)fm;
        dassert (frame->flags & HTTP2_DATA_FLAG_END_HEADERS,
             "we don't support headers spanning more than one frame for now'");

        auto sid = frame->stream_id();
        auto it_s = _streams_info.find(sid);
        if (it_s == _streams_info.end())
        {
            create_http2_stream(sid,sid);
            _recving_msg->header->id = sid;
        }
        else
            _recving_msg->header->id = it_s->second.get()->pms.msg_id;

        auto sz = frame->headers_length();
        char* headers = frame->headers_ptr();

        std::vector<header> rheaders;
        hpack_decode(rheaders, (uint8_t*)headers, &_hpack_table, sz);

        decode_headers(rheaders, _recving_msg);
        return err;
    }
}
