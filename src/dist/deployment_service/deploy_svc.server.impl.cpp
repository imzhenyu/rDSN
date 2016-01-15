
# include "deploy_svc.server.impl.h"
# include <dsn/internal/factory_store.h>

namespace dsn
{
    namespace dist
    {


        DEFINE_THREAD_POOL_CODE(THREAD_POOL_DEPLOY_LONG)

        DEFINE_TASK_CODE(LPC_DEPLOY_DOWNLOAD_RESOURCE, TASK_PRIORITY_COMMON, THREAD_POOL_DEPLOY_LONG)

        static void __svc_cli_freeer__(dsn_cli_reply reply)
        {
            std::string* s = (std::string*)reply.context;
            delete s;
        }

        deploy_svc_service_impl::deploy_svc_service_impl()
        {
            _cli_deploy = nullptr;
        }

        deploy_svc_service_impl::~deploy_svc_service_impl()
        {
            if (nullptr != _cli_deploy)
            {
                stop();
            }
        }

        void deploy_svc_service_impl::stop()
        {
            dsn_cli_deregister(_cli_deploy);
            _cli_deploy = nullptr;
        }
        
        error_code deploy_svc_service_impl::start()
        {
            std::string pdir = utils::filesystem::path_combine(dsn_get_current_app_data_dir(), "services");
            _service_dir = dsn_config_get_value_string("deploy.service",
                "deploy_dir",
                pdir.c_str(),
                "where to put temporal deployment resources"
                );

            // load clusters
            const char* clusters[100];
            int sz = 100;
            int count = dsn_config_get_all_keys("deploy.service.clusters", clusters, &sz);
            dassert(count <= 100, "too many clusters");

            for (int i = 0; i < count; i++)
            {
                std::string cluster_name = dsn_config_get_value_string(
                    clusters[i],
                    "name",
                    "",
                    "cluster name"
                    );

                if (nullptr != get_cluster(cluster_name))
                {
                    derror("cluster %s already defined", cluster_name.c_str());
                    return ERR_CLUSTER_ALREADY_EXIST;
                }

                std::string cluster_factory_type = dsn_config_get_value_string(
                    clusters[i],
                    "factory",
                    "",
                    "factory name to create the target cluster scheduler"
                    );

                auto cluster = ::dsn::utils::factory_store<cluster_scheduler>::create(
                    cluster_factory_type.c_str(),
                    PROVIDER_TYPE_MAIN
                    );

                if (nullptr == cluster)
                {
                    derror("cluster type %s is not defined", cluster_factory_type.c_str());
                    return ERR_OBJECT_NOT_FOUND;
                }

                std::shared_ptr<cluster_ex> ce(new cluster_ex);
                ce->scheduler.reset(cluster);
                ce->cluster.name = cluster_name;
                ce->cluster.type = cluster->type();

                _clusters[cluster_name] = ce;
            }

            _cli_deploy = dsn_cli_app_register(
                "deploy",
                "deploy deploy_request(in json format)",
                "deploy an app via our deployment service",
                (void*)this,
                [](void *context, int argc, const char **argv, dsn_cli_reply *reply)
                {
                    auto this_ = (deploy_svc_service_impl*)context;
                    this_->on_deploy_cli(context, argc, argv, reply);
                },
                __svc_cli_freeer__
                );

            _cli_undeploy = dsn_cli_app_register(
                    "undeploy",
                    "undepoy service_name(in json format)",
                    "undeploy an app via our deployment service",
                    (void*)this,
                    [](void *context, int argc, const char **argv, dsn_cli_reply *reply)
                {
                    auto this_ = (deploy_svc_service_impl*)context;
                    this_->on_undeploy_cli(context, argc, argv, reply);
                },
                __svc_cli_freeer__
                );

            _cli_get_service_list = dsn_cli_app_register(
                    "service_list",
                    "service_list package_id(in json format)",
                    "get service list of a package via our deployment service",
                    (void*)this,
                    [](void *context, int argc, const char **argv, dsn_cli_reply *reply)
                {
                    auto this_ = (deploy_svc_service_impl*)context;
                    this_->on_get_service_list_cli(context, argc, argv, reply);
                },
                    __svc_cli_freeer__
                );

            _cli_get_service_info = dsn_cli_app_register(
                    "service_info",
                    "service_info service_name(in json format)",
                    "get service info of a service via our deployment service",
                    (void*)this,
                    [](void *context, int argc, const char **argv, dsn_cli_reply *reply)
                {
                    auto this_ = (deploy_svc_service_impl*)context;
                    this_->on_get_service_info_cli(context, argc, argv, reply);
                },
                    __svc_cli_freeer__
                );

                _cli_get_cluster_list = dsn_cli_app_register(
                    "cluster_list",
                    "cluster_list format(in json format)",
                    "get cluster list with a specific format via our deployment service",
                    (void*)this,
                    [](void *context, int argc, const char **argv, dsn_cli_reply *reply)
                {
                    auto this_ = (deploy_svc_service_impl*)context;
                    this_->on_get_cluster_list_cli(context, argc, argv, reply);
                },
                    __svc_cli_freeer__
                    );

            return ERR_OK;
        }

