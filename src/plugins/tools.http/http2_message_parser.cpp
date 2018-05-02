/*
* Description:
*     http message parser based on grpc parser impl`
*
* Revision history:
*     Dec. 26, 2016, Zhenyu Guo, first version
*     xxxx-xx-xx, author, fix bug about xxx
*/

# include "http2_message_parser.h"
# include <dsn/tool-api/rpc_message.h>
# include "HPACK.h"

# ifdef __TITLE__
# undef __TITLE__
# endif
# define __TITLE__ "http2.message.parser"

namespace dsn
{

template <typename T, size_t N>
char(&ArraySizeHelper(T(&array)[N]))[N];

#define MAX_MAX_HEADER_LIST_SIZE (1024 * 1024 * 1024)

const http2_setting_parameters
    s_http2_settings_parameters[HTTP2_NUM_SETTINGS] = {
        {NULL, 0, 0, 0, HTTP2_DISCONNECT_ON_INVALID_VALUE,
         HTTP2_PROTOCOL_ERROR},
        {"HEADER_TABLE_SIZE", 4096, 0, 0xffffffff,
         HTTP2_CLAMP_INVALID_VALUE, HTTP2_PROTOCOL_ERROR},
        {"ENABLE_PUSH", 1, 0, 1, HTTP2_DISCONNECT_ON_INVALID_VALUE,
         HTTP2_PROTOCOL_ERROR},
        {"MAX_CONCURRENT_STREAMS", 0xffffffffu, 0, 0xffffffffu,
         HTTP2_DISCONNECT_ON_INVALID_VALUE, HTTP2_PROTOCOL_ERROR},
        {"INITIAL_WINDOW_SIZE", 65535, 0, 0x7fffffffu,
         HTTP2_DISCONNECT_ON_INVALID_VALUE,
         HTTP2_FLOW_CONTROL_ERROR},
        {"MAX_FRAME_SIZE", 16384, 16384, 16777215,
         HTTP2_DISCONNECT_ON_INVALID_VALUE, HTTP2_PROTOCOL_ERROR},
        {"MAX_HEADER_LIST_SIZE", MAX_MAX_HEADER_LIST_SIZE, 0,
         MAX_MAX_HEADER_LIST_SIZE, HTTP2_CLAMP_INVALID_VALUE,
         HTTP2_PROTOCOL_ERROR},
    };

http2_message_parser::http2_message_parser(bool is_client)
    : message_parser(is_client), _is_client(is_client)
{
    _recving_msg = nullptr;
    reset();
}

void http2_message_parser::reset()
{
    if (!_recving_msg)
        delete _recving_msg;
    _is_triggerred = false;
    _recving_msg = nullptr;
    _recving_msg_sid = 0;
    _last_req_sid = 1;
    _last_resp_sid = 2;
    _preface_sent = false;
    _preface_recved = false;
    _need_settings_ack_times = 0;
    _recv_bytes_since_last_window_update = 0;
    _max_recv_bytes_before_window_update = _send_bytes_since_last_window_update = 65535;

    if (_is_client)
        _preface_recved = true;

    memcpy(&_settings_parameters, &s_http2_settings_parameters, sizeof(s_http2_settings_parameters));
    _settings_frame_buffer.zero_all(sizeof(_settings_frame_buffer));
}

void http2_message_parser::trigger_protocol_running(std::queue<std::unique_ptr<message_ex> >& msgs)
{
    bool false_s = false;
    if (_is_triggerred.compare_exchange_strong(false_s, true, std::memory_order_release, std::memory_order_relaxed))
    {
        auto msg = message_ex::create_receive_message_with_standalone_header();
        msg->header->context.u.is_request = 1;
        msg->local_rpc_code = RPC_HTTP2_PROTOCOL_DRIVER;
        //0xdeadbeaf TBD
        msg->io_session_secret = 0xdeadbeef;
        msgs.emplace(std::unique_ptr<message_ex>(msg));
    }
}

message_ex* http2_message_parser::get_message_on_receive(message_reader* reader, /*out*/ int& read_next)
{
    read_next = 4096;

    // only used by server
    if (!_preface_recved && reader->length() >= HTTP2_CLIENT_CONNECT_STRLEN)
    {
        if (memcmp((const void*)reader->data(), HTTP2_CLIENT_CONNECT_STRING, HTTP2_CLIENT_CONNECT_STRLEN) == 0)
        {
            reader->consume(HTTP2_CLIENT_CONNECT_STRLEN);
            _preface_recved = true; 

            trigger_protocol_running(_received_messages);
        }
        else
        {
            derror ("invalid http2 peer from %s", "TODO: hostname:port @ http2");
            read_next = -1;
            return nullptr;
        }
    }

    auto err = ERR_OK;
    while (reader->length() >= FRAME_HDR_SIZE)
    {
        // check frame header 
        http2_frame* fm = (http2_frame*)reader->data();
        auto size = fm->length();

        // frame not ready
        if (reader->length() < size + FRAME_HDR_SIZE)
            break;

        uint32_t sid = fm->stream_id();
        http2_frame_type fmt = (http2_frame_type)fm->type;

        if (sid != 0)
        {
            if (_recving_msg_sid != sid)
            {
                auto it = _recving_messages.find(sid);
                if (it != _recving_messages.end())
                    _recving_msg = it->second.get();
                else
                {
                    _recving_msg = message_ex::create_receive_message_with_standalone_header();
                    _recving_msg_sid = sid;
                    _recving_messages.emplace(sid, std::unique_ptr<message_ex>(_recving_msg));
                }
            }
            else
            {
                dassert (_recving_msg, "");
            }
        }
        else
        {
            dassert (fmt == HTTP2_FRAME_SETTINGS ||
                     fmt == HTTP2_FRAME_WINDOW_UPDATE ||
                     fmt == HTTP2_FRAME_PING,
                     "unexpected frame %s with sid == 0", enum_to_string(fmt));
        }

        ddebug ("handle frame %s (sid = %d, flags = %x, len = %d)\r\n",
            enum_to_string(fmt),
            sid,
            (uint32_t)fm->flags,
            size
            );

        switch (fmt)
        {
            case HTTP2_FRAME_DATA:
                {
                    auto data = reader->range(0, reader->length());
                    err = handle_data_frame(fm, size, data);
                }
                break;
            case HTTP2_FRAME_HEADER:
                err = handle_header_frame(fm, size);
                break;
            case HTTP2_FRAME_RST_STREAM:
                err = handle_rst_frame(fm, size);
                break;
            case HTTP2_FRAME_SETTINGS:
                err = handle_settings_frame(fm, size);
                break;
            case HTTP2_FRAME_PING:
                err = handle_ping_frame(fm, size);
                break;
            case HTTP2_FRAME_GOAWAY:
                err = handle_goaway_frame(fm, size);
                break;
            case HTTP2_FRAME_WINDOW_UPDATE:
                err = handle_window_frame(fm, size);
                break;
            case HTTP2_FRAME_CONTINUATION:
                err = handle_continuation_frame(fm, size);
                break;
            default:
                derror("unsupported http2 frame type %d", fm->type);
                err = ERR_INVALID_DATA;
        }

        if (err != ERR_OK)
        {
            derror("invalid incoming http2 frame data, close socket");
            read_next = -1;
            return nullptr;
        }

        if (sid != 0 && (fm->flags & HTTP2_DATA_FLAG_END_STREAM))
        {
            if (!_recving_msg->header->context.u.is_request)
                delete_http2_stream(sid);

            auto it = _recving_messages.find(sid);
            _received_messages.emplace(std::move(it->second));
            _recving_messages.erase(it);
            _recving_msg = nullptr;
            _recving_msg_sid = 0;
        }

        reader->consume(size + FRAME_HDR_SIZE);

        if (_recv_bytes_since_last_window_update >= (uint32_t)(RECV_WINDOW_SCALE*(double_t)_max_recv_bytes_before_window_update))
            trigger_protocol_running(_received_messages);
    }

    if (!_received_messages.empty())
    {
        auto msg = std::move(_received_messages.front());
        _received_messages.pop();

        dinfo("rpc_name = %s, from_address = %s, seq_id = %u, trace_id = %016" PRIx64,
            msg->dheader.rpc_name, msg->from_address.to_string(),
            msg->header->id, msg->header->trace_id);
        return msg.release();
    }
    else
    {
        return nullptr;
    }
}

void http2_message_parser::prepare_on_send(message_ex *msg)
{
    // TODO: move get-buffers-on-send here to prompt performance 
}

int http2_message_parser::get_buffer_count_on_send(message_ex* msg)
{
    // for data 
    auto r = (int)msg->buffers.size();

    // maximum 
    //      connection preface
    //      HEADERS FRAME 
    //      Ping ACK
    //      Setting ACK
    //          msg->buffers
    //      [HEADERS(response)]
    //      [WINDOW_UPDATE]
    return r + 6;
}

void http2_message_parser::send_connection_preface(send_buf* buffers)
{
    char* ptr = (char*)_preface_buffer;
    
    // only client sents this string 
    if (_is_client)
    {
        // prepare buffer
        memcpy(ptr, HTTP2_CLIENT_CONNECT_STRING, HTTP2_CLIENT_CONNECT_STRLEN);
        ptr += HTTP2_CLIENT_CONNECT_STRLEN;
    }
    
    http2_frame fm;
    fm.zero_all(sizeof(fm));
    fm.type = HTTP2_FRAME_SETTINGS;
    memcpy(ptr, (const void*)&fm, FRAME_HDR_SIZE);
    ptr += FRAME_HDR_SIZE;

    buffers[0].buf = (void*)_preface_buffer;
    buffers[0].sz = (size_t)(ptr - _preface_buffer);
}

int http2_message_parser::get_buffers_on_send(message_ex* msg, send_buf* buffers)
{
    auto& msg_header = msg->header;
    auto& msg_buffers = msg->buffers;
    int i = 0;

    // prepare connection preface if necessary
    if (!_preface_sent)
    {
        send_connection_preface(buffers);
        ++i;
        _preface_sent = true;
    }

    // ping ack if necessary
    if (IS_NEED_PING_ACK)
    {
        uint8_t ping_ack_times = _need_ping_ack_opaque_vec.size();
        const int ping_ack_len = sizeof(http2_ping_frame) * ping_ack_times;
        std::shared_ptr<char> header_holder(static_cast<char *>(dsn_transient_malloc(ping_ack_len)), [](char *c) { dsn_transient_free(c); });
        memset(header_holder.get(),0,ping_ack_len);
        msg_buffers.emplace_back(blob(header_holder, 1));

        uint8_t index = 0;
        while (ping_ack_times --)
        {
            http2_ping_frame* ping_ack_frame_buffer = (http2_ping_frame*)header_holder.get() + index;
            ping_ack_frame_buffer->type = HTTP2_FRAME_PING;
            ping_ack_frame_buffer->flags |= HTTP2_FLAG_ACK;
            ping_ack_frame_buffer->set_length(8);
            ping_ack_frame_buffer->ping_opaque_data = _need_ping_ack_opaque_vec[index ++];
        }
        _need_ping_ack_opaque_vec.clear();

        buffers[i].buf = (void *) header_holder.get();
        buffers[i].sz = ping_ack_len;
        ++i;
    }

    // settings ack if necessary
    if (IS_NEED_SETTINGS_ACK)
    {
        _settings_frame_buffer.type = HTTP2_FRAME_SETTINGS;
        _settings_frame_buffer.flags = HTTP2_FLAG_ACK;

        while (_need_settings_ack_times --)
        {
            buffers[i].buf = (void *) &_settings_frame_buffer;
            buffers[i].sz = sizeof(_settings_frame_buffer);
            ++i;
        }
        _need_settings_ack_times = 0;
    }

    // window update if necessary
    if (_recv_bytes_since_last_window_update >= (uint32_t)(RECV_WINDOW_SCALE * (double_t)_max_recv_bytes_before_window_update))
    {
        int window_update_num = 1;
        for (auto& s_info : _streams_info)
            if (s_info.second.get()->recv_bytes_for_stream > 0)
                window_update_num ++;

        const int32_t window_size = sizeof(http2_window_update_frame) * window_update_num;
        std::shared_ptr<char> header_holder(static_cast<char *>(dsn_transient_malloc(window_size)), [](char *c) { dsn_transient_free(c); });
        msg_buffers.emplace_back(blob(header_holder, 1));
        memset(header_holder.get(),0,window_size);

        http2_window_update_frame* window_update_frame_for_transport = (http2_window_update_frame*)header_holder.get();
        window_update_frame_for_transport->type = HTTP2_FRAME_WINDOW_UPDATE;
        window_update_frame_for_transport->set_length(sizeof(uint32_t));
        window_update_frame_for_transport->set_window_size_increment(_recv_bytes_since_last_window_update);
        _recv_bytes_since_last_window_update = 0;

        uint8_t index = 1;
        for (auto& s_info : _streams_info)
            if (s_info.second.get()->recv_bytes_for_stream > 0)
            {
                http2_window_update_frame* window_update_frame_for_stream = (http2_window_update_frame*)header_holder.get() + index++;
                window_update_frame_for_stream->type = HTTP2_FRAME_WINDOW_UPDATE;
                window_update_frame_for_stream->set_length(sizeof(uint32_t));
                window_update_frame_for_stream->set_window_size_increment(s_info.second.get()->recv_bytes_for_stream);
                window_update_frame_for_stream->set_stream_id(s_info.first);
                s_info.second.get()->recv_bytes_for_stream = 0;
            }

        buffers[i].buf = (void *) header_holder.get();
        buffers[i].sz = window_size;
        ++i;
    }

    // we don't really need to send the ack of this msg to 
    if (RPC_HTTP2_PROTOCOL_DRIVER_ACK == msg->local_rpc_code)
    {
        _is_triggerred = false;
        return i;
    }

    //
    // grpc unary msg framing format 
    //

    /*
    HEADERS (flags = END_HEADERS)
        :status = 200
        grpc-encoding = gzip

    DATA
        <Delimited Message>

   // response 
    HEADERS (flags = END_STREAM, END_HEADERS)
        grpc-status = 0 # OK
        trace-proto-bin = jher831yy13JHy3hc
    DATA 
    HEADERS
    */

    uint32_t sid = 0;
    auto buffer_body_len = msg_header->body_length;
    auto it = _mapped_between_mid_with_sid.find(msg_header->id);
    if (it != _mapped_between_mid_with_sid.end())
    {
        sid = it->second;
        auto it_stream = _streams_info.find(it->second);
        if(it_stream != _streams_info.end())
            _h2_cur_stream = it_stream->second.get();
    }
    else
    {
        if (msg_header->context.u.is_request)
        {
            sid = _last_req_sid;
            _last_req_sid += 2;
        }
        else
        {
            if (!strcmp(msg->dheader.rpc_name,"RPC_NETWORK_PING_ACK"))
            {
                sid = _last_resp_sid;
                _last_resp_sid += 2;
            }
            else
                sid = msg_header->id;
        }

        create_http2_stream(msg_header->id,sid);
    }

    auto max_sending_for_stream = MIN(_h2_cur_stream->send_bytes_delta_for_stream +
                                 _settings_parameters[HTTP2_SETTINGS_INITIAL_WINDOW_SIZE].default_value,
                                 _send_bytes_since_last_window_update);
    bool is_first_send = !_h2_cur_stream->pms.buffer_index & !_h2_cur_stream->pms.buffer_offset;
    if (is_first_send)
    {
        // prepare http headers
        std::vector<header> headers;
        encode_headers(headers, msg);

        const int header_len = FRAME_HDR_SIZE + 512; // large enough to hold our header
        std::shared_ptr<char> header_holder(static_cast<char*>(dsn_transient_malloc(header_len)), [](char* c) {dsn_transient_free(c);});
        msg_buffers.emplace_back(blob(header_holder, 1));

        // prepare HEADER frame
        auto hdr = (http2_header_frame*)header_holder.get();
        hdr->zero_all(sizeof(http2_header_frame));
        hdr->type = HTTP2_FRAME_HEADER;
        hdr->flags |= HTTP2_DATA_FLAG_END_HEADERS;
        hdr->set_stream_id(sid);
        uint32_t len = (uint32_t)hpack_encode((uint8_t*)hdr->headers_ptr(), headers, false, false, true, &_hpack_table, -1);
        hdr->set_length(len);

# ifndef NDEBUG
        dassert (hdr->length() == len, "");
        dassert (hdr->stream_id() == sid, "");
# endif

        buffers[i].buf = header_holder.get();
        buffers[i].sz = FRAME_HDR_SIZE + len;
        ++i;


        //erase the header in msg
        unsigned int offset = sizeof(message_header);
        int dsn_buf_count = 0;
        while (dsn_buf_count < msg_buffers.size())
        {
            blob& buf = msg_buffers[dsn_buf_count];

            if (offset >= buf.length())
            {
                offset -= buf.length();
                ++dsn_buf_count;
                continue;
            }
            break;
        }

        _h2_cur_stream->pms.buffer_index = dsn_buf_count;
        _h2_cur_stream->pms.buffer_offset = offset;
    }

    if ( buffer_body_len > 0 && ((_h2_cur_stream->is_first_data_frame && max_sending_for_stream > 5) || (!_h2_cur_stream->is_first_data_frame && max_sending_for_stream)) )
    {
        //data
        auto max_sending_len = MIN(_settings_parameters[HTTP2_SETTINGS_MAX_FRAME_SIZE].default_value,max_sending_for_stream);

        // prepare DATA frame (without data)
        const int header_len = FRAME_HDR_SIZE + 512; // large enough to hold our header
        std::shared_ptr<char> header_holder(static_cast<char*>(dsn_transient_malloc(header_len)), [](char* c) {dsn_transient_free(c);});
        msg_buffers.emplace_back(blob(header_holder, 1));

        auto data = (http2_data_frame*)header_holder.get();
        data->zero_all(sizeof(http2_data_frame));
        data->type = HTTP2_FRAME_DATA;
        data->set_stream_id(sid);

        if (_h2_cur_stream->is_first_data_frame)
        {
            auto data_msg = (grpc_delimited_msg*)data->msg_ptr();
            data_msg->zero_all();
            data_msg->set_msg_length(buffer_body_len);
            max_sending_len -= 5;
        }

        auto data_sending_len = MIN(max_sending_len,buffer_body_len);
        data->set_length(data_sending_len + (_h2_cur_stream->is_first_data_frame?5:0));

# ifndef NDEBUG
        dassert (data->length() == data_sending_len + (_h2_cur_stream->is_first_data_frame?5:0), "");
        dassert (data->stream_id() == sid, "");
# endif

        buffers[i].buf = header_holder.get();
        buffers[i].sz = FRAME_HDR_SIZE + (_h2_cur_stream->is_first_data_frame ? 5:0);
        ++i;

        // attach data
        _send_bytes_since_last_window_update -= (data_sending_len + (_h2_cur_stream->is_first_data_frame?5:0));
        _h2_cur_stream->send_bytes_delta_for_stream -= (data_sending_len + (_h2_cur_stream->is_first_data_frame?5:0));

        buffer_body_len -= data_sending_len;
        msg_header->body_length = buffer_body_len;
        while (data_sending_len > 0)
        {
            auto cur_buffer_len = msg_buffers[_h2_cur_stream->pms.buffer_index].length() - _h2_cur_stream->pms.buffer_offset;

            if (cur_buffer_len <= data_sending_len)
            {
                buffers[i].buf = (void *)(msg_buffers[_h2_cur_stream->pms.buffer_index].data() + _h2_cur_stream->pms.buffer_offset);
                buffers[i].sz = cur_buffer_len;
                i ++;

                data_sending_len -= cur_buffer_len;
                _h2_cur_stream->pms.buffer_index ++;
                _h2_cur_stream->pms.buffer_offset = 0;
            }
            else
            {
                buffers[i].buf = (void*)(msg_buffers[_h2_cur_stream->pms.buffer_index].data() + _h2_cur_stream->pms.buffer_offset);
                buffers[i].sz = data_sending_len;
                i ++;

                _h2_cur_stream->pms.buffer_offset += data_sending_len;
                data_sending_len = 0;
            }
        }

        if (!buffer_body_len)
        {
            // request
            if (msg_header->context.u.is_request)
            {
                data->flags |= HTTP2_DATA_FLAG_END_STREAM;
            }
        }
        else
            _h2_cur_stream->is_first_data_frame = false;
    }

    if (!buffer_body_len && !msg->header->context.u.is_request)
    {
        delete_http2_stream(sid);

        // prepare http headers
        std::vector<header> headers;
        // prepare another headers block for response msg
        headers.emplace_back(header("grpc-status", "0"));

        const int header_len = FRAME_HDR_SIZE + 512; // large enough to hold our header
        std::shared_ptr<char> header_holder(static_cast<char*>(dsn_transient_malloc(header_len)), [](char* c) {dsn_transient_free(c);});
        msg_buffers.emplace_back(blob(header_holder, 1));

        auto hdr = (http2_header_frame*)header_holder.get();
        hdr->zero_all(sizeof(http2_header_frame));
        hdr->type = HTTP2_FRAME_HEADER;
        hdr->flags |= HTTP2_DATA_FLAG_END_HEADERS;
        hdr->flags |= HTTP2_DATA_FLAG_END_STREAM;
        hdr->set_stream_id(sid);
        auto len = (uint32_t)hpack_encode((uint8_t*)hdr->headers_ptr(), headers, false, false, true, &_hpack_table, -1);
        hdr->set_length(len);

        // add header frame + data frame header 
        buffers[i].buf = header_holder.get();
        buffers[i].sz = FRAME_HDR_SIZE + len;
        ++i;
    }
    return i;
}

}
