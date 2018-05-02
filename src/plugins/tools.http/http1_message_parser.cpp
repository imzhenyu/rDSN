/*
* The MIT License (MIT)
*
* Copyright (c) 2015 Microsoft Corporation
*
* -=- Robust Distributed System Nucleus (rDSN) -=-
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/

/*
* Description:
*     message parser for browser-generated http request
*
* Revision history:
*     Feb. 2016, Tianyi Wang, first version
*     Jun. 2016, Zuoyan Qin, second version
*     xxxx-xx-xx, author, fix bug about xxx
*/

# include <dsn/utility/ports.h>
# include <dsn/tool-api/rpc_message.h>
# include <dsn/utility/singleton.h>
# include <vector>
# include <iomanip>
# include "http1_message_parser.h"

# ifdef __TITLE__
# undef __TITLE__
# endif
# define __TITLE__ "http1.message.parser"

namespace dsn{

template <typename T, size_t N>
char(&ArraySizeHelper(T(&array)[N]))[N];

http1_message_parser::http1_message_parser(bool is_client)
    : message_parser(is_client), _body_received(0)
{
    memset(&_parser_setting, 0, sizeof(_parser_setting));
    _parser.data = this;
    _parser_setting.on_message_begin = [](http_parser* parser)->int
    {
        auto owner = static_cast<http1_message_parser*>(parser->data);

        owner->_current_message.reset(message_ex::create_receive_message_with_standalone_header());
        owner->_response_parse_state = parsing_nothing;

        message_header* header = owner->_current_message->header;
        header->fix_hdr_length = sizeof(message_header);
        return 0;
    };
    _parser_setting.on_url = [](http_parser* parser, const char *at, size_t length)->int
    {
        // url = "/" + service_name + "/" + rpc_method_name;
        // e.g., /service-name/cli
        //service_name optional and rpc_name is must
        std::string url(at, length);
        std::vector<std::string> args;
        utils::split_args(url.c_str(), args, '/');

        dinfo("http call %s", url.c_str());
        auto owner = static_cast<http1_message_parser*>(parser->data);
        if(args.size() == 1) {           
            owner->_current_message->local_rpc_code = dsn_task_code_from_string(args[0].data(), TASK_CODE_INVALID);
        }
        else if(args.size() == 2) {
            owner->_current_message->dheader.service_name = args[0].data();
            owner->_current_message->local_rpc_code = dsn_task_code_from_string(args[1].data(), TASK_CODE_INVALID);
        }
        else{
            dinfo("skip url parse for %s, could be done in headers if not cross-domain", url.c_str());
            return 0;
        }
        owner->_current_message->dheader.rpc_name = task_spec::get(owner->_current_message->local_rpc_code)->name.c_str();  
        return 0;
    };
    _parser_setting.on_header_field = [](http_parser* parser, const char *at, size_t length)->int
    {
#define StrLiteralLen(str) (sizeof(ArraySizeHelper(str)) - 1)
#define MATCH(pat) (length >= StrLiteralLen(pat) && strncmp(at, pat, StrLiteralLen(pat)) == 0)
        auto owner = static_cast<http1_message_parser*>(parser->data);
        if (MATCH("id"))
        {
            owner->_response_parse_state = parsing_id;
        }
        else if (MATCH("trace_id"))
        {
            owner->_response_parse_state = parsing_trace_id;
        }
        else if (MATCH("service_name"))
        {
            owner->_response_parse_state = parsing_service_name;
        }
        else if (MATCH("rpc_name"))
        {
            owner->_response_parse_state = parsing_rpc_name;
        }
        else if (MATCH("app_id"))
        {
            owner->_response_parse_state = parsing_app_id;
        }
        else if (MATCH("partition_index"))
        {
            owner->_response_parse_state = parsing_partition_index;
        }
        else if (MATCH("serialize_format"))
        {
            owner->_response_parse_state = parsing_serialize_format;
        }
        else if (MATCH("client_thread_hash"))
        {
            owner->_response_parse_state = parsing_client_thread_hash;
        }
        else if (MATCH("server_error"))
        {
            owner->_response_parse_state = parsing_server_error;
        }
        else if (MATCH("Content-Length"))
        {
            owner->_response_parse_state = parsing_content_length;
        }
        return 0;
#undef StrLiteralLen
#undef MATCH
    };
    _parser_setting.on_header_value = [](http_parser* parser, const char *at, size_t length)->int
    {
        auto owner = static_cast<http1_message_parser*>(parser->data);
        message_header* header = owner->_current_message->header;
        switch(owner->_response_parse_state)
        {
        case parsing_id:
        {
            char *end;
            header->id = std::strtoull(at, &end, 10);
            if (end != at + length)
            {
                derror("invalid header.id '%.*s'", length, at);
                return 1;
            }
            break;
        }
        case parsing_trace_id:
        {
            char *end;
            header->trace_id = std::strtoull(at, &end, 10);
            if (end != at + length)
            {
                derror("invalid header.trace_id '%.*s'", length, at);
                return 1;
            }
            break;
        }
        case parsing_service_name:
        {
            char* buffer = (char*)alloca(length + 1);
            buffer[length] = '\0';
            strncpy(buffer, at, length);
            owner->_current_message->dheader.service_name = buffer;
            break;
        }
        case parsing_rpc_name:
        {
            char* buffer = (char*)alloca(length + 1);
            buffer[length] = '\0';
            strncpy(buffer, at, length);
            owner->_current_message->local_rpc_code = dsn_task_code_from_string(buffer, TASK_CODE_INVALID);
            owner->_current_message->dheader.rpc_name = task_spec::get(owner->_current_message->local_rpc_code)->name.c_str();
            break;
        }
        case parsing_app_id:
        {
            char *end;
            header->context.u.app_id = std::strtol(at, &end, 10);
            if (end != at + length)
            {
                derror("invalid header.app_id '%.*s'", length, at);
                return 1;
            }
            break;
        }
        case parsing_partition_index:
        {
            char *end;
            header->context.u.partition_index = std::strtol(at, &end, 10);
            if (end != at + length)
            {
                derror("invalid header.partition_index '%.*s'", length, at);
                return 1;
            }
            break;
        }
        case parsing_serialize_format:
        {
            dsn_msg_serialize_format fmt = enum_from_string(std::string(at, length).c_str(), DSF_INVALID);
            if (fmt == DSF_INVALID)
            {
                derror("invalid header.serialize_format '%.*s'", length, at);
                return 1;
            }
            header->context.u.serialize_format = fmt;
            break;
        }
        case parsing_client_thread_hash:
        {
            char *end;
            header->client.thread_hash = std::strtol(at, &end, 10);
            if (end != at + length)
            {
                derror("invalid header.client_thread_hash '%.*s'", length, at);
                return 1;
            }
            break;
        }
        case parsing_server_error:
        {
            char *end;
            header->context.u.server_error = std::strtol(at, &end, 10);
            if (end != at + length)
            {
                derror("invalid header.client_thread_hash '%.*s'", length, at);
                return 1;
            }
            break;
        }
        case parsing_content_length:
        {
            char *end;
            header->body_length = std::strtol(at, &end, 10);
            if (end != at + length)
            {
                derror("invalid Content-Length '%.*s'", length, at);
                return 1;
            }
            break; 
        }
        case parsing_nothing:
            ;
            //no default
        }

        owner->_response_parse_state = parsing_nothing;
        return 0;
    };
    _parser_setting.on_headers_complete = [](http_parser* parser)->int
    {
        auto owner = static_cast<http1_message_parser*>(parser->data);
        message_header* header = owner->_current_message->header;
        if (parser->type == HTTP_REQUEST && parser->method == HTTP_GET)
        {
            header->hdr_type = *(uint32_t*)"GET ";
            header->context.u.is_request = 1;
        }
        else if (parser->type == HTTP_REQUEST && parser->method == HTTP_POST)
        {
            header->hdr_type = *(uint32_t*)"POST";
            header->context.u.is_request = 1;
        }
        else if (parser->type == HTTP_REQUEST && parser->method == HTTP_OPTIONS)
        {
            header->hdr_type = *(uint32_t*)"OPTI";
            header->context.u.is_request = 1;
        }        
        else if (parser->type == HTTP_RESPONSE)
        {
            header->hdr_type = *(uint32_t*)"HTTP";
            header->context.u.is_request = 0;
        }        
        else
        {
            derror("invalid http type %d and method %d", parser->type, parser->method);
            return 1;
        }

        if (header->body_length == 0)
        {
            auto owner = static_cast<http1_message_parser*>(parser->data);
            owner->_current_message->buffers.emplace_back(blob());
            owner->_received_messages.emplace(std::move(owner->_current_message));
        }

        return 0;
    };
    _parser_setting.on_body = [](http_parser* parser, const char *at, size_t length)->int
    {
        auto owner = static_cast<http1_message_parser*>(parser->data);
        auto body_length = owner->_current_message->header->body_length;

        if (owner->_body_received + length > body_length)
        {
            // something error
            return 1;
        }

        dassert(owner->_current_buffer.buffer() != nullptr, "the read buffer is not owning");
        owner->_current_message->buffers.emplace_back(blob(owner->_current_buffer.buffer(), at - owner->_current_buffer.buffer_ptr(), length));
        owner->_body_received += length;
        return 0;
    };
    _parser_setting.on_message_complete = [](http_parser* parser)->int
    {
        auto owner = static_cast<http1_message_parser*>(parser->data);
        auto body_length = owner->_current_message->header->body_length;
        dassert(owner->_body_received == body_length, "length of received not equals to body_length");

        owner->_received_messages.emplace(std::move(owner->_current_message));
        owner->_body_received = 0;
        return 0;
    };

    http_parser_init(&_parser, HTTP_BOTH);
}

void http1_message_parser::reset()
{
    _body_received = 0;
    _current_message->buffers.clear();
    http_parser_init(&_parser, HTTP_BOTH);
}

message_ex* http1_message_parser::get_message_on_receive(message_reader* reader, /*out*/ int& read_next)
{
    read_next = 4096;

    if (HTTP_PARSER_ERRNO(&_parser) != HPE_OK)
    {
        reset();
    }

    auto sz = _received_messages.size();
    while (reader->length() > 0)
    {
        _current_buffer = reader->range(reader->length());
        auto nparsed = http_parser_execute(&_parser, &_parser_setting, reader->data(), reader->length());
        if (HTTP_PARSER_ERRNO(&_parser) != HPE_OK)
        {
            derror("state = %u, read = %u, parsed = %zu, method = %s, error = %s",
                   _parser.state, _parser.nread, nparsed,
                   http_method_str((http_method)_parser.method),
                   http_errno_description((http_errno)_parser.http_errno));

            // session is broken so make read_next = -1, which is useful to external caller. eg. shutdown the channel
            read_next = -1;
            return nullptr;
        }

        reader->consume(nparsed);
        _current_buffer = blob();

        // messages received
        if (_received_messages.size() > sz)
        {
            if (_parser.upgrade)
            {
                derror("unsupported http protocol");
                read_next = -1;
                return nullptr;
            }
        }
        else
            break;
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

void http1_message_parser::prepare_on_send(message_ex *msg)
{
    
}

int http1_message_parser::get_buffer_count_on_send(message_ex* msg)
{
    return (int)msg->buffers.size() + 1;
}

int http1_message_parser::get_buffers_on_send(message_ex* msg, send_buf* buffers)
{
    auto& header = msg->header;

    // construct http header blob
    std::string header_str;
    if (header->context.u.is_request)
    {
        std::stringstream ss;
        ss << "POST /" << msg->dheader.service_name << "/" << msg->dheader.rpc_name << " HTTP/1.1\r\n";
        ss << "Host: " << dsn_address_to_string(dsn_primary_address()) << "\r\n";//TODO get local ip
//        ss << "Content-Type: text/plain\r\n";
        ss << "Content-Type: application/x-www-form-urlencoded\r\n";
        ss << "id: " << header->id << "\r\n";
        ss << "trace_id: " << header->trace_id << "\r\n";
        ss << "service_name: " << msg->dheader.service_name << "\r\n";
        ss << "rpc_name: " << msg->dheader.rpc_name << "\r\n";
        ss << "app_id: " << header->context.u.app_id << "\r\n";
        ss << "partition_index: " << header->context.u.partition_index << "\r\n";
        ss << "serialize_format: " << enum_to_string((dsn_msg_serialize_format)header->context.u.serialize_format) << "\r\n";
        ss << "client_timeout: " << header->client.timeout_ms << "\r\n";
        ss << "client_thread_hash: " << header->client.thread_hash << "\r\n";
        ss << "Content-Length: " << msg->body_size() << "\r\n";
        ss << "\r\n";
        header_str = ss.str();
    }
    else
    {
        std::stringstream ss;
        ss << "HTTP/1.1 200 OK\r\n";
        ss << "Access-Control-Allow-Headers: Content-Type, Access-Control-Allow-Headers, Access-Control-Allow-Origin\r\n";
        ss << "Content-Type: text/plain\r\n";
        ss << "Access-Control-Allow-Origin: *\r\n";
        ss << "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n";
        ss << "id: " << header->id << "\r\n";
        ss << "trace_id: " << header->trace_id << "\r\n";
        ss << "rpc_name: " << msg->dheader.rpc_name << "\r\n";
        ss << "serialize_format: " << enum_to_string((dsn_msg_serialize_format)header->context.u.serialize_format) << "\r\n";
        ss << "server_error: " << header->context.u.server_error << "\r\n";
        ss << "Content-Length: " << msg->body_size() << "\r\n";
        ss << "\r\n";
        header_str = ss.str();
    }
    unsigned int header_len = header_str.size();
    std::shared_ptr<char> header_holder(static_cast<char*>(dsn_transient_malloc(header_len)),
        [](char* c) {dsn_transient_free(c); });
    memcpy(header_holder.get(), header_str.data(), header_len);
    _send_header = blob(header_holder, header_len);

    // header
    buffers[0].buf = (void*)_send_header.data();
    buffers[0].sz = _send_header.length();

    // body (make sure skip message header)
    int idx = 1;
    size_t need_skip = sizeof(message_header);
    for (auto& buf : msg->buffers)
    {
        if (need_skip >= buf.length())
        {
            need_skip -= buf.length();
            continue;
        }

        buffers[idx].buf = (char*)buf.data() + need_skip;
        buffers[idx].sz = buf.length() - need_skip;
        need_skip = 0;

        ++idx;
    }

    return idx;
}

}
