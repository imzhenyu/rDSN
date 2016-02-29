<?php
require_once($argv[1]); // type.php
require_once($argv[2]); // program.php
$file_prefix = $argv[3];
$idl_type = $argv[4];
$idl_format = $argv[5];
?>
# pragma once
# include <dsn/service_api_cpp.h>

//
// uncomment the following line if you want to use 
// data encoding/decoding from the original tool instead of rDSN
// in this case, you need to use these tools to generate
// type files with --gen=cpp etc. options
//
// !!! WARNING: not feasible for replicated service yet!!! 
//

<?php if ($idl_type == "thrift") { ?>

# include <dsn/thrift_helper.h>
# include "<?=$_PROG->name?>_types.h" 

<?php } else if ($idl_type == "proto") {?>

<?php       if ($idl_format == "binary") {?>
// define DSN_IDL_BINARY to use binary format to send rpc request/response
#define DSN_IDL_BINARY
<?php       } else if ($idl_format == "json") {?>
// define DSN_IDL_JSON to use json format to send rpc request/response
#define DSN_IDL_JSON

<?php       }?>
# include "<?=$_PROG->name?>.pb.h"
# include <dsn/idl/gproto_helper.h>

<?php } else { ?>
// error not supported idl type <?=$idl_type?>
// use rDSN's data encoding/decoding

<?php
echo $_PROG->get_cpp_namespace_begin().PHP_EOL;

foreach ($_PROG->enums as $em) 
{
    echo "    // ---------- ". $em->name . " -------------". PHP_EOL;
    echo "    enum ". $em->get_cpp_name() .PHP_EOL;
    echo "    {".PHP_EOL;
    foreach ($em->values as $k => $v) {
        echo "        ". $k . " = " .$v ."," .PHP_EOL;
    }
    echo "    };".PHP_EOL;
    echo PHP_EOL;
    echo "    DEFINE_POD_SERIALIZATION(". $em->get_cpp_name() .");".PHP_EOL;
    echo PHP_EOL;
}

foreach ($_PROG->structs as $s) 
{
    echo "    // ---------- ". $s->name . " -------------". PHP_EOL;
    echo "    struct ". $s->get_cpp_name() .PHP_EOL;
    echo "    {".PHP_EOL;
    foreach ($s->fields as $fld) {
        echo "        ". $fld->get_cpp_type() . " " .$fld->name .";" .PHP_EOL;
    }
    echo "    };".PHP_EOL;
    echo PHP_EOL;
    echo "    inline void marshall(::dsn::binary_writer& writer, const ". $s->get_cpp_name() . "& val)".PHP_EOL;
    echo "    {".PHP_EOL;
    foreach ($s->fields as $fld) {
        echo "        marshall(writer, val." .$fld->name .");" .PHP_EOL;
    }
    echo "    }".PHP_EOL;
    echo PHP_EOL;
    echo "    inline void unmarshall(::dsn::binary_reader& reader, /*out*/ ". $s->get_cpp_name() . "& val)".PHP_EOL;
    echo "    {".PHP_EOL;
    foreach ($s->fields as $fld) {
        echo "        unmarshall(reader, val." .$fld->name .");" .PHP_EOL;
    }
    echo "    }".PHP_EOL;
    echo PHP_EOL;
}

echo $_PROG->get_cpp_namespace_end().PHP_EOL;
?>

#endif 
<?php } ?>