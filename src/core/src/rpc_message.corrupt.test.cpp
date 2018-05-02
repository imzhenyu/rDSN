#include <gtest/gtest.h>
#include <dsn/service_api_cpp.h>
#include <iostream>
#include <vector>
#include <string>
#include <dsn/cpp/test_utils.h>

//this only works with the fault injector
TEST(core_fj, corrupt_message)
{
    int req = 0;
    ::dsn::rpc_address server("localhost", 20101);
    auto ch = dsn_rpc_channel_open(server.to_string());

    auto result = ::dsn::rpc::call_wait<std::string>(
        ch,
        RPC_TEST_HASH1,
        req,
        std::chrono::milliseconds(0),
        1
        );
    ASSERT_EQ(result.first, ERR_TIMEOUT);

    result = ::dsn::rpc::call_wait<std::string>(
        ch,
        RPC_TEST_HASH2,
        req,
        std::chrono::milliseconds(0),
        1
        );
    ASSERT_EQ(result.first, ERR_TIMEOUT);

    result = ::dsn::rpc::call_wait<std::string>(
        ch,
        RPC_TEST_HASH3,
        req,
        std::chrono::milliseconds(0),
        1
        );
    ASSERT_EQ(result.first, ERR_TIMEOUT);

    result = ::dsn::rpc::call_wait<std::string>(
        ch,
        RPC_TEST_HASH4,
        req,
        std::chrono::milliseconds(0),
        1
        );
    ASSERT_EQ(result.first, ERR_TIMEOUT);

    dsn_rpc_channel_close(ch);
}