        void deploy_svc_service_impl::on_service_failure(
            std::shared_ptr<::dsn::dist::deployment_unit> unit,
            ::dsn::error_code err,
            const std::string& err_msg
            )
        {
            // TODO: fail-over?
            unit->status = ::dsn::dist::service_status::SS_FAILED;
        }


        void deploy_svc_service_impl::download_service_resource_completed(error_code err, std::shared_ptr<::dsn::dist::deployment_unit> svc)
        {
            if (err != ::dsn::ERR_OK)
            {
                svc->status = ::dsn::dist::service_status::SS_FAILED;
                return;
            }

            svc->status = ::dsn::dist::service_status::SS_DEPLOYING;
            auto cluster = get_cluster(svc->cluster);
            dassert(nullptr != cluster, "cluster %s is missing", svc->cluster.c_str());

            cluster->scheduler->schedule(svc);
        }

        void deploy_svc_service_impl::on_service_deployed(
            std::shared_ptr<::dsn::dist::deployment_unit> unit,
            ::dsn::error_code err,
            ::dsn::rpc_address addr
            )
        {
            if (err != ::dsn::ERR_OK)
                unit->status = ::dsn::dist::service_status::SS_FAILED;
            else
                unit->status = ::dsn::dist::service_status::SS_RUNNING;
        }

        void deploy_svc_service_impl::on_deploy_internal(const deploy_request& req, /*out*/ deploy_info& di)
        {
            di.name = req.name;
            di.package_id = req.package_id;
            di.cluster = req.cluster_name;
            di.status = service_status::SS_FAILED;

            auto svc = get_service(req.name);

            // service with the same name is already deployed
            if (svc != nullptr)
            {
                di.error = ::dsn::ERR_SERVICE_ALREADY_RUNNING;
                di.cluster = svc->cluster;
                return;
            }

            auto cluster = get_cluster(req.cluster_name);

            // cluster is missing
            if (cluster == nullptr)
            {
                di.error = ::dsn::ERR_CLUSTER_NOT_FOUND;
                di.cluster = req.cluster_name;
                return;
            }

            // prepare for svc starting
            svc.reset(new ::dsn::dist::deployment_unit());
            svc->cluster = req.cluster_name;
            svc->package_id = req.package_id;
            svc->name = req.name;
            svc->deployment_callback = [this, svc](::dsn::error_code err, ::dsn::rpc_address addr)
            {
                this->on_service_deployed(svc, err, addr);
            };
            svc->failure_notification = [this, svc](::dsn::error_code err, const std::string& err_msg)
            {
                this->on_service_failure(svc, err, err_msg);
            };

            // add to service collections
            {
                ::dsn::service::zauto_write_lock l(_service_lock);
                auto it = _services.find(req.name);
                if (it != _services.end())
                {
                    di.error = ::dsn::ERR_SERVICE_ALREADY_RUNNING;
                }
                else
                {
                    di.error = ::dsn::ERR_OK.to_string();
                    di.status = service_status::SS_PREPARE_RESOURCE;
                    _services.insert(
                        std::unordered_map<std::string, std::shared_ptr<::dsn::dist::deployment_unit> >::value_type(
                        req.name, svc
                        ));
                }
            }

            // start resource downloading ...
            if (di.error == ::dsn::ERR_IO_PENDING)
            {
                std::stringstream ss;
                ss << req.package_id << "." << req.name;

                std::string ldir = utils::filesystem::path_combine(_service_dir, ss.str());
                std::vector<std::string> files;

                file::copy_remote_files(
                    req.package_server,
                    req.package_full_path,
                    files,
                    ldir,
                    true,
                    LPC_DEPLOY_DOWNLOAD_RESOURCE,
                    this,
                    [this, svc](error_code err, size_t sz)
                {
                    this->download_service_resource_completed(err, svc);
                }
                );
            }
        }

