/*
* Description:
*     http message parser based on grpc parser impl`
*
* Revision history:
*     Dec. 26, 2016, Zhenyu Guo, first version
*     xxxx-xx-xx, author, fix bug about xxx
*/

#pragma once

# include <dsn/tool-api/rpc_message.h>
# include <dsn/tool-api/message_parser.h>
# include <dsn/service_api_cpp.h>
# include <vector>
# include <queue>
# include "frame.h"
# include "hpack_table.h"

#define HTTP2_CLIENT_CONNECT_STRING "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"
#define HTTP2_CLIENT_CONNECT_STRLEN (sizeof(HTTP2_CLIENT_CONNECT_STRING) - 1)
#define IS_NEED_PING_ACK            (_need_ping_ack_opaque_vec.size() > 0)
#define IS_NEED_SETTINGS_ACK        (_need_settings_ack_times > 0)
#define MIN(a, b)                   ((a) > (b) ? (b) : (a))
#define RECV_WINDOW_SCALE           (1.0/2)

namespace dsn
{
    //DEFINE_NET_PROTOCOL(NET_HDR_HTTP2)
    DEFINE_TASK_CODE_RPC(RPC_HTTP2_PROTOCOL_DRIVER, TASK_PRIORITY_HIGH, THREAD_POOL_DEFAULT)

    class http2_stream
    {
        typedef struct
        {
            uint32_t                    buffer_index;
            uint32_t                    buffer_offset;
            uint32_t                    msg_id;
        }progress_msg_sending;

    public:
        uint32_t recv_bytes_for_stream;
        int32_t send_bytes_delta_for_stream;
        bool    is_first_data_frame;

        progress_msg_sending pms;

        http2_stream()
        {
            recv_bytes_for_stream = 0;
            send_bytes_delta_for_stream = 0;
            is_first_data_frame = true;

            memset(&pms,0, sizeof(progress_msg_sending));
        }
    };

    class http2_message_parser : public message_parser
    {
    public:
        http2_message_parser(bool is_client);
        virtual ~http2_message_parser() {}

        virtual void reset() override;

        virtual message_ex* get_message_on_receive(message_reader* reader, /*out*/ int& read_next) override;

        virtual void prepare_on_send(message_ex *msg) override;

        virtual int get_buffer_count_on_send(message_ex* msg) override;

        virtual int get_buffers_on_send(message_ex* msg, /*out*/ send_buf* buffers) override;

    private:
        error_code handle_data_frame(http2_frame* fm, uint32_t size, ::dsn::blob& buffer);

        error_code handle_header_frame(http2_frame* fm, uint32_t size);

        error_code handle_rst_frame(http2_frame* fm, uint32_t size);

        error_code handle_settings_frame(http2_frame* fm, uint32_t size);

        error_code handle_ping_frame(http2_frame* fm, uint32_t size);

        error_code handle_goaway_frame(http2_frame* fm, uint32_t size);

        error_code handle_window_frame(http2_frame* fm, uint32_t size);

        error_code handle_continuation_frame(http2_frame* fm, uint32_t size);

        void send_connection_preface(send_buf* buffers);

        int  encode_headers(std::vector<header>& headers, message_ex* msg);

        int  decode_headers(const std::vector<header>& headers, message_ex* msg);

        void trigger_protocol_running(std::queue<std::unique_ptr<message_ex> >& msgs);

    private:
        uint32_t _last_req_sid; // assigned stream id for sending
        uint32_t _last_resp_sid;
        bool     _is_client; 
        bool     _preface_sent;
        bool     _preface_recved;

        std::vector<int64_t> _need_ping_ack_opaque_vec;
        uint8_t _need_settings_ack_times;
        http2_settins_frame       _settings_frame_buffer;

        char                      _preface_buffer[HTTP2_CLIENT_CONNECT_STRLEN + FRAME_HDR_SIZE];
        http2_setting_parameters _settings_parameters[HTTP2_NUM_SETTINGS];
        Table                    _hpack_table;

        // streams as messages, which are segmented as frames, so we may receive another  msg 
        // even when the curent recving msg is not finished 
        std::unordered_map<uint32_t, std::unique_ptr<message_ex> > _recving_messages;
        message_ex*                                                _recving_msg;
        uint32_t                                                   _recving_msg_sid;

        // http2 requires window update for flow control 
        std::atomic<bool>                                          _is_triggerred;
        uint32_t                                                   _recv_bytes_since_last_window_update;
        uint32_t                                                   _send_bytes_since_last_window_update;
        uint32_t                                                   _max_recv_bytes_before_window_update;

        std::queue<std::unique_ptr<message_ex> > _received_messages;

    private:
        //due to limitation of flow control

        std::unordered_map<uint32_t, std::unique_ptr<http2_stream> > _streams_info;
        http2_stream* _h2_cur_stream;
        std::unordered_map<uint32_t, uint32_t > _mapped_between_mid_with_sid;

    private:
        void delete_http2_stream(uint32_t sid);

        void create_http2_stream(uint32_t mid, uint32_t sid);
    };
}
