<?php
require_once($argv[1]); // type.php
require_once($argv[2]); // program.php
$file_prefix = $argv[3];
$idl_type = $argv[4];

$dsf_object = array();
foreach ($_PROG->structs as $s)
{
    array_push($dsf_object, $s->name);
}
foreach ($_PROG->enums as $e)
{
    array_push($dsf_object, $e->name);
}
?>
# pragma once
# include <dsn/service_api_cpp.h>
# include <dsn/cpp/serialization.h>
# include <dsn/cpp/protobuf_helper.h>
# include "<?=$_PROG->name?>.pb.h"

<?=$_PROG->get_cpp_namespace_begin()?>

<?php
foreach ($_PROG->structs as $s)
{
?>
    // <?=$s->name?> 
    inline void marshall(::dsn::binary_writer& writer, const <?=$s->name?>& value)
    {
        marshall_protobuf_binary(writer, value);
    }

    inline void unmarshall(::dsn::binary_reader& reader, /*out*/ <?=$s->name?>& value)
    {
        unmarshall_protobuf_binary(reader, value);
    }

<?php
} 
?>

<?=$_PROG->get_cpp_namespace_end()?>
