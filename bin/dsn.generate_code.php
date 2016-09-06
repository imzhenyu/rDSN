<?php

function usage()
{
    echo "dsn.cg %name%.thrift|.proto cpp|csharp|js %out_dir% [format = binary|json] [mode = single]".PHP_EOL;
    echo "\tformat - use binary(default) or json format to send rpc request/response".PHP_EOL;
    echo "\tnotice : currently for js we only support json format of thrift".PHP_EOL;
}

if (count($argv) < 4)
{
    usage();
    exit(0);
}

global $g_idl;
global $g_out_dir;
global $g_cg_dir;
global $g_php_path;
global $g_cg_libs;
global $g_idl_type;
global $g_idl_post;
global $g_program;
global $g_idl_php;
global $g_idl_format;
global $g_mode;

$g_idl = $argv[1];
$g_lang = $argv[2];
$g_out_dir = $argv[3];
$g_cg_dir = __DIR__;
$g_templates = $g_cg_dir."/dsn.templates";
$g_php_path = "php";
$g_idl_type = "";
$g_idl_post = "";
$g_program = "";
$g_idl_php = "";
$g_idl_format = "";

if (strtoupper(substr(PHP_OS, 0, 3)) === 'WIN') 
{
    $g_php_path = $g_cg_dir."/Windows/php.exe";
}

if (count($argv) >= 5)
{
    $format_input = $argv[4];
    if ($format_input == "json")
    {
        $g_idl_format = "json";
    }
    else if ($format_input == "binary")
    {
        $g_idl_format = "binary";
    }
    else
    {
        echo "invalid format '$format_input'".PHP_EOL;
        usage();
        exit(0);
    }
}
else
{
    $g_idl_format = "binary";
}

if (count($argv) >= 6)
{
    $g_mode = $argv[5];
}
else
{
    $g_mode = "single";
}
    
/*if ($g_mode != "single" && $g_mode != "replication" && $g_mode != "layer3")
{
    echo "invalid mode '$g_mode'".PHP_EOL;
    usage();
    exit(0);
}*/

if (!file_exists($g_idl))
{
    echo "input file '". $g_idl ."' is not found.".PHP_EOL;
    exit(0);
}
else
{
    if (strlen($g_idl) > strlen(".thrift")
      && substr($g_idl, strlen($g_idl) - strlen(".thrift")) == ".thrift")
    {
        $g_idl_type = "thrift";
        $g_idl_post = ".php";
    }
    else if (strlen($g_idl) > strlen(".proto")
      && substr($g_idl, strlen($g_idl) - strlen(".proto")) == ".proto")
    {
        $g_idl_type = "proto";
        $g_idl_post = ".pb.php";
    }
    else
    {
        echo "unknown idl type for input file '".$g_idl."'".PHP_EOL;
        exit(0);
    }
}

if ($g_lang != "cpp" && $g_lang != "csharp" && $g_lang != "js")
{
    echo "unsupported language : ".$g_lang.PHP_EOL;
    exit(0);
}

if ($g_lang == "js" && ($g_idl_type != "thrift" || $g_idl_format != "json"))
{
    echo "currently for js we only support json format of thrift, please check your arguments.".PHP_EOL;
    exit(0);
}

$pos = strrpos($g_idl, "\\");
$pos2 = strrpos($g_idl, "/");
if ($pos == FALSE && $pos2 == FALSE)
{
    $g_program = substr($g_idl, 0, strlen($g_idl) - strlen($g_idl_type) - 1);
}
else if ($pos != FALSE)
{
    $g_program = substr($g_idl, $pos + 1, strlen($g_idl) - $pos - 1  - strlen($g_idl_type) - 1);
}
else
{
    $g_program = substr($g_idl, $pos2 + 1, strlen($g_idl) - $pos2 - 1  - strlen($g_idl_type) - 1);
}

$g_idl_php = $g_out_dir."/".$g_program.$g_idl_post;

if (!file_exists($g_out_dir))
{
    if (!mkdir($g_out_dir))
    {
        echo "create output directory '". $g_out_dir ."' failed.".PHP_EOL;
        exit(0);
    }
    else
    {
        echo "output directory '". $g_out_dir ."' created.".PHP_EOL;
    }
}