        void deploy_svc_service_impl::on_deploy_cli(void *context, int argc, const char **argv, dsn_cli_reply *reply)
        {
            dassert(context == (void*)this, "context must be this");

            deploy_info di;
            deploy_request req;

            if (argc < 1 || ERR_OK != unmarshall_json(argv[0], "deploy_request", req))
            {
                di.error = ERR_INVALID_PARAMETERS;
            }
            else
            {
                on_deploy_internal(req, di);
            }

            std::string* resp_json = new std::string();
            *resp_json = marshall_json(di);
            reply->context = resp_json;
            reply->message = (const char*)resp_json->c_str();
            reply->size = resp_json->size();
            return;
        }

        void deploy_svc_service_impl::on_deploy(const deploy_request& req, ::dsn::rpc_replier<deploy_info>& reply)
        {
            deploy_info di;

            on_deploy_internal(req, di);

            reply(di);
            return;
        }

        void deploy_svc_service_impl::on_undeploy_internal(const std::string& service_name, error_code& err)
        {
            ::dsn::service::zauto_write_lock l(_service_lock);
            auto it = _services.find(service_name);
            if (it != _services.end())
            {
                _services.erase(it);
                err = ::dsn::ERR_OK;
            }
            else
            {
                err = ::dsn::ERR_SERVICE_NOT_FOUND;
            }
        }

        void deploy_svc_service_impl::on_undeploy_cli(void *context, int argc, const char **argv, dsn_cli_reply *reply)
        {
            dassert(context == (void*)this, "context must be this");

            error_code err;
            std::string service_name;

            if (argc < 1 || ERR_OK != unmarshall_json(argv[0], "service_name", service_name))
            {
                err = ERR_INVALID_PARAMETERS;
            }
            else
            {
                on_undeploy_internal(service_name, err);
            }

            std::string* resp_json = new std::string();
            *resp_json = marshall_json(err);
            reply->context = resp_json;
            reply->message = (const char*)resp_json->c_str();
            reply->size = resp_json->size();
            return;
        }

        void deploy_svc_service_impl::on_undeploy(const std::string& service_name, ::dsn::rpc_replier<error_code>& reply)
        {
            error_code err;

            on_undeploy_internal(service_name, err);

            reply(err);
        }

        void deploy_svc_service_impl::on_get_service_list_internal(const std::string& package_id, deploy_info_list& dlist)
        {
            ::dsn::service::zauto_read_lock l(_service_lock);
            for (auto& c : _services)
            {
                if (c.second->package_id == package_id)
                {
                    deploy_info di;
                    di.cluster = c.second->cluster;
                    di.package_id = c.second->package_id;
                    di.error = ::dsn::ERR_OK;
                    di.status = c.second->status;
                    dlist.services.push_back(di);
                }
            }
        }

        void deploy_svc_service_impl::on_get_service_list_cli(void *context, int argc, const char **argv, dsn_cli_reply *reply)
        {
            dassert(context == (void*)this, "context must be this");

            deploy_info_list dlist;
            std::string package_id;

            if (argc < 1 || ERR_OK != unmarshall_json(argv[0], "package_id", package_id))
            {
                //TODO: need raise error here?
            }
            else
            {
                on_get_service_list_internal(package_id, dlist);
            }

            std::string* resp_json = new std::string();
            *resp_json = marshall_json(dlist);
            reply->context = resp_json;
            reply->message = (const char*)resp_json->c_str();
            reply->size = resp_json->size();
            return;
        }

