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
    GLogConfiguration::init_logging(argv[0]);
    if (!Environment::get("ERP_VLOG_MAX_VALUE"))
    {
        Environment::set("ERP_VLOG_MAX_VALUE", "1");
    }
    Environment::set("ERP_SERVER_HOST", "127.0.0.1");
    Environment::set("ERP_FEATURE_PKV", "true");

    ::testing::InitGoogleTest(&argc, argv);
    //google::ParseCommandLineFlags(&argc, &argv, true);

    // The following code can be used to force the new profile versions to be used in all tests.
    // This will be necessary when the profile switch is imminent, i.e. the production server still needs the old
    // profiles but the unit tests and workflow tests shall be already using the new profiles.
//    const auto yesterday = (::model::Timestamp::now() + -24h).toXsDateTime();
//    Environment::set("ERP_FHIR_PROFILE_VALID_FROM", yesterday);
//    Environment::set("ERP_FHIR_PROFILE_RENDER_FROM", yesterday);
//    Environment::set("ERP_FHIR_PROFILE_OLD_VALID_UNTIL", yesterday);
    Environment::set("ERP_SERVICE_GENERIC_VALIDATION_MODE", "require_success");
    Environment::set("ERP_SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION", "reject");
    Environment::set("FHIR_VALIDATION_LEVELS_UNREFERENCED_BUNDLED_RESOURCE", "error");
    Environment::set("FHIR_VALIDATION_LEVELS_UNREFERENCED_CONTAINED_RESOURCE", "error");
    Environment::set("FHIR_VALIDATION_LEVELS_MANDATORY_RESOLVABLE_REFERENCE_FAILURE", "error");

    TestClient::setFactory(&EndpointTestClient::factory);
    ThreadNames::instance().setCurrentThreadName("test");
    ThreadNames::instance().setCurrentThreadName("test-runner");
    MockTerminationHandler::setupForTesting();
    return RUN_ALL_TESTS();
}
