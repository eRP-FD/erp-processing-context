/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/util/Environment.hxx"
#include "erp/util/TLog.hxx"
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
    static constexpr auto erp_profiles = "--erp_profiles="sv;
    static constexpr auto old_profiles = "2022-01-01"sv;
    static constexpr auto new_profiles = "2023-07-01"sv;
    static constexpr auto patched_profiles = "2024-01-01"sv;
    static constexpr auto all_profiles = "all"sv;

    static constexpr auto erp_instance = "--erp_instance="sv;
    static constexpr size_t instance_offset = 2;

    if (!Environment::get("ERP_VLOG_MAX_VALUE"))
    {
        Environment::set("ERP_VLOG_MAX_VALUE", "1");
    }
    GLogConfiguration::init_logging(argv[0]);
    Environment::set("ERP_SERVER_HOST", "127.0.0.1");

    std::string_view supportedProfiles = old_profiles;
    std::optional<size_t> instance;
    for (int i = 0; i < argc; ++i)
    {
        std::string_view arg{argv[i]};
        if (arg.starts_with(erp_profiles))
        {
            supportedProfiles = arg.substr(erp_profiles.size());
        }
        else if (arg.starts_with(erp_instance))
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
    if (supportedProfiles == old_profiles)
    {
        std::ranges::move(testutils::getOldFhirProfileEnvironment(), std::inserter(env, env.end()));
    }
    else if (supportedProfiles == new_profiles)
    {
        std::ranges::move(testutils::getNewFhirProfileEnvironment(), std::inserter(env, env.end()));
    }
    else if (supportedProfiles == all_profiles)
    {
        std::ranges::move(testutils::getOverlappingFhirProfileEnvironment(), std::inserter(env, env.end()));
    }
    else if (supportedProfiles == patched_profiles)
    {
        std::ranges::move(testutils::getPatchedFhirProfileEnvironment(), std::inserter(env, env.end()));
    }
    else
    {
        std::clog << "missing or invalid argument to " << erp_profiles << '\n';
        std::clog << "    possible values are: ";
        std::clog << old_profiles << ", " << new_profiles << ", " << all_profiles << ", " << patched_profiles
                  << std::endl;
        return EXIT_FAILURE;
    }
    ::testing::InitGoogleTest(&argc, argv);

    TestClient::setFactory(&EndpointTestClient::factory);
    ThreadNames::instance().setCurrentThreadName("test-runner");
    MockTerminationHandler::setupForTesting();
    Configuration::instance().check();
    return RUN_ALL_TESTS();
}
