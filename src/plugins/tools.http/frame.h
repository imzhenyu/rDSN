/*
* Description:
*     http message parser based on grpc parser impl`
*
* Revision history:
*     Dec. 26, 2016, Zhenyu Guo, first version
*     xxxx-xx-xx, author, fix bug about xxx
*/

#pragma once

# include <dsn/tool-api/message_parser.h>
# include <dsn/service_api_cpp.h>
# include <dsn/utility/enum_helper.h>

namespace dsn 
{
    enum http2_frame_type 
    {
        HTTP2_FRAME_DATA = 0,
        HTTP2_FRAME_HEADER = 1,
        HTTP2_FRAME_RST_STREAM = 3,
        HTTP2_FRAME_SETTINGS = 4,
        HTTP2_FRAME_PING = 6,
        HTTP2_FRAME_GOAWAY = 7,
        HTTP2_FRAME_WINDOW_UPDATE = 8,    
        HTTP2_FRAME_CONTINUATION = 9,

        HTTP2_FRAME_INVALID = 15
    };

    ENUM_BEGIN(http2_frame_type, HTTP2_FRAME_INVALID)
        ENUM_REG(HTTP2_FRAME_DATA)
        ENUM_REG(HTTP2_FRAME_HEADER)
        ENUM_REG(HTTP2_FRAME_RST_STREAM)
        ENUM_REG(HTTP2_FRAME_SETTINGS)
        ENUM_REG(HTTP2_FRAME_PING)
        ENUM_REG(HTTP2_FRAME_GOAWAY)
        ENUM_REG(HTTP2_FRAME_WINDOW_UPDATE)
        ENUM_REG(HTTP2_FRAME_CONTINUATION)
    ENUM_END(http2_frame_type)

    enum http2_flag
    {
        HTTP2_DATA_FLAG_END_STREAM = 1,
        HTTP2_FLAG_ACK = 1,
        HTTP2_DATA_FLAG_END_HEADERS = 4,
        HTTP2_DATA_FLAG_PADDED = 8,
        HTTP2_FLAG_HAS_PRIORITY = 0x20
    };

    /* error codes for RST_STREAM from http2 draft 14 section 7 */
    typedef enum
    {
        HTTP2_NO_ERROR = 0x0,
        HTTP2_PROTOCOL_ERROR = 0x1,
        HTTP2_INTERNAL_ERROR = 0x2,
        HTTP2_FLOW_CONTROL_ERROR = 0x3,
        HTTP2_SETTINGS_TIMEOUT = 0x4,
        HTTP2_STREAM_CLOSED = 0x5,
        HTTP2_FRAME_SIZE_ERROR = 0x6,
        HTTP2_REFUSED_STREAM = 0x7,
        HTTP2_CANCEL = 0x8,
        HTTP2_COMPRESSION_ERROR = 0x9,
        HTTP2_CONNECT_ERROR = 0xa,
        HTTP2_ENHANCE_YOUR_CALM = 0xb,
        HTTP2_INADEQUATE_SECURITY = 0xc,
        /* force use of a default clause */
        HTTP2__ERROR_DO_NOT_USE = -1
    } http2_error_code;

    # pragma pack(push, 1)
    struct http2_frame
    {
        unsigned int len : 24;
        unsigned int type : 8;
        unsigned int flags : 8;
        unsigned int r_sid : 32;

        void zero_all(size_t size)
        {
            memset(this, 0, size);
        }

        uint32_t length() const 
        {
            uint32_t len0 = len;
            return DSN_SWAP24(len0);
        }

        void set_length(uint32_t size)
        {
            len = DSN_SWAP24(size);
        }

        uint32_t stream_id() const 
        {
            uint32_t sid0 = r_sid & 0xffffff7f;
            return DSN_SWAP32(sid0);
        }

        void set_stream_id(uint32_t sid0)
        {
            uint32_t sid = ((r_sid & 0x80) << 24) | (sid0 & 0x7fffffff);
            r_sid = DSN_SWAP32(sid);
        }
    };

    # define FRAME_HDR_SIZE sizeof(http2_frame)

    //------------- HTTP2_FRAME_SETTINGS ------------------

    struct settings_entry
    {
        uint16_t id_;
        uint32_t value_;

        uint16_t id() const 
        {
            return DSN_SWAP16(id_);
        }

        uint32_t value() const 
        {
            return DSN_SWAP32(value_);
        }
    };

    struct http2_settins_frame : http2_frame
    {
        settings_entry entries[0];
        uint32_t entry_count() const 
        {
            return this->length() / 6;
        }
    };