// generate service definition file from input idl file using the code generation tools
$os_name = explode(" ", php_uname())[0];
switch ($g_idl_type)
{
case "thrift":
    {
        $command = $g_cg_dir."/".$os_name."/thrift --gen rdsn -out ".$g_out_dir." ".$g_idl;
        echo "exec: ".$command.PHP_EOL;
        system($command);
        if (!file_exists($g_idl_php))
        {
            echo "failed invoke thrift tool to generate '".$g_idl_php."'".PHP_EOL;
            exit(0);
        }
        //we expect the cpp to generate the moveable types
        $lang_with_options = $g_lang;
        if ($g_lang == "cpp")
        {
            $lang_with_options = $g_lang.":moveable_types";
        }
        $command = $g_cg_dir."/".$os_name."/thrift --gen ".$lang_with_options." -out ".$g_out_dir." ".$g_idl;
        echo "exec: ".$command.PHP_EOL;
        system($command);
    }
    break;
case "proto":
    {
        $g_idl_dir = dirname($g_idl);
        $command = $g_cg_dir."/".$os_name."/protoc --rdsn_out=".$g_out_dir." ".$g_idl." -I=".$g_idl_dir;
        echo "exec: ".$command.PHP_EOL;
        system($command);
        if (!file_exists($g_idl_php))
        {
            echo "failed invoke protoc tool to generate '".$g_idl_php."'".PHP_EOL;
            exit(0);
        }

        $command = $g_cg_dir."/".$os_name."/protoc --".$g_lang."_out=".$g_out_dir." ".$g_idl." -I=".$g_idl_dir;
        echo "exec: ".$command.PHP_EOL;
        system($command);
    }
    break;
default:
    echo "idl type '". $g_idl_type ."' not supported yet!".PHP_EOL;
    exit(0);
}

// load annotations when they are present
if (file_exists($g_idl.".annotations"))
{
    $annotations = parse_ini_file($g_idl.".annotations", true);
    if (FALSE == $annotations)
    {
        echo "read annotation file $g_idl.annotations failed".PHP_EOL;
        exit(0);
    }
    
    $as = "<?php".PHP_EOL;
    $as .= "\$_PROG->add_annotations(Array(".PHP_EOL;
    foreach ($annotations as $s => $kvs)
    {
        $as .= "\t\"".$s."\" => Array(".PHP_EOL;
        foreach($kvs as $k => $v)
        {
            $as .= "\t\t\"".$k."\" => \"". $v ."\",".PHP_EOL;
        }        
        $as .= "\t),".PHP_EOL;
    }
    $as .= "));".PHP_EOL;
    $as .= "?>".PHP_EOL;
    
    file_put_contents($g_idl_php, $as, FILE_APPEND);
}

function generate_files_from_dir($dr)
{
    global $g_templates;
    global $g_idl_php;
    global $g_program;
    global $g_out_dir;
    global $g_idl_type;
    global $g_idl_format;
    global $g_php_path;
    
    foreach (scandir($dr) as $template)
    {
        if ($template == "type.php" 
            || $template == "." 
            || $template == ".." 
            )
            continue;
            
        if (is_dir($dr."/".$template))
            continue;

        // special files with which prefix are not neded
        if ($template == "config.ini.php"
         || $template == "CMakeLists.txt.php"
         || $template == "App.config.php"
         || $template == "Dockerfile.php"
         || $template == "run.cmd.php"
         || $template == "config.appstore.ini.php"
           )
            $output_file = $g_out_dir."/".substr($template, 0, strlen($template)-4);
        else
            $output_file = $g_out_dir."/".$g_program.".".substr($template, 0, strlen($template)-4);

        $command = $g_php_path." -f ".$dr."/".$template
                    ." ".$g_templates."/type.php"
                    ." ".$g_idl_php
                    ." ".$g_program
                    ." ".$g_idl_type
                    ." ".$g_idl_format
                    ." >".$output_file
                    ;
        
        //echo "exec: ".$command.PHP_EOL;
        system($command);
        if (!file_exists($output_file))
        {
            echo "failed to generate '".$output_file."'".PHP_EOL;
            exit(0);
        }
        else
        {
            echo "generate '".$output_file."' successfully!".PHP_EOL;
        }
    }
}

// generate all files 
if (!file_exists($g_templates."/".$g_lang))
{
    echo "specified language '" . $g_lang. "' is not supported".PHP_EOL;
    exit(0);
}

generate_files_from_dir($g_templates."/".$g_lang);
generate_files_from_dir($g_templates."/".$g_lang."/".$g_mode);

// copy additional files
$add_idl_file_name= "";
if ($g_idl_type == "proto" && $g_lang == "csharp")
{
    if ($g_idl_format == "json")
    {
        $add_idl_file_name = "GProtoJsonHelper.cs";
    }
    else
    {
        $add_idl_file_name = "GProtoBinaryHelper.cs";
    }
} else if ($g_idl_type == "thrift" && $g_lang == "csharp")
{
    if ($g_idl_format == "json")
    {
        $add_idl_file_name = "ThriftJsonHelper.cs";
    }
    else
    {
        $add_idl_file_name = "ThriftBinaryHelper.cs";
    }
}
if ($add_idl_file_name != "")
{
    $dsn_root = getenv('DSN_ROOT');
    $add_file = $dsn_root."/include/dsn/idl/".$add_idl_file_name;
    $target = $g_out_dir."/".$add_idl_file_name;
    if (!copy($add_file, $target))
    {
        echo "failed to copy ".$add_file;
        exit(0);
    }
}
?>
