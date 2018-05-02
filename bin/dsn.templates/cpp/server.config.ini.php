<?php
require_once($argv[1]); // type.php
require_once($argv[2]); // program.php
$file_prefix = $argv[3];
?>
; enter 'config-dump config.full.txt' to dump full config on program console
[modules]
dsn.tools.common
dsn.tools.emulator

[apps.mimic]
ports = 27101, 27201

[core]
; logs with level below this will not be logged
;logging_start_level = LOG_LEVEL_INFORMATION
logging_start_level = LOG_LEVEL_WARNING

; logging provider
logging_factory_name = dsn::tools::screen_logger
; logging_factory_name = dsn::tools::hpc_tail_logger
; logging_factory_name = dsn::tools::simple_logger

; use what tool to run this process, e.g., nativerun or emulator
tool = nativerun

; use what toollets, e.g., tracer, profiler, fault_injector
; toollets = tracer 

pause_on_start = false

; [ enable single thread server as follows 
cli_local = false
cli_remote = false 

[task.LPC_CONTROL_SERVICE_APP]
pool_code = THREAD_POOL_IO

[threadpool.THREAD_POOL_DEFAULT]
worker_count = 0 

[threadpool.THREAD_POOL_IO]
worker_count = 1
partitioned = true
worker_priority = THREAD_xPRIORITY_ABOVE_NORMAL
worker_share_core = false
name = io

; ] // end enable single thread server 

<?php
foreach ($_PROG->services as $svc)
{
    foreach ($svc->functions as $f)
    {
        echo "[task.". $f->get_rpc_name() ."]".PHP_EOL;
        echo "allow_inline = true".PHP_EOL;
        echo "".PHP_EOL;
    }
}
?>
