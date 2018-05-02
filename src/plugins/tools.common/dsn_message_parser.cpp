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
 *     What is this file about?
 *
 * Revision history:
 *     Jun. 2016, Zuoyan Qin, first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# include "dsn_message_parser.h"
# include <dsn/service_api_c.h>
# include <dsn/cpp/rpc_stream.h>
# include <dsn/tool-api/thread_profiler.h>

# ifdef __TITLE__
# undef __TITLE__
# endif
# define __TITLE__ "dsn.message.parser"

namespace dsn
{
    void dsn_message_parser::reset()
    {
        _header_checked = false;
    }

    message_ex* dsn_message_parser::get_message_on_receive(message_reader* reader, /*out*/ int& read_next)
    {
        read_next = 4096;

        char* buf_ptr = (char*)reader->data();
        unsigned int buf_len = reader->length();

        if (buf_len >= sizeof(message_header))
        {
            if (!_header_checked)
            {
                if (!is_right_header(buf_ptr))
                {
                    derror("dsn message header check failed");
                    read_next = -1;
                    return nullptr;
                }
                else
                {
                    _header_checked = true;
                }
            }

            unsigned int msg_sz = sizeof(message_header)
                + message_ex::get_body_and_dynhdr_length(buf_ptr)
                ;

            // msg done
            if (buf_len >= msg_sz)
            {
                unsigned int non_dhdr_sz = sizeof(message_header) + message_ex::get_body_length(buf_ptr);
                dsn::blob msg_bb = reader->range(0, non_dhdr_sz);
                dsn::blob dhdr_bb = reader->range(non_dhdr_sz, msg_sz - non_dhdr_sz);
                message_ex* msg = message_ex::create_receive_message(std::move(msg_bb));
                
                binary_reader dreader(dhdr_bb);
                msg->dheader.read(dreader, msg);
                
                reader->consume(msg_sz);
                _header_checked = false;

                read_next = (reader->length() >= sizeof(message_header) ?
                    0 : sizeof(message_header) - reader->length());
                return msg;
            }
            else // buf_len < msg_sz
            {
                read_next = msg_sz - buf_len;
                return nullptr;
            }
        }
        else // buf_len < sizeof(message_header)
        {
            read_next = sizeof(message_header) - buf_len;
            return nullptr;
        }
    }

    void dsn_message_parser::prepare_on_send(message_ex* msg)
    {
        auto& header = msg->header;

        if (header->dyn_hdr_length == 0)
        {
            auto bz = header->body_length;
            {
                rpc_write_stream writer(msg);
                msg->dheader.write(writer, msg);
            }
            header->dyn_hdr_length = header->body_length - bz;
            header->body_length = bz;
        }
        
#ifndef NDEBUG
        auto& buffers = msg->buffers;
        int i_max = (int)buffers.size() - 1;
        size_t len = 0;
        for (int i = 0; i <= i_max; i++)
        {
            len += (size_t)buffers[i].length();
        }
        
        dassert(len == (size_t)header->body_length + header->dyn_hdr_length + sizeof(message_header),
            "data length is wrong");
#endif
    }

    int dsn_message_parser::get_buffer_count_on_send(message_ex* msg)
    {
        return (int)msg->buffers.size();
    }

    int dsn_message_parser::get_buffers_on_send(message_ex* msg, /*out*/ send_buf* buffers)
    {
        int i = 0;        
        for (auto& buf : msg->buffers)
        {
            buffers[i].buf = (void*)buf.data();
            buffers[i].sz = buf.length();

            if (buffers[i].sz > 0)
                ++i;
        }
        return i;
    }

    /*static*/ bool dsn_message_parser::is_right_header(char* hdr)
    {
        auto mhdr = (message_header*)hdr;
        return mhdr->magic == 0xdeadbeef;
    }
}
