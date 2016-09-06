<?php
require_once($argv[1]); // type.php
require_once($argv[2]); // program.php
$file_prefix = $argv[3];
$_IDL_FORMAT = $argv[4];
?>
# pragma once
# include "<?=$file_prefix?>.client.h"
# include "<?=$file_prefix?>.client.perf.h"
# include "<?=$file_prefix?>.server.h"

<?=$_PROG->get_cpp_namespace_begin()?>

// server app example
class <?=$_PROG->name?>_server_app : 
    public ::dsn::service_app
{
public:
    <?=$_PROG->name?>_server_app(dsn_gpid gpid)
        : ::dsn::service_app(gpid) {}

    virtual ::dsn::error_code start(int argc, char** argv) override
    {
<?php foreach ($_PROG->services as $svc) { ?>
        _<?=$svc->name?>_svc.open_service(gpid());
<?php } ?>
        return ::dsn::ERR_OK;
    }

    virtual ::dsn::error_code stop(bool cleanup = false) override
    {
<?php foreach ($_PROG->services as $svc) { ?>
        _<?=$svc->name?>_svc.close_service(gpid());
<?php } ?>
        return ::dsn::ERR_OK;
    }

private:
<?php foreach ($_PROG->services as $svc) { ?>
    <?=$svc->name?>_service _<?=$svc->name?>_svc;
<?php } ?>
};

// client app example
class <?=$_PROG->name?>_client_app : 
    public ::dsn::service_app, 
    public virtual ::dsn::clientlet
{
public:
    <?=$_PROG->name?>_client_app(dsn_gpid gpid)
        : ::dsn::service_app(gpid) {}
        
    ~<?=$_PROG->name?>_client_app() 
    {
        stop();
    }

    virtual ::dsn::error_code start(int argc, char** argv) override
    {
        if (argc < 1)
        {
            printf ("Usage: <exe> server-host:server-port or service-url\n");
            return ::dsn::ERR_INVALID_PARAMETERS;
        }

        // argv[1]: e.g., dsn://mycluster/simple-kv.instance0
        _server = ::dsn::url_host_address(argv[1]);
            
<?php foreach ($_PROG->services as $svc) { ?>
        _<?=$svc->name?>_client.reset(new <?=$svc->name?>_client(_server));
<?php } ?>
        _timer = ::dsn::tasking::enqueue_timer(<?=$_PROG->get_test_task_code()?>, this, [this]{on_test_timer();}, std::chrono::seconds(1));
        return ::dsn::ERR_OK;
    }

    virtual ::dsn::error_code stop(bool cleanup = false) override
    {
        _timer->cancel(true);
<?php foreach ($_PROG->services as $svc) { ?> 
        _<?=$svc->name?>_client.reset();
<?php } ?>
        return ::dsn::ERR_OK;
    }

    void on_test_timer()
    {
<?php
foreach ($_PROG->services as $svc)
{
    echo "        // test for service '". $svc->name ."'". PHP_EOL;
    foreach ($svc->functions as $f)
{?>
        {
<?php if ($f->is_one_way()) { ?>
            _<?=$svc->name?>_client-><?=$f->name?>({});
<?php } else { ?>
            //sync:
            auto rs = random64(0, 1000000000);
            auto result = _<?=$svc->name?>_client-><?=$f->name?>_sync({}, std::chrono::milliseconds(0), 0, rs);
            std::cout << "call <?=$f->get_rpc_code()?> end, return " << result.first.to_string() << std::endl;
            //async: 
            //_<?=$svc->name?>_client-><?=$f->name?>({});
<?php } ?>           
        }
<?php }    
}
?>
    }

private:
    ::dsn::task_ptr _timer;
    ::dsn::url_host_address _server;
    
<?php foreach ($_PROG->services as $svc) { ?>
    std::unique_ptr<<?=$svc->name?>_client> _<?=$svc->name?>_client;
<?php } ?>
};

<?php foreach ($_PROG->services as $svc) { ?>
class <?=$svc->name?>_perf_test_client_app :
    public ::dsn::service_app, 
    public virtual ::dsn::clientlet
{
public:
    <?=$svc->name?>_perf_test_client_app(dsn_gpid gpid)
        : ::dsn::service_app(gpid)
    {
        _<?=$svc->name?>_client = nullptr;
    }

    ~<?=$svc->name?>_perf_test_client_app()
    {
        stop();
    }

    virtual ::dsn::error_code start(int argc, char** argv) override
    {
        if (argc < 1)
            return ::dsn::ERR_INVALID_PARAMETERS;

        // argv[1]: e.g., dsn://mycluster/simple-kv.instance0
        _server = ::dsn::url_host_address(argv[1]);

        _<?=$svc->name?>_client = new <?=$svc->name?>_perf_test_client(_server);
        _<?=$svc->name?>_client->start_test("<?=$_PROG->name?>.<?=$svc->name?>.perf-test.case.", <?=count($svc->functions)?>);
        return ::dsn::ERR_OK;
    }

    virtual ::dsn::error_code stop(bool cleanup = false) override
    {
        if (_<?=$svc->name?>_client != nullptr)
        {
            delete _<?=$svc->name?>_client;
            _<?=$svc->name?>_client = nullptr;
        }
        
        return ::dsn::ERR_OK;
    }
    
private:
    <?=$svc->name?>_perf_test_client *_<?=$svc->name?>_client;
    ::dsn::rpc_address _server;
};
<?php } ?>

<?=$_PROG->get_cpp_namespace_end()?>
