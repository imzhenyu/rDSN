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
 *     io + computation looper interface, so everything uses the same thread
 *
 * Revision history:
 *     Aug., 2015, @imzhenyu (Zhenyu Guo), the first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

# include "io_looper.h"

namespace dsn
{
    namespace tools
    {
        class hpc_timer_service : public timer_service
        {
        public:
            hpc_timer_service(service_node* node, timer_service* inner_provider)
                : timer_service(node, inner_provider)
            {
                _looper = nullptr;
            }

            virtual void start()
            {
                _looper = get_io_looper(node(), false);
                dassert(_looper != nullptr, "correspondent looper is empty");
            }

            // after milliseconds, the provider should call task->enqueue()        
            virtual void add_timer(task* task);

        private:
            io_looper *_looper;
        };

        // ------------------ inline implementation --------------------
        

    }
}
