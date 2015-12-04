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

# include <dsn/dist/replication.h>
# include "server_state.h"
# include "meta_service.h"
# include "distributed_lock_service_simple.h"
# include "meta_state_service_simple.h"
# include "../zookeeper/meta_state_service_zookeeper.h"
# include "../zookeeper/distributed_lock_service_zookeeper.h"
# include <dsn/internal/factory_store.h>

namespace dsn {
    namespace service {

        bool register_component_provider(
            const char* name,
            ::dsn::dist::distributed_lock_service::factory f)
        {
            return dsn::utils::factory_store< ::dsn::dist::distributed_lock_service>::register_factory(
                name, 
                f,
                PROVIDER_TYPE_MAIN);
        }

        bool register_component_provider(
            const char* name,
            ::dsn::dist::meta_state_service::factory f)
        {
            return dsn::utils::factory_store< ::dsn::dist::meta_state_service>::register_factory(
                name,
                f,
                PROVIDER_TYPE_MAIN);
        }

        meta_service_app::meta_service_app()
        {
            _state = new server_state();
            _service = new meta_service(_state);

            register_component_provider(
                "distributed_lock_service_simple",
                ::dsn::dist::distributed_lock_service::create<dsn::dist::distributed_lock_service_simple>
                );

            register_component_provider(
                "meta_state_service_simple",
                ::dsn::dist::meta_state_service::create<dsn::dist::meta_state_service_simple>
                );

            register_component_provider(
                "distributed_lock_service_zookeeper",
                dsn::dist::distributed_lock_service::create<dsn::dist::distributed_lock_service_zookeeper>
                );

            register_component_provider(
                "meta_state_service_zookeeper",
                dsn::dist::meta_state_service::create<dsn::dist::meta_state_service_zookeeper>
                );
            // TODO: register more provides here used by meta servers
        }

        meta_service_app::~meta_service_app()
        {
        }

        ::dsn::error_code meta_service_app::start(int argc, char** argv)
        {
            _state->initialize(name().c_str());
            _service->start();
            return ERR_OK;
        }

        void meta_service_app::stop(bool cleanup)
        {
            if (_state != nullptr)
            {
                if (_service != nullptr)
                {
                    _service->stop();
                    delete _service;
                    _service = nullptr;

                    delete _state;
                    _state = nullptr;
                }
            }
            else
            {
                dassert(_service == nullptr, "service must be null");
            }
        }
    }
}
