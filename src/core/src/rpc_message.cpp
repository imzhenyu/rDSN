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

# include <dsn/utility/ports.h>
# include <dsn/tool-api/rpc_message.h>
# include <dsn/tool-api/network.h>
# include <dsn/tool-api/message_parser.h>
# include <dsn/cpp/rpc_stream.h>
# include <dsn/tool-api/thread_profiler.h>
# include <cctype> // for isprint()

# include "task_engine.h"
# include "transient_memory.h"

using namespace dsn::utils;

# ifdef __TITLE__
# undef __TITLE__
# endif
# define __TITLE__ "rpc.message"

DSN_API dsn_message_t dsn_msg_create_request(
    dsn_task_code_t rpc_code,
    int timeout_milliseconds,
    int thread_hash,
    uint64_t partition_hash
    )
{
    return ::dsn::message_ex::create_request(rpc_code, timeout_milliseconds, thread_hash, partition_hash);
}

DSN_API dsn_message_t dsn_msg_create_response(dsn_message_t request)
{
    auto msg = ((::dsn::message_ex*)request)->create_response();
    return msg;
}

DSN_API void dsn_msg_write_next(dsn_message_t msg, void** ptr, size_t* size, size_t min_size)
{
    ((::dsn::message_ex*)msg)->write_next(ptr, size, min_size);
}

DSN_API void dsn_msg_write_commit(dsn_message_t msg, size_t size)
{
    ((::dsn::message_ex*)msg)->write_commit(size);
}

DSN_API void dsn_msg_append(dsn_message_t msg, void* buffer, size_t size, void* context, void(*sent_callback)(void*, void*))
{
    ::dsn::blob bb(
            std::shared_ptr<char>((char*)buffer, [=](void* buf)
                {
                    if (sent_callback)
                        (*sent_callback)(context, buf);
                }),
            (unsigned int)size
            );
    ((::dsn::message_ex*)msg)->write_append(std::move(bb));
}

DSN_API bool dsn_msg_read_next(dsn_message_t msg, void** ptr, size_t* size)
{
    return ((::dsn::message_ex*)msg)->read_next(ptr, size);
}

DSN_API void dsn_msg_read_commit(dsn_message_t msg, size_t size)
{
    ((::dsn::message_ex*)msg)->read_commit(size);
}

DSN_API size_t dsn_msg_body_size(dsn_message_t msg)
{
    return ((::dsn::message_ex*)msg)->body_size();
}

DSN_API void* dsn_msg_rw_ptr(dsn_message_t msg, size_t offset_begin)
{
    return ((::dsn::message_ex*)msg)->rw_ptr(offset_begin);
}

DSN_API void dsn_msg_add_ref(dsn_message_t msg)
{
    //((::dsn::message_ex*)msg)->add_ref();
}

DSN_API void dsn_msg_destroy(dsn_message_t msg)
{
    delete ((::dsn::message_ex*)msg);
}

DSN_API uint64_t dsn_msg_trace_id(dsn_message_t msg)
{
    return ((::dsn::message_ex*)msg)->header->trace_id;
}

DSN_API dsn_task_code_t dsn_msg_task_code(dsn_message_t msg)
{
    return ((::dsn::message_ex*)msg)->rpc_code();
}

DSN_API void* dsn_msg_session_context(dsn_message_t msg)
{
    return ((::dsn::message_ex*)msg)->io_session_context;
}

DSN_API void dsn_msg_set_options(
    dsn_message_t msg,
    dsn_msg_options_t *opts,
    uint32_t mask // set opt bits using DSN_MSGM_XXX
    )
{
    auto hdr = ((::dsn::message_ex*)msg)->header;

    if (mask & DSN_MSGM_TIMEOUT)
    {
        hdr->client.timeout_ms = opts->timeout_ms;
    }

    if (mask & DSN_MSGM_THREAD_HASH)
    {
        hdr->client.thread_hash = opts->thread_hash;
    }

    if (mask & DSN_MSGM_PARTITION_HASH)
    {
        ((::dsn::message_ex*)msg)->u.client.partition_hash = opts->partition_hash;
    }

    if (mask & DSN_MSGM_VNID)
    {
        hdr->context.u.app_id = opts->gpid.u.app_id;
        hdr->context.u.partition_index = opts->gpid.u.partition_index;
    }

}

DSN_API dsn_msg_serialize_format dsn_msg_get_serialize_format(dsn_message_t msg)
{
    auto hdr = ((::dsn::message_ex*)msg)->header;
    return static_cast<dsn_msg_serialize_format>(hdr->context.u.serialize_format);
}