    enum http2_setting_id
    {
        HTTP2_SETTINGS_HEADER_TABLE_SIZE = 1,
        HTTP2_SETTINGS_ENABLE_PUSH = 2,
        HTTP2_SETTINGS_MAX_CONCURRENT_STREAMS = 3,
        HTTP2_SETTINGS_INITIAL_WINDOW_SIZE = 4,
        HTTP2_SETTINGS_MAX_FRAME_SIZE = 5,
        HTTP2_SETTINGS_MAX_HEADER_LIST_SIZE = 6,
//        HTTP2_SETTINGS_GRPC_ALLOW_TRUE_BINARY_METADATA = 7,
        HTTP2_NUM_SETTINGS
    };

    typedef enum 
    {
        HTTP2_CLAMP_INVALID_VALUE,
        HTTP2_DISCONNECT_ON_INVALID_VALUE
    } http2_invalid_value_behavior;

    typedef struct {
        const char *name;
        uint32_t default_value;
        uint32_t min_value;
        uint32_t max_value;
        http2_invalid_value_behavior invalid_value_behavior;
        uint32_t error_value;
    } http2_setting_parameters;

    //------------- HTTP2_FRAME_WINDOW_UPDATE ------------------

    struct http2_window_update_frame : http2_frame
    {
        unsigned int r_window_size_incre; // : 32;

        uint32_t window_size_increment() const 
        {
            //uint32_t sid0 = r_window_size_incre & 0xffffff7f;
            //return DSN_SWAP32(sid0);
            return DSN_SWAP32(r_window_size_incre);
        }

        void set_window_size_increment(uint32_t sid0)
        {
            //uint32_t sid = ((r_window_size_incre & 0x80) << 24) | (sid0 & 0x7fffffff);
            //r_window_size_incre = DSN_SWAP32(sid);
            r_window_size_incre = DSN_SWAP32(sid0);
        }
    };

    //------------- HTTP2_FRAME_DATA ------------------

    struct grpc_delimited_msg
    {
        unsigned int compressed : 8;
        unsigned int len : 32;
        char msg[0];

        void zero_all()
        {
            compressed = 0;
            len = 0;
        }

        uint32_t msg_offset() const 
        {
            return 5;
        }

        uint32_t msg_length() const 
        {
            uint32_t sid0 = len;
            return DSN_SWAP32(sid0);
        }

        void set_msg_length(uint32_t sid)
        {
            len = DSN_SWAP32(sid);
        }
    };

    struct http2_data_frame : http2_frame
    {
        unsigned int pad_length : 8; // when HTTP2_DATA_FLAG_PADDED
        char data[0];

        uint32_t data_length()
        {
            if (flags & HTTP2_DATA_FLAG_PADDED)
                return this->length() - pad_length - 1;
            else
                return this->length();
        }

        uint32_t data_start_offset() const
        {
            return (flags & HTTP2_DATA_FLAG_PADDED) ? (FRAME_HDR_SIZE + 1) : FRAME_HDR_SIZE;
        }

        grpc_delimited_msg* msg_ptr() const 
        {
            return (grpc_delimited_msg*)((flags & HTTP2_DATA_FLAG_PADDED) ? (char*)&data[0] : ((char*)this + FRAME_HDR_SIZE));
        }

        void set_pad_length(uint8_t size)
        {
            flags |= HTTP2_DATA_FLAG_PADDED;
            pad_length = size;
        }
    };

    //------------- HTTP2_FRAME_HEADER ------------------
    struct http2_header_frame : http2_frame
    {
        unsigned int pad_length : 8;  // when HTTP2_DATA_FLAG_PADDED
        unsigned int e_stream_dependency : 32;  // when HTTP2_FLAG_HAS_PRIORITY
        unsigned int weight : 8;  // when HTTP2_FLAG_HAS_PRIORITY
        char segments[0];
        
        uint32_t headers_length() const
        {
            auto r = length();
            if (flags & HTTP2_DATA_FLAG_PADDED)
                r -= pad_length + 1;
            if (flags & HTTP2_FLAG_HAS_PRIORITY)
                r -= 4 + 1;
            return r;
        }

        char* headers_ptr() const 
        {
            char* ptr = (char*)this + FRAME_HDR_SIZE;
            if (flags & HTTP2_DATA_FLAG_PADDED)
                ptr += pad_length + 1;
            if (flags & HTTP2_FLAG_HAS_PRIORITY)
                ptr += 4 + 1;
            return ptr;
        }

        void set_pad_length(uint8_t size)
        {
            flags |= HTTP2_DATA_FLAG_PADDED;
            pad_length = size;
        }
    };

    //------------- HTTP2_FRAME_PING ------------------
    struct http2_ping_frame : http2_frame
    {
        int64_t ping_opaque_data;
    };

    //------------- HTTP2_FRAME_RST ------------------
    struct http2_rst_frame : http2_frame
    {
        uint32_t error_code;

        uint32_t error_code_value()
        {
            uint32_t ec = error_code;
            return DSN_SWAP32(ec);
        }

    };
    # pragma pack(pop)
}
