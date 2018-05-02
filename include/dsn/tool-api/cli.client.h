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
# include <dsn/service_api_cpp.h>
# include <iostream>

DEFINE_TASK_CODE_RPC(RPC_CLI_CLI_CALL, TASK_PRIORITY_COMMON, ::dsn::THREAD_POOL_DEFAULT)
namespace dsn 
{

    // note both request and response msgs
    // are both c strings (no length is prefixed)
    // see void command_manager::on_remote_cli(dsn_message_t req)

    struct cli_string
    {
        std::string content;
    };

    inline void marshall( ::dsn::binary_writer& writer, const cli_string& val)
    {
        writer.write(val.content.c_str(), (int)val.content.length());
        writer.write((uint8_t)'\0');
    }

    inline void unmarshall( ::dsn::binary_reader& reader, /*out*/ cli_string& val)
    {
        val.content.resize(reader.get_remaining_size() - 1);
        reader.read((char*)&val.content[0], val.content.length());
    }

    class cli_client
    {
    public:
        cli_client(dsn_channel_t server) { _server = server; }
        virtual ~cli_client() {}

        // ---------- call RPC_CLI_CLI_CALL ------------
        // - synchronous
        std::pair< ::dsn::error_code, cli_string> call_sync(
            const cli_string& args,
            std::chrono::milliseconds timeout = std::chrono::milliseconds(0),
            int thread_hash = 0,
            uint64_t partition_hash = 0
            )
        {
            return ::dsn::rpc::call_wait< cli_string, cli_string>(
                    _server,
                    RPC_CLI_CLI_CALL,
                    args,
                    timeout,
                    thread_hash,
                    partition_hash
                    );
        }

        // - asynchronous with on-stack cli_string and cli_string
        // TCallback - (error_code err, cli_string&& resp){} 
        // TCallback - bool (dsn_rpc_error_t, dsn_message_t resp) {}
        template<typename TCallback>
        void call(
            const cli_string& args,
            TCallback&& callback,
            std::chrono::milliseconds timeout = std::chrono::milliseconds(0),
            int thread_hash = 0,
            uint64_t partition_hash = 0,
            int reply_thread_hash = 0
            )
        {
            return ::dsn::rpc::call(
                _server,
                RPC_CLI_CLI_CALL,
                args,
                std::forward<TCallback>(callback),
                timeout,
                thread_hash,
                partition_hash,
                reply_thread_hash
                );
        }

        dsn_channel_t get_dsn_channel_t() {return _server;}
    private:
        dsn_channel_t _server;
    };

}