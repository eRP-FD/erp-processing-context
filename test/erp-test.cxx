/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/Environment.hxx"
#include "erp/util/TLog.hxx"
#include "workflow-test/EndpointTestClient.hxx"
#include "test/mock/MockTerminationHandler.hxx"

#include <gtest/gtest.h>
#include <chrono>


using namespace ::std::chrono_literals;

int main(int argc, char** argv)
{
    Environment::set("ERP_VLOG_MAX_VALUE", "1");
    Environment::set("ERP_SERVER_HOST", "127.0.0.1");
    Environment::set("ERP_FEATURE_PKV", "true");

    const auto yesterday = (::model::Timestamp::now() + -24h).toXsDateTime();
    Environment::set("ERP_FHIR_PROFILE_VALID_FROM", yesterday);
    Environment::set("ERP_FHIR_PROFILE_RENDER_FROM", yesterday);
    Environment::set("ERP_FHIR_PROFILE_OLD_VALID_UNTIL", yesterday);

    TestClient::setFactory(&EndpointTestClient::factory);
    GLogConfiguration::init_logging(argv[0]);
    ThreadNames::instance().setCurrentThreadName("test");
    ::testing::InitGoogleTest(&argc, argv);
    ThreadNames::instance().setCurrentThreadName("test-runner");
    MockTerminationHandler::setupForTesting();
    return RUN_ALL_TESTS();
}
