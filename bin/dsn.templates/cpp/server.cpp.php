<?php
require_once($argv[1]); // type.php
require_once($argv[2]); // program.php
$file_prefix = $argv[3];
?>

# include "<?=$file_prefix?>.server.h"

int main(int argc, char** argv)
{
    std::cout << "<usage>: <?=$_PROG->name?>.server <?=$_PROG->name?>.server.config.ini [...see dsn_run...]" << std::endl;

    // initialize dsn system
    dsn_run(argc, argv, false, true); // using main thread for loop 
    
    <?=$_PROG->get_cpp_namespace().$_PROG->name?>_service svc1("<?=$_PROG->name?>1");
    <?=$_PROG->get_cpp_namespace().$_PROG->name?>_service svc2("<?=$_PROG->name?>2");

    svc1.open_service();
    svc2.open_service();

    dsn_loop(); // serve requests 

    std::cout << "Press any key to stop the services ..." << std::endl;
    getchar();

    svc2.close_service();
    svc1.close_service();
    dsn_exit(0);
    return 0;
}
