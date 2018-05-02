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
 *     message parser base prototype, to support different kinds
 *     of message headers (so as to interact among them)
 *
 * Revision history:
 *     Mar., 2015, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

#pragma once

# include <dsn/utility/ports.h>
# include <dsn/utility/singleton.h>
# include <dsn/tool-api/task_spec.h>
# include <dsn/tool-api/rpc_message.h>
# include <dsn/utility/autoref_ptr.h>
# include <dsn/cpp/blob.h>
# include <dsn/utility/dlib.h>
# include <vector>

namespace dsn 
{    
    class message_reader
    {
    public:
        explicit message_reader(int buffer_block_size)
            : _buffer_occupied(0), _buffer_block_size(buffer_block_size) {}
        ~message_reader() {}

        //
        // the following are used by producers that read data from network into this buffer
        //

        // called before read to extend read buffer
        DSN_API char* read_buffer_ptr(unsigned int read_next);

        // get remaining buffer capacity
        unsigned int read_buffer_capacity() const { return _buffer.length() - _buffer_occupied; }

        // called after read to mark data occupied
        void mark_read(unsigned int read_length) { _buffer_occupied += read_length; }

        // discard read data
        void truncate_read() { _buffer_occupied = 0; }

        unsigned int block_size() const { return _buffer_block_size; }

        //
        // the following are used by consumers that use the data in this buffer already
        //
        const char* data() const { return _buffer.data(); }
        
        unsigned int length() const { return _buffer_occupied; }

        dsn::blob range(unsigned int len) { return _buffer.range(0, len); }

        dsn::blob range(int offset, unsigned int len) { return _buffer.range(offset, len); }

        void consume(unsigned len);

    private:
        dsn::blob      _buffer;
        unsigned int    _buffer_occupied;
        unsigned int    _buffer_block_size;
    };

    class message_parser
    {
    public:
        template <typename T> static message_parser* create(bool is_client)
        {
            return new T(is_client);
        }

        template <typename T> static message_parser* create2(void* place, bool is_client)
        {
            return new(place) T(is_client);
        }

        typedef message_parser*  (*factory)(bool);
        typedef message_parser*  (*factory2)(void*, bool);
        
    public:
        message_parser(bool is_client) : _header_format(NET_HDR_INVALID) {}
        virtual ~message_parser() {}

        // reset the parser
        virtual void reset() {}

        // after read, see if we can compose a message
        // if read_next returns -1, indicated the the message is corrupted
        virtual message_ex* get_message_on_receive(message_reader* reader, /*out*/ int& read_next) = 0;

        // prepare buffer before send.
        // this method should be called before get_buffer_count_on_send() and get_buffers_on_send()
        // to do some prepare operation.
        // may be invoked for mutiple times if the message is reused for resending.
        virtual void prepare_on_send(message_ex* msg) {}

        // get max buffer count needed by get_buffers_on_send().
        // may be invoked for mutiple times if the message is reused for resending.
        virtual int get_buffer_count_on_send(message_ex* msg) = 0;

        // get buffers from message to 'buffers'.
        // return buffer count used, which must be no more than the return value of get_buffer_count_on_send().
        // may be invoked for mutiple times if the message is reused for resending.
        virtual int get_buffers_on_send(message_ex* msg, /*out*/ send_buf* buffers) = 0;

    public:
        net_header_format format() const { return _header_format; }
        DSN_API static net_header_format get_header_type(const char* bytes, int len);
        DSN_API static safe_string get_debug_string(const char* bytes, int len);
        DSN_API static message_parser* new_message_parser(net_header_format hdr_format, bool is_client);

    private:
        net_header_format _header_format;
    };

    // -------------- inline implementation ---------------
    inline void message_reader::consume(unsigned len)
    {
        _buffer = _buffer.range((int)len);
        _buffer_occupied -= len;
    }
}
