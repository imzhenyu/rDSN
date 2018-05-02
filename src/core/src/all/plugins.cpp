
# include <dsn/utility/module_init.cpp.h>
# include <dsn/service_api_c.h>

# ifndef __TITLE__
# define __TITLE__ "dsn.plugins"
# endif

void link_all_dsn_plugins()
{
    MODULE_LINK(tools_common);
    MODULE_LINK(tools_emulator);
    MODULE_LINK(tools_http);
}
