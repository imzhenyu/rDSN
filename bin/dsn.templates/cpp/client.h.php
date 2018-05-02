<?php
require_once($argv[1]); // type.php
require_once($argv[2]); // program.php
$file_prefix = $argv[3];
$_IDL_FORMAT = $argv[4];
?>
# pragma once
# include "<?=$file_prefix?>.code.definition.h"
# include <iostream>


<?=$_PROG->get_cpp_namespace_begin()?>

<?php foreach ($_PROG->services as $svc) { ?>
class <?=$svc->name?>_client
{
public:
    explicit <?=$svc->name?>_client(dsn_channel_t ch) { _channel = ch; }
    virtual ~<?=$svc->name?>_client() {}

<?php foreach ($svc->functions as $f) { ?>

    // ---------- call <?=$f->get_rpc_code()?> ------------
<?php    if ($f->is_one_way()) {?>
    void <?=$f->name?>(
        const <?=$f->get_cpp_request_type_name()?>& args,
        int thread_hash = 0, 
        uint64_t partition_hash = 0
        )
    {
        ::dsn::rpc::call_one_way_typed(_channel,
            <?=$f->get_rpc_code()?>, args, thread_hash, partition_hash);
    }
<?php    } else { ?>
    // - synchronous
    std::pair< ::dsn::error_code, <?=$f->get_cpp_return_type()?>> <?=$f->name?>_sync(
        const <?=$f->get_cpp_request_type_name()?>& args,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0),
        int thread_hash = 0,
        uint64_t partition_hash = 0
        )
    {
        return ::dsn::rpc::call_wait< <?=$f->get_cpp_return_type()?>, <?=$f->get_cpp_request_type_name()?>>(
                _channel,
                <?=$f->get_rpc_code()?>,
                args,
                timeout,
                thread_hash,
                partition_hash
				);
    }

    // - asynchronous with on-stack <?=$f->get_cpp_request_type_name()?> and <?=$f->get_cpp_return_type()?> 
    // TCallback - (error_code err, <?=$f->get_cpp_return_type()?>&& resp){} 
    // TCallback - bool (dsn_rpc_error_t, dsn_message_t resp) {}
    template<typename TCallback> 
    void <?=$f->name?>(
        const <?=$f->get_cpp_request_type_name()?>& args,
        TCallback&& callback,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0),
        int request_thread_hash = 0, 
        uint64_t request_partition_hash = 0,
        int reply_thread_hash = 0
        )
    {
        return ::dsn::rpc::call(
                    _channel,
                    <?=$f->get_rpc_code()?>,
                    args,
                    std::forward<TCallback>(callback),
                    timeout,
                    request_thread_hash,
                    request_partition_hash,
                    reply_thread_hash
                    );
    }
<?php    }?>
<?php } ?>

private:
    dsn_channel_t _channel;
};

<?php } ?>
<?=$_PROG->get_cpp_namespace_end()?>
