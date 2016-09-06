<?php
require_once($argv[1]); // type.php
require_once($argv[2]); // program.php
$file_prefix = $argv[3];
$idl_type = $argv[4];
$idl_format = $argv[5];

$default_serialize_format = "DSF";
if ($idl_type == "thrift")
{
    $default_serialize_format = $default_serialize_format."_THRIFT";
} else
{
    $default_serialize_format = $default_serialize_format."_PROTOC";
}
$default_serialize_format = $default_serialize_format."_".strtoupper($idl_format);

?>
[modules]
dsn.tools.common
dsn.tools.hpc
dsn.tools.nfs
<?=$_PROG->name?> 

[apps.server]
type = <?=$_PROG->name?> 
arguments = 
ports = %port%
pools = THREAD_POOL_DEFAULT
run = true

[core]
start_nfs = true

tool = nativerun
;tool = simulator
;toollets = tracer
;toollets = tracer,profiler,fault_injector
toollets = %toollets%
pause_on_start = false

logging_start_level = LOG_LEVEL_WARNING
logging_factory_name = dsn::tools::hpc_logger

[network]
; how many network threads for network library(used by asio)
io_service_worker_count = 2

; specification for each thread pool
[threadpool..default]
worker_count = 2
worker_priority = THREAD_xPRIORITY_LOWEST

[threadpool.THREAD_POOL_DEFAULT]
name = default
partitioned = false
max_input_queue_length = 1024
worker_priority = THREAD_xPRIORITY_LOWEST

[threadpool.THREAD_POOL_REPLICATION]
name = replication
partitioned = true
max_input_queue_length = 2560
worker_priority = THREAD_xPRIORITY_LOWEST

[task..default]
is_trace = true
is_profile = true
allow_inline = false
rpc_call_channel = RPC_CHANNEL_TCP
rpc_message_header_format = dsn
rpc_timeout_milliseconds = 5000

disk_write_fail_ratio = 0.0
disk_read_fail_ratio = 0.00001

perf_test_seconds = 30
perf_test_payload_bytes = 1,128,1024

[task.LPC_AIO_IMMEDIATE_CALLBACK]
is_trace = false
allow_inline = false
disk_write_fail_ratio = 0.0

[task.LPC_RPC_TIMEOUT]
is_trace = false

[task.LPC_CHECKPOINT_REPLICA]
;execution_extra_delay_us_max = 10000000

[task.LPC_LEARN_REMOTE_DELTA_FILES]
;execution_extra_delay_us_max = 10000000

[task.RPC_FD_FAILURE_DETECTOR_PING]
is_trace = false
rpc_call_channel = RPC_CHANNEL_UDP

[task.RPC_FD_FAILURE_DETECTOR_PING_ACK]
is_trace = false
rpc_call_channel = RPC_CHANNEL_UDP

[task.LPC_BEACON_CHECK]
is_trace = false

[task.RPC_PREPARE]
rpc_request_resend_timeout_milliseconds = 8000

[replication]

prepare_timeout_ms_for_secondaries = 10000
prepare_timeout_ms_for_potential_secondaries = 20000

learn_timeout_ms = 30000
staleness_for_commit = 20
staleness_for_start_prepare_for_potential_secondary = 110
mutation_max_size_mb = 15
mutation_max_pending_time_ms = 20
mutation_2pc_min_replica_count = 2

prepare_list_max_size_mb = 250
request_batch_disabled = false
group_check_internal_ms = 100000
group_check_disabled = false
fd_disabled = false
fd_check_interval_seconds = 5
fd_beacon_interval_seconds = 3
fd_lease_seconds = 14
fd_grace_seconds = 15
working_dir = .
log_buffer_size_mb = 1
log_pending_max_ms = 100
log_file_size_mb = 32
log_batch_write = true

log_enable_shared_prepare = true
log_enable_private_commit = false

config_sync_interval_ms = 60000
