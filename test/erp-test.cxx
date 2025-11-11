/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/fhir/Fhir.hxx"
#include "shared/util/CrashHandler.hxx"
#include "shared/util/Environment.hxx"
#include "shared/util/TLog.hxx"
#include "workflow-test/EndpointTestClient.hxx"
#include "test/mock/MockTerminationHandler.hxx"
#include "test/util/TestUtils.hxx"

#include <date/tz.h>
#include <gtest/gtest.h>
#include <chrono>


using namespace ::std::chrono_literals;

int main(int argc, char** argv)
{
    using enum ConfigurationKey;
    using namespace std::string_view_literals;
    static constexpr auto erp_shiftto = "--erp_shiftto="sv;
    static constexpr auto erp_testdate = "--erp_testdate="sv;
    static constexpr auto erp_instance = "--erp_instance="sv;
    static constexpr size_t instance_offset = 2;
    CrashHandler::registerSignalHandlers({SIGILL, SIGABRT, SIGSEGV, SIGSYS, SIGFPE});

    if (!Environment::get("ERP_VLOG_MAX_VALUE"))
    {
        Environment::set("ERP_VLOG_MAX_VALUE", "1");
    }
    std::span args{argv, static_cast<size_t>(argc)};
    GLogConfiguration::initLogging(args[0]);
    Environment::set("ERP_SERVER_HOST", "127.0.0.1");

    std::optional<std::string> shiftTo;
    std::optional<std::string> testDate;
    std::optional<size_t> instance;
    for (std::string_view arg : args)
    {
        if (arg.starts_with(erp_testdate) && arg.size() > erp_testdate.size())
        {
            testDate.emplace(arg.substr(erp_testdate.size()));
        }
        else if (arg.starts_with(erp_shiftto))
        {
            shiftTo.emplace(arg.substr(erp_shiftto.size()));
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
    if (testDate && shiftTo)
    {
        LOG(ERROR) << "arguments " << erp_shiftto << " and " << erp_testdate << " are mutually exclusive";
        return EXIT_FAILURE;
    }
    Fhir::init<ConfigurationBase::ERP>(Fhir::Init::later);
    std::optional<testutils::ShiftFhirResourceViewsGuard> shiftGuard;
    if (shiftTo)
    {
        const auto& viewConfig = Configuration::instance().fhirResourceViewConfiguration<ConfigurationBase::ERP>();
        std::set<std::string> availableViews;
        std::ranges::transform(viewConfig.allViews(), std::inserter(availableViews, availableViews.end()),
                               &fhirtools::FhirResourceViewConfiguration::ViewConfig::mId);
        if (availableViews.contains(*shiftTo))
        {
            shiftGuard.emplace(*shiftTo, floor<std::chrono::days>(std::chrono::system_clock::now()));
        }
        else
        {
            LOG(ERROR) << "unknown view \"" << *shiftTo << "\" available views are: " << String::join(availableViews);
            return EXIT_FAILURE;
        }
    }
    else if (testDate && testDate != "today")
    {
        const auto testTimestamp = model::Timestamp::fromXsDate(*testDate, model::Timestamp::UTCTimezone);
        auto now = floor<std::chrono::days>(date::utc_clock::now());
        auto offset = now - floor<std::chrono::days>(date::clock_cast<date::utc_clock>(testTimestamp.toChronoTimePoint()));
        LOG(INFO) << "setting offset: " << offset;
        shiftGuard.emplace(offset);
    }
    ::testing::InitGoogleTest(&argc, argv);

    TestClient::setFactory(&EndpointTestClient::factory);
    ThreadNames::instance().setCurrentThreadName("test-runner");
    MockTerminationHandler::setupForTesting();
    Configuration::instance().check(Configuration::ProcessType::ERP);
    return RUN_ALL_TESTS();
}
