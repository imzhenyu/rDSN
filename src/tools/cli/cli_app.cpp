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


# include "cli_app.h"
# include <iostream>

namespace dsn {
    namespace service {

        cli::cli(dsn_gpid gpid)
            : service_app(gpid)
        {
            _timeout = std::chrono::seconds(10); // 10 seconds by default
            _target = nullptr;
        }

        void usage()
        {
            std::cout << "------------ rcli commands ------" << std::endl;
            std::cout << "lhelp:  show this message" << std::endl;
            std::cout << "exit:   exit the console" << std::endl;
            std::cout << "remote: set cli target by 'remote %machine:port% %timeout_seconds%" << std::endl;
            std::cout << "help:   show help message of remote target" << std::endl;
            std::cout << "all other commands are sent to remote target %machine%:%port%" << std::endl;
            std::cout << "---------------------------------" << std::endl;
        }

        error_code cli::start(int argc, char** argv)
        {
            
            std::cout << "dsn remote cli begin ..." << std::endl;
            usage();

            while (true)
            {
                std::string cmdline;
                std::cout << ">";
                std::getline(std::cin, cmdline);
                if (!std::cin)
                {
                    exit(0);
                }

                std::string scmd = cmdline;
                std::vector<std::string> args;

                utils::split_args(scmd.c_str(), args, ' ');

                if (args.size() < 1)
                    continue;

                std::string cmd = args[0];
                if (cmd == "lhelp")
                {
                    usage();
                    continue;
                }
                else if (cmd == "exit")
                {
                    exit(0);
                }
                else if (cmd == "remote")
                {
                    if (args.size() < 3)
                    {
                        std::cout << "invalid parameters for remote command, try lhelp" << std::endl;
                        continue;
                    }
                    else
                    {
                        std::string addr = args[1];
                        _timeout = std::chrono::seconds(atoi(args[2].c_str()));

                        if (nullptr != _target)
                            dsn_rpc_channel_close(_target);

                        _target = dsn_rpc_channel_open(addr.c_str());
                        _target_addr = addr;

                        std::cout << "remote target is set to " << addr << ", timeout = " << _timeout.count() << " seconds" <<std::endl;
                        _client.reset(new cli_client(_target));
                        continue;
                    }
                }
                else
                {
                    if (_target == nullptr)
                    {
                        std::cout << "remote target is not specified, try lhelp" << std::endl;
                        continue;
                    }

                    std::cout << "CALL " << _target_addr << " ..." << std::endl;

                    cli_string req;
                    req.content = std::move(cmdline);
                    auto pr = _client->call_sync(req, _timeout, 0, 0);
                    if (pr.first == ERR_OK)
                    {
                        std::cout << pr.second.content << std::endl;
                    }
                    else
                    {
                        std::cout << "remote cli failed, err = " << pr.first.to_string() << std::endl;
                    }
                    continue;
                }
            }
            return ERR_OK;
        }

        error_code cli::stop(bool cleanup)
        {
            return ERR_OK;
        } 

    }
}
