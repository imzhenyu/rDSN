<?php
require_once($argv[1]); // type.php
require_once($argv[2]); // program.php
$file_prefix = $argv[3];
$idl_type = $argv[4];
?>

# include "<?=$file_prefix?>.app.example.h"

void dsn_app_registration_<?=$_PROG->name?>()
{
    // register all possible service apps
    dsn::register_app< <?=$_PROG->get_cpp_namespace().$_PROG->name?>_server_app>("<?=$_PROG->name?>");
    // if replicated, using 
    // dsn::register_app_with_type_1_replication_support< <?=$_PROG->get_cpp_namespace().$_PROG->name?>_service_impl>("<?=$_PROG->name?>");
    
    dsn::register_app< <?=$_PROG->get_cpp_namespace().$_PROG->name?>_client_app>("<?=$_PROG->name?>.client");
    dsn::register_app< <?=$_PROG->get_cpp_namespace().$_PROG->name?>_perf_test_client_app>("<?=$_PROG->name?>.client.perf");
}

int main(int argc, char** argv)
{
    dsn_app_registration_<?=$_PROG->name?>();
    dsn_run(argc, argv, true);
    return 0;
}
