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
// apps
# include "echo.client.h"
# include <dsn/cpp/zlocks.h>

int main(int argc, char** argv)
{
    std::cout << "<usage>: echo.client echo.client.config.ini [...see dsn_run...]" << std::endl;

    // initialize dsn system
    dsn_run(argc, argv, false);
    
    // run client
    auto ch = dsn_rpc_channel_open(
            "raw://localhost:27101/echo1",
            "NET_CHANNEL_TCP", 
            "NET_HDR_HTTP" // NET_HDR_DSN, ...
            );

    dsn::example::echo_client client(ch);

    // sync ping
    for (int i = 0; i < 10; i++)
    {
        std::stringstream req;
        req << "client ping " << i;
        auto resp = client.ping_sync(req.str());

        std::cout << "client sync ping returns " << resp.first.to_string() << ", result = " << resp.second << std::endl;
    }

    // async pings 
    int i = 0; 
    ::dsn::zevent evt;

    std::function<void()> ping_routine = [&]()
    {
        std::stringstream req;
        req << "client ping " << i;

        client.ping(
            req.str(), // request
            [&](::dsn::error_code err, std::string&& resp) // callback
            {
                std::cout << "client async ping returns " << err.to_string() << ", result = " << resp << std::endl;

                if (++i == 10)
                {
                    evt.set();
                }
                else
                {
                    ping_routine();
                }
            }
            );
    };

    ping_routine();
    evt.wait();

    dsn_rpc_channel_close(ch);

    dsn_exit(0);

    return 0;
}
