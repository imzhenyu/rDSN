
# include <dsn/service_api_c.h>

extern void run_all_unit_tests_prepare_when_necessary();

int main(int argc, char** argv)
{
    run_all_unit_tests_prepare_when_necessary();
    dsn_run(argc, argv, true);
    return 0;
}
