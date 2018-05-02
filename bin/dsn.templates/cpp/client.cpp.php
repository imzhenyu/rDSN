<?php
require_once($argv[1]); // type.php
require_once($argv[2]); // program.php
$file_prefix = $argv[3];
$idl_type = $argv[4];
?>

# include "<?=$file_prefix?>.client.h"
# include <dsn/cpp/zlocks.h>

int main(int argc, char** argv)
{
    std::cout << "<usage>: <?=$_PROG->name?>.client <?=$_PROG->name?>.client.config.ini [...see dsn_run...]" << std::endl;

    // initialize dsn system
    dsn_run(argc, argv, false);
    
    // run client
    auto ch = dsn_rpc_channel_open(
            "raw://localhost:27101/<?=$_PROG->name?>1",
            "NET_CHANNEL_TCP", 
            "NET_HDR_DSN" // NET_HDR_HTTP, ...
            );

<?php foreach ($_PROG->services as $svc) { ?>

    <?=$_PROG->get_cpp_namespace().$svc->name?>_client <?=$svc->name?>_c(ch);
<?php foreach ($svc->functions as $f) {?>
<?php if ($f->is_one_way()) { ?>
    <?=$svc->name?>_c.<?=$f->name?>({});
<?php } else { ?>
    {
        //sync:
        auto result = <?=$svc->name?>_c.<?=$f->name?>_sync({});
        std::cout << "call <?=$f->get_rpc_code()?> end, return " << result.first.to_string() << std::endl;
        //async: 
        //<?=$svc->name?>_c.<?=$f->name?>(...);
    }
<?php } ?>
<?php } ?>
<?php } ?>
	
	
    dsn_rpc_channel_close(ch);

    dsn_exit(0);

    return 0;
}