DSN_API void dsn_msg_set_serailize_format(dsn_message_t msg, dsn_msg_serialize_format fmt)
{
    auto hdr = ((::dsn::message_ex*)msg)->header;
    hdr->context.u.serialize_format = fmt;
}

DSN_API void dsn_msg_get_options(
    dsn_message_t msg,
    /*out*/ dsn_msg_options_t* opts
    )
{
    auto hdr = ((::dsn::message_ex*)msg)->header;
    opts->timeout_ms = hdr->client.timeout_ms;
    opts->thread_hash = hdr->client.thread_hash;
    opts->partition_hash = ((::dsn::message_ex*)msg)->u.client.partition_hash;
    opts->gpid.u.app_id = hdr->context.u.app_id;
    opts->gpid.u.partition_index = hdr->context.u.partition_index;
}

namespace dsn {

std::atomic<uint32_t> message_ex::_request_id(1);
uint32_t message_ex::s_local_hash = 0;

void message_dynamic_header::add(safe_string&& key, safe_string&& value)
{
    if (headers == nullptr)
    {
        headers = new safe_unordered_map<safe_string, safe_string>();
    }
    headers->emplace(std::forward<safe_string>(key), std::forward<safe_string>(value));
}

void message_dynamic_header::write(binary_writer & writer, message_ex* msg)
{
    auto& name = task_spec::get(msg->local_rpc_code)->name;
    dbg_dassert(name == safe_string(rpc_name), "");

    int16_t sz;
    if (msg->header->context.u.is_request)
    {
        // write method name
        sz = (int16_t)name.length();
        writer.write(sz);
        writer.write(name.c_str(), (int)sz);
        sz = 0;

        // write service name
        sz = (int16_t)service_name.length();
        writer.write(sz);
        if (sz > 0)
        {
            writer.write(service_name.c_str(), (int)sz);
            sz = 0;
        }
    }
    else
    {
        sz = 0;
        writer.write(sz);
    }

    if (headers) sz = (int16_t)(headers->size());
    writer.write(sz);

    if (sz > 0)
    {
        for (auto& kv : *headers)
        {
            writer.write(kv.first);
            writer.write(kv.second);
        }
    }
}


void message_dynamic_header::read(binary_reader & reader, message_ex* msg)
{
    int16_t sz;
    reader.read(sz);

    if (msg->header->context.u.is_request)
    {
        // read method name
        char* ptr = (char*)reader.ptr();
        char c = ptr[sz];
        ptr[sz] = '\0';

        auto sp = task_spec::rpc_get((const char*)ptr);
        msg->local_rpc_code = sp->code;
        rpc_name = sp->name.c_str();

        ptr[sz] = c;
        reader.skip((int)sz);

        // read service name
        reader.read(sz);
        if (sz > 0)
        {
            service_name.resize((size_t)sz);
            reader.read((char*)service_name.c_str(), sz);
        }
    }
    else
    {
        msg->local_rpc_code = TASK_CODE_INVALID;
        rpc_name = task_spec::get(TASK_CODE_INVALID)->name.c_str();
        reader.skip((int)sz);
    }

    // map
    reader.read(sz);
    if (sz > 0)
    {
        headers = new safe_unordered_map<safe_string, safe_string>();
        safe_string k, v;
        for (int16_t i = 0; i < sz; i++)
        {
            reader.read(k);
            reader.read(v);
            headers->emplace(std::move(k), std::move(v));
        }
    }
}

message_ex::message_ex()
    : header(nullptr),
      local_rpc_code(::dsn::TASK_CODE_INVALID),
      _read_index(0),
      _rw_offset(0),
      _rw_committed(true),
      _is_read(false)
{
    memset(&u, 0, sizeof(u));
    io_session_context = nullptr;
    io_session_secret = 0;
}

message_ex::~message_ex()
{
    if (!_is_read)
    {
        dassert(_rw_committed, "message write is not committed");
    }
}

message_ex* message_ex::create_receive_message(blob&& data)
{
    message_ex* msg = new message_ex();
    msg->header = (message_header*)data.data();
    msg->_is_read = true;
    // the message_header is hidden ahead of the buffer

    data.remove_head((unsigned int)sizeof(message_header));
    dbg_dassert(msg->header->body_length == data.length(), "");

    msg->buffers.emplace_back(std::move(data));
    return msg;
}

message_ex* message_ex::create_receive_message_with_standalone_header(blob&& data)
{
    message_ex* msg = new message_ex();
    std::shared_ptr<char> header_holder(static_cast<char*>(dsn_transient_malloc(sizeof(message_header))), [](char* c) {dsn_transient_free(c);});
    msg->header = reinterpret_cast<message_header*>(header_holder.get());
    memset(msg->header, 0, sizeof(message_header));
    msg->buffers.emplace_back(blob(std::move(header_holder), sizeof(message_header)));
    msg->buffers.emplace_back(std::move(data));

    msg->header->body_length = data.length();
    msg->_is_read = true;
    //we skip the message header
    msg->_read_index = 1;

    return msg;
}

message_ex* message_ex::create_receive_message_with_standalone_header()
{
    message_ex* msg = new message_ex();
    std::shared_ptr<char> header_holder(static_cast<char*>(dsn_transient_malloc(sizeof(message_header))), [](char* c) {dsn_transient_free(c);});
    msg->header = reinterpret_cast<message_header*>(header_holder.get());
    memset(msg->header, 0, sizeof(message_header));
    msg->buffers.emplace_back(blob(std::move(header_holder), sizeof(message_header)));
    msg->_is_read = true;
    //we skip the message header
    msg->_read_index = 1;
    return msg;
}

message_ex* message_ex::create_request(dsn_task_code_t rpc_code, int timeout_milliseconds, int thread_hash, uint64_t partition_hash)
{
    message_ex* msg = new message_ex();

    TPF_MARK("message_ex::create_request.new.done");

    msg->_is_read = false;
    msg->pepare_buffer_header_on_write();

    TPF_MARK("message_ex::create_request.pepare_buffer_header_on_write.done");

    // init header
    auto& hdr = *msg->header;
    memset(&hdr, 0, sizeof(hdr));
    hdr.hdr_type = *(uint32_t*)"RDSN";
    hdr.magic = 0xdeadbeef;
    hdr.fix_hdr_length = sizeof(message_header);

    // if thread_hash == 0 && partition_hash != 0,
    // thread_hash is computed from partition_hash in rpc_engine
    hdr.client.thread_hash = thread_hash;
    msg->u.client.partition_hash = partition_hash;

    task_spec* sp = task_spec::get(rpc_code);
    if (0 == timeout_milliseconds)
    {
        hdr.client.timeout_ms = sp->rpc_timeout_milliseconds;
    }
    else
    {
        hdr.client.timeout_ms = timeout_milliseconds;
    }

    msg->local_rpc_code = rpc_code;
    msg->dheader.rpc_name = sp->name.c_str();

    static std::atomic<uint32_t> sid(0);
    hdr.id = ++sid; // TODO: performance optimization
    hdr.trace_id = tls_dsn.node_pool_thread_ids + (++tls_dsn.last_lower32_task_id);

    hdr.context.u.is_request = true;
    hdr.context.u.serialize_format = sp->rpc_msg_payload_serialize_default_format;
    return msg;
}

message_ex* message_ex::create_response()
{
    message_ex* msg = new message_ex();
    msg->_is_read = false;
    msg->pepare_buffer_header_on_write();

    // init header
    auto& hdr = *msg->header;
    hdr = *header; // copy request header
    hdr.body_length = 0;
    hdr.dyn_hdr_length = 0;
    hdr.context.u.is_request = false;

    task_spec* sp = task_spec::get(local_rpc_code);
    msg->local_rpc_code = sp->rpc_paired_code;
    msg->dheader.rpc_name = task_spec::get(sp->rpc_paired_code)->name.c_str();

    // ATTENTION: the from_address may not be the primary address of this node
    // if there are more than one ports listened and the to_address is not equal to
    // the primary address.
    msg->from_address = to_address;
    msg->to_address = from_address;
    msg->u.server = u.server;
    msg->io_session_secret = io_session_secret;
    msg->io_session_context = io_session_context;

    // join point
    sp->on_rpc_create_response.execute(this, msg);

    return msg;
}

void message_ex::pepare_buffer_header_on_write()
{
    void* ptr;
    size_t size;
    ::dsn::tls_trans_mem_next(&ptr, &size, sizeof(message_header));

    ::dsn::blob buffer(
        (*::dsn::tls_trans_memory.block),
        (int)((char*)(ptr) - ::dsn::tls_trans_memory.block->get()),
        (int)sizeof(message_header)
        );

    ::dsn::tls_trans_mem_commit(sizeof(message_header));
    this->_rw_offset = (int)sizeof(message_header);
    this->buffers.emplace_back(std::move(buffer));

    header = (message_header*)ptr;
}

void message_ex::write_next(void** ptr, size_t* size, size_t min_size)
{
    // printf("%p %s\n", this, __FUNCTION__);
    dassert(!this->_is_read && this->_rw_committed, "there are pending msg write not committed"
        ", please invoke dsn_msg_write_next and dsn_msg_write_commit in pairs");
    ::dsn::tls_trans_mem_next(ptr, size, min_size);
    this->_rw_committed = false;

    // optimization
    if (this->buffers.size() > 0)
    {
        auto& lbb = *this->buffers.rbegin();

        // if the current allocation is within the same buffer with the previous one
        if (*ptr == lbb.data() + lbb.length()
            && ::dsn::tls_trans_memory.block->get() == lbb.buffer_ptr())
        {
            lbb.extend(*size);
            return;
        }
    }

    ::dsn::blob buffer(
        (*::dsn::tls_trans_memory.block),
        (int)((char*)(*ptr) - ::dsn::tls_trans_memory.block->get()),
        (int)(*size)
        );
    this->_rw_offset = 0;
    this->buffers.emplace_back(std::move(buffer));
}

void message_ex::write_commit(size_t size)
{
    // printf("%p %s\n", this, __FUNCTION__);
    dassert(!this->_rw_committed, "there are no pending msg write to be committed"
        ", please invoke dsn_msg_write_next and dsn_msg_write_commit in pairs");

    ::dsn::tls_trans_mem_commit(size);

    this->_rw_offset += (int)size;
    this->buffers.rbegin()->set_length(this->_rw_offset);
    this->_rw_committed = true;
    this->header->body_length += (int)size;
}

void message_ex::write_append(blob&& data)
{
    // printf("%p %s\n", this, __FUNCTION__);
    dassert(!this->_is_read && this->_rw_committed, "there are pending msg write not committed"
        ", please invoke dsn_msg_write_next and dsn_msg_write_commit in pairs");

    int size = data.length();
    if (size > 0)
    {
        this->_rw_offset += size;
        this->buffers.emplace_back(std::move(data));
        this->header->body_length += size;
    }
}

bool message_ex::read_next(void** ptr, size_t* size)
{
    // printf("%p %s %d\n", this, __FUNCTION__, utils::get_current_tid());
    dassert(this->_is_read && this->_rw_committed, "there are pending msg read not committed"
        ", please invoke dsn_msg_read_next and dsn_msg_read_commit in pairs");

    int idx = this->_read_index;
    if (this->_rw_offset == static_cast<int>(this->buffers[idx].length()))
    {
        idx = ++this->_read_index;
        this->_rw_offset = 0;
    }

    if (idx < (int)this->buffers.size())
    {
        this->_rw_committed = false;
        *ptr = (void*)(this->buffers[idx].data() + this->_rw_offset);
        *size = (size_t)this->buffers[idx].length() - this->_rw_offset;
        return true;
    }
    else
    {
        *ptr = nullptr;
        *size = 0;
        return false;
    }
}

void message_ex::read_commit(size_t size)
{
    // printf("%p %s\n", this, __FUNCTION__);
    dassert(!this->_rw_committed, "there are no pending msg read to be committed"
        ", please invoke dsn_msg_read_next and dsn_msg_read_commit in pairs");

    this->_rw_offset += (int)size;
    this->_rw_committed = true;
}

void* message_ex::rw_ptr(size_t offset_begin)
{
    //printf("%p %s\n", this, __FUNCTION__);
    int i_max = (int)this->buffers.size();

    if (!_is_read)
        offset_begin += sizeof(message_header);

    for (int i = 0; i < i_max; i++)
    {
        size_t c_length = (size_t)(this->buffers[i].length());
        if (offset_begin < c_length)
        {
            return (void*)(this->buffers[i].data() + offset_begin);
        }
        else
        {
            offset_begin -= c_length;
        }
    }
    return nullptr;
}

void message_ex::get_buffers(message_parser* parser, /*out*/ std::vector<send_buf>& buffers)
{
    auto lcount = parser->get_buffer_count_on_send(this);
    if (buffers.size() < lcount)
    {
        buffers.resize(lcount);
    }

    auto rcount = parser->get_buffers_on_send(this, &buffers[0]);
    dassert(lcount >= rcount, "");
    buffers.resize(rcount);
}

} // end namespace dsn
