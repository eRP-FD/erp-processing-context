#include "erp/util/Environment.hxx"
#include "erp/util/TLog.hxx"
#include "workflow-test/EndpointTestClient.hxx"
#include "test/mock/MockTerminationHandler.hxx"

#include <gtest/gtest.h>


int main (int argc, char** argv)
{
    Environment::set("ERP_VLOG_MAX_VALUE", "1");
    Environment::set("ERP_SERVER_HOST", "127.0.0.1");

    TestClient::setFactory([]{return std::make_unique<EndpointTestClient>();});
    GLogConfiguration::init_logging(argv[0]);
    ThreadNames::instance().setCurrentThreadName("test");
    ::testing::InitGoogleTest(&argc, argv);
    ThreadNames::instance().setCurrentThreadName("test-runner");
    MockTerminationHandler::setupForTesting();
    return RUN_ALL_TESTS();
}
