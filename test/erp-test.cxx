/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/fhir/Fhir.hxx"
#include "shared/util/Environment.hxx"
#include "shared/util/TLog.hxx"
#include "workflow-test/EndpointTestClient.hxx"
#include "test/mock/MockTerminationHandler.hxx"
#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>
#include <chrono>


using namespace ::std::chrono_literals;

int main(int argc, char** argv)
{
    using enum ConfigurationKey;
    using namespace std::string_view_literals;

    static constexpr auto erp_instance = "--erp_instance="sv;
    static constexpr size_t instance_offset = 2;

    if (!Environment::get("ERP_VLOG_MAX_VALUE"))
    {
        Environment::set("ERP_VLOG_MAX_VALUE", "1");
    }
    auto args = std::span(argv, size_t(argc));
    GLogConfiguration::initLogging(args[0]);
    Environment::set("ERP_SERVER_HOST", "127.0.0.1");

    std::optional<size_t> instance;
    for (std::string_view arg : args)
    {
        if (arg.starts_with(erp_instance))
        {
            instance = std::stoull(std::string{arg.substr(erp_instance.size())});
        }
    }
    std::vector<EnvironmentVariableGuard> env;
    if (instance)
    {

        auto offset = instance_offset * (*instance);
        env.emplace_back("ERP_SERVER_PORT", std::to_string(9190 + offset));
        env.emplace_back(ENROLMENT_ACTIVATE_FOR_PORT, std::to_string(9190 + offset));
        env.emplace_back(ENROLMENT_SERVER_PORT, std::to_string(9191 + offset));
        env.emplace_back(ADMIN_SERVER_PORT, std::to_string(9999 + offset));
        env.emplace_back("TEST_USE_POSTGRES", "off");
    }

    ::testing::InitGoogleTest(&argc, argv);

    TestClient::setFactory(&EndpointTestClient::factory);
    ThreadNames::instance().setCurrentThreadName("test-runner");
    MockTerminationHandler::setupForTesting();
    Configuration::instance().check(Configuration::ProcessType::ERP);
    Fhir::init<ConfigurationBase::ERP>(Fhir::Init::later);
    return RUN_ALL_TESTS();
}