        void deploy_svc_service_impl::on_get_service_list(const std::string& package_id, ::dsn::rpc_replier<deploy_info_list>& reply)
        {
            deploy_info_list dlist;

            on_get_service_list_internal(package_id, dlist);

            reply(dlist);
        }

        void deploy_svc_service_impl::on_get_service_info_internal(const std::string& service_name, deploy_info& di)
        {
            di.name = service_name;

            ::dsn::service::zauto_read_lock l(_service_lock);
            auto it = _services.find(service_name);
            if (it == _services.end())
            {
                di.error = ::dsn::ERR_SERVICE_NOT_FOUND;
            }
            else
            {
                di.cluster = it->second->cluster;
                di.package_id = it->second->package_id;
                di.error = ::dsn::ERR_OK;
                di.status = it->second->status;
            }
        }

        void deploy_svc_service_impl::on_get_service_info_cli(void *context, int argc, const char **argv, dsn_cli_reply *reply)
        {
            dassert(context == (void*)this, "context must be this");

            deploy_info di;
            std::string service_name;

            if (argc < 1 || ERR_OK != unmarshall_json(argv[0], "service_name", service_name))
            {
                di.error = ERROR_INVALID_PARAMETER;
            }
            else
            {
                on_get_service_info_internal(service_name, di);
            }

            std::string* resp_json = new std::string();
            *resp_json = marshall_json(di);
            reply->context = resp_json;
            reply->message = (const char*)resp_json->c_str();
            reply->size = resp_json->size();
            return;
        }

        void deploy_svc_service_impl::on_get_service_info(const std::string& service_name, ::dsn::rpc_replier<deploy_info>& reply)
        {
            deploy_info di;
           
            on_get_service_info_internal(service_name, di);

            reply(di);
        }

        void deploy_svc_service_impl::on_get_cluster_list_internal(const std::string& format, cluster_list& clist)
        {
            ::dsn::service::zauto_read_lock l(_cluster_lock);
            for (auto& c : _clusters)
            {
                clist.clusters.push_back(c.second->cluster);
            }
        }

        void deploy_svc_service_impl::on_get_cluster_list_cli(void *context, int argc, const char **argv, dsn_cli_reply *reply)
        {
            dassert(context == (void*)this, "context must be this");

            cluster_list clist;
            std::string format;
            if (argc < 1 || ERR_OK != unmarshall_json(argv[0], "format", format))
            {
                //TODO: need raise error here?
            }
            else
            {
                on_get_cluster_list_internal(format, clist);
            }

            std::string* resp_json = new std::string();
            *resp_json = marshall_json(clist);
            reply->context = resp_json;
            reply->message = (const char*)resp_json->c_str();
            reply->size = resp_json->size();
            return;
        }

        void deploy_svc_service_impl::on_get_cluster_list(const std::string& format, ::dsn::rpc_replier<cluster_list>& reply)
        {
            cluster_list clist;

            on_get_cluster_list_internal(format, clist);

            reply(clist);
        }

        std::shared_ptr<::dsn::dist::deployment_unit> deploy_svc_service_impl::get_service(const std::string& name)
        {
            ::dsn::service::zauto_read_lock l(_service_lock);
            auto it = _services.find(name);
            if (it != _services.end())
            {
                return it->second;
            }
            else
            {
                return nullptr;
            }
        }

        std::shared_ptr<deploy_svc_service_impl::cluster_ex> deploy_svc_service_impl::get_cluster(const std::string& name)
        {
            ::dsn::service::zauto_read_lock l(_cluster_lock);
            auto it = _clusters.find(name);
            if (it != _clusters.end())
            {
                return it->second;
            }
            else
            {
                return nullptr;
            }
        }

    }
}
