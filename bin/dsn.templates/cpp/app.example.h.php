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
        _<?=$svc->name?>_svc.open_service(get_gpid());
<?php } ?>
        return ::dsn::ERR_OK;
    }

    virtual ::dsn::error_code stop(bool cleanup = false) override
    {
<?php foreach ($_PROG->services as $svc) { ?>
        _<?=$svc->name?>_svc.close_service(get_gpid());
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
    public ::dsn::service_app
{
public:
    <?=$_PROG->name?>_client_app(dsn_gpid gpid)
        : ::dsn::service_app(gpid) { _server_channel = nullptr; }
        
    ~<?=$_PROG->name?>_client_app() 
    {
        stop();
    }

    virtual ::dsn::error_code start(int argc, char** argv) override
    {
        if (argc < 1)
        {
            printf ("Usage: <exe> server-host:server-port\n");
            return ::dsn::ERR_INVALID_PARAMETERS;
        }

        // argv[1]: e.g., raw://localhost:53221
        _server_channel = dsn_rpc_channel_open(argv[1], "NET_CHANNEL_TCP", "NET_HDR_DSN");
            
<?php foreach ($_PROG->services as $svc) { ?>
        _<?=$svc->name?>_client.reset(new <?=$svc->name?>_client(_server_channel));
<?php } ?>
        _timer = ::dsn::tasking::start_timer(<?=$_PROG->get_test_task_code()?>, [this]{on_test_timer();}, std::chrono::seconds(1));
        return ::dsn::ERR_OK;
    }

    virtual ::dsn::error_code stop(bool cleanup = false) override
    {
        ::dsn::tasking::stop_timer(_timer);

        if (_server_channel)
        {
            dsn_rpc_channel_close(_server_channel);
            _server_channel = nullptr;
        }

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
            auto rs = dsn_random64(0, 1000000000);
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
    dsn_timer_t   _timer;
    dsn_channel_t _server_channel;    

<?php foreach ($_PROG->services as $svc) { ?>
    std::unique_ptr<<?=$svc->name?>_client> _<?=$svc->name?>_client;
<?php } ?>
};

// perf client app example
class <?=$_PROG->name?>_perf_test_client_app :
    public ::dsn::service_app 
{
public:
    <?=$_PROG->name?>_perf_test_client_app(dsn_gpid gpid)
        : ::dsn::service_app(gpid)
    {
        _server_channel = nullptr; 
    }

    ~<?=$_PROG->name?>_perf_test_client_app()
    {
        stop();
    }

    virtual ::dsn::error_code start(int argc, char** argv) override
    {
        if (argc < 1)
            return ::dsn::ERR_INVALID_PARAMETERS;

        // argv[1]: e.g., raw://localhost:53221
        _server_channel = dsn_rpc_channel_open(argv[1], "NET_CHANNEL_TCP", "NET_HDR_DSN");
        
<?php foreach ($_PROG->services as $svc) { ?>
        _<?=$svc->name?>_client.reset(new <?=$svc->name?>_perf_test_client(_server_channel));
        _<?=$svc->name?>_client->start_test("<?=$_PROG->name?>.<?=$svc->name?>.perf-test.case.", <?=count($svc->functions)?>);
<?php } ?>
        
        return ::dsn::ERR_OK;
    }

    virtual ::dsn::error_code stop(bool cleanup = false) override
    {
        if (_server_channel)
        {
            dsn_rpc_channel_close(_server_channel);
            _server_channel = nullptr;
        }

<?php foreach ($_PROG->services as $svc) { ?> 
        _<?=$svc->name?>_client.reset();
<?php } ?>
        
        return ::dsn::ERR_OK;
    }
    
private:
<?php foreach ($_PROG->services as $svc) { ?>
    std::unique_ptr<<?=$svc->name?>_perf_test_client> _<?=$svc->name?>_client;
<?php } ?>    
    dsn_channel_t _server_channel;
};

<?=$_PROG->get_cpp_namespace_end()?>
