<?php
require_once($argv[1]); // type.php
require_once($argv[2]); // program.php
$file_prefix = $argv[3];
?>
; enter 'config-dump config.full.txt' to dump full config on program console
; <?=$_PROG->name?> 
[modules]
dsn.tools.common
dsn.tools.emulator

[apps.server]
type = <?=$_PROG->name?> 
arguments = 
ports = 34888
pools = THREAD_POOL_IO, THREAD_POOL_DEFAULT
run = true

[apps.client]
type = <?=$_PROG->name?>.client 
arguments = raw://localhost:34888
pools = THREAD_POOL_IO, THREAD_POOL_DEFAULT
run = true 

[apps.client.perf]
type = <?=$_PROG->name?>.client.perf 
arguments = raw://localhost:34888
pools = THREAD_POOL_IO, THREAD_POOL_DEFAULT
run = false

<?php 
foreach ($_PROG->services as $svc) 
{
    echo "[".$_PROG->name.".".$svc->name.".perf-test.case.1]".PHP_EOL;
    echo "perf_test_seconds  = 360000".PHP_EOL;
    echo "perf_test_key_space_size = 100000".PHP_EOL;
    echo "perf_test_concurrency = 1".PHP_EOL;
    echo "perf_test_payload_bytes = 128".PHP_EOL;
    echo "perf_test_timeouts_ms = 10000".PHP_EOL;
    echo "perf_test_hybrid_request_ratio = ";
    foreach ($svc->functions as $f) echo "1,";
    echo PHP_EOL;
    echo PHP_EOL;
    foreach ($svc->functions as $f) { 
        if ($f->is_write)
        {   
            echo "[task.". $f->get_rpc_code(). "]".PHP_EOL;
            echo "rpc_request_is_write_operation = true".PHP_EOL;
            echo PHP_EOL;
        }
    }
} ?>


[core]
; logs with level below this will not be logged
logging_start_level = LOG_LEVEL_INFORMATION
;logging_start_level = LOG_LEVEL_WARNING

; logging provider
logging_factory_name = dsn::tools::screen_logger
; logging_factory_name = dsn::tools::hpc_tail_logger
; logging_factory_name = dsn::tools::simple_logger

; use what tool to run this process, e.g., nativerun or emulator
tool = nativerun

; use what toollets, e.g., tracer, profiler, fault_injector
toollets = tracer 

pause_on_start = false