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
 *     xxxx-xx-xx, author, first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# pragma once

# include <atomic>
# include <dsn/utility/ports.h>
# include <dsn/utility/extensible_object.h>
# include <dsn/utility/dlib.h>
# include <dsn/cpp/callocator.h>
# include <dsn/cpp/auto_codes.h>
# include <dsn/cpp/address.h>
# include <dsn/cpp/safe_string.h>
# include <dsn/cpp/blob.h>
# include <dsn/utility/link.h>
# include <dsn/tool-api/global_config.h>
# include <dsn/tool-api/network_ids.h>

namespace dsn 
{
    class message_parser;
    class rpc_session;
    class network;
    typedef struct message_header
    {
        uint32_t       hdr_type;    // must be the first four bytes, see message_parser for details
        uint32_t       magic;
        uint32_t       fix_hdr_length;  // fixed header length as sizeof(message_header)
        uint32_t       dyn_hdr_length;  // dynamic header length as marshall mesasge_dynamic_header below
        uint32_t       body_length;      // payload
        uint32_t       id;          // sequence id
        uint64_t       trace_id;    // used for tracking source

        union msg_context_t
        {
            struct {
                uint64_t is_request : 1;        ///< whether the RPC message is a request or response
                uint64_t serialize_format : 4;  ///< dsn_msg_serialize_format
                uint64_t server_error : 3;      ///< dsn_rpc_error_t, only used for is_request == 0
                uint64_t app_id : 24;           ///< 1-based app id (0 for invalid when virtual nodes are not enabled)
                uint64_t partition_index : 32;  ///< zero-based partition index
            } u;
            uint64_t context;                   ///< msg_context is of sizeof(uint64_t)
        } context;
        
        // only used when context.u.is_request == 1
        struct
        {
            int32_t  timeout_ms;     // rpc timeout in milliseconds
            int32_t  thread_hash;    // thread hash used for thread dispatching
        } client;
    } message_header;

    typedef struct message_dynamic_header
    {
        safe_string service_name;
        const char* rpc_name;
        safe_unordered_map<safe_string, safe_string>* headers; // use ptr to avoid ctor cost when no meta data is needed

        DSN_API void add(safe_string&& key, safe_string&& value);
        DSN_API void write(binary_writer & writer, message_ex* msg);
        DSN_API void read(binary_reader & reader, message_ex* msg);

        message_dynamic_header() : rpc_name("unknown"), headers(nullptr) {}
        ~message_dynamic_header() { if (headers) delete headers; }

    } message_dynamic_header;

    // be compatible with WSABUF on windows and iovec on linux
# ifdef _WIN32
    struct send_buf
    {
        uint32_t sz;
        void*    buf;
    };
# else
    struct send_buf
    {
        void*    buf;
        size_t   sz;
    };
# endif

    class message_ex :
        public extensible_object<message_ex, 4>,
        public transient_object
    {
    public:
        message_header         *header;
        safe_vector<blob>      buffers; // header included for *send* message, 
                                        // header not included for *recieved*

        message_dynamic_header dheader;

        // by rpc and network
        rpc_address            from_address;    
        rpc_address            to_address;
        dsn_task_code_t       local_rpc_code;

        // local context
        union {
            struct {
                uint64_t          partition_hash;  // only used by client
                rpc_response_task *call;
            } client;
            struct {
                rpc_session *s;
                network     *net;     
            } server;
        } u;

        void*                  io_session_context;
        uint64_t               io_session_secret; // send/recv sessions
        
        // by message queuing
        dlink                  dl;

    public:        
        //message_ex(blob bb, bool parse_hdr = true); // read 
        DSN_API ~message_ex();

        //
        // utility routines
        //
        task_code rpc_code();
        static unsigned int get_body_and_dynhdr_length(char* hdr) { return ((message_header*)hdr)->body_length + ((message_header*)hdr)->dyn_hdr_length;; }
        static unsigned int get_body_length(char* hdr) { return ((message_header*)hdr)->body_length; }
        static unsigned int get_dheader_length(char* hdr) { return ((message_header*)hdr)->dyn_hdr_length; }
        gpid get_gpid() const;

        //
        // routines for create messages
        //
        DSN_API static message_ex* create_receive_message(blob&& data);
        static message_ex* create_receive_message(const blob& data);
        DSN_API static message_ex* create_request(
            dsn_task_code_t rpc_code, 
            int timeout_milliseconds = 0,
            int thread_hash = 0,
            uint64_t partition_hash = 0
            );

        DSN_API static message_ex* create_receive_message_with_standalone_header(blob&& data);
        DSN_API static message_ex* create_receive_message_with_standalone_header();
        DSN_API message_ex* create_response();

        //
        // routines for buffer management
        //        
        DSN_API void write_next(void** ptr, size_t* size, size_t min_size);
        DSN_API void write_commit(size_t size);
        DSN_API void write_append(blob&& data);
        void write_append(const blob& data);
        DSN_API bool read_next(void** ptr, size_t* size);
        DSN_API void read_commit(size_t size);
        size_t body_size() { return (size_t)header->body_length; }
        DSN_API void* rw_ptr(size_t offset_begin);
        DSN_API void get_buffers(message_parser* parser, /*out*/ std::vector<send_buf>& buffers);

    private:
        DSN_API message_ex();
        DSN_API void pepare_buffer_header_on_write();

    private:        
        static std::atomic<uint32_t> _request_id;

    private:
        // by msg read & write
        int                    _read_index;   // current read buffer index
        int                    _rw_offset;    // current buffer offset
        bool                   _rw_committed; // mark if it is in middle state of reading/writing
        bool                   _is_read;      // is for read(recv) or write(send)

    public:
        static uint32_t s_local_hash;  // used by fast_rpc_name
    };

    // ---------------- inline ----------------------
    inline gpid message_ex::get_gpid() const 
    {
        gpid gd; 
        gd.set_app_id(header->context.u.app_id); 
        gd.set_partition_index(header->context.u.partition_index); 
        return gd; 
    }

    inline task_code message_ex::rpc_code()
    {
        return task_code(local_rpc_code);
    }

    inline /*static*/ message_ex* message_ex::create_receive_message(const blob& data)
    {
        blob tmp = data;
        return create_receive_message(std::move(tmp));
    }

    inline void message_ex::write_append(const blob& data)
    {
        blob tmp = data;
        write_append(std::move(tmp));
    }

} // end namespace
