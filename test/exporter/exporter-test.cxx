/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/fhir/Fhir.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/CrashHandler.hxx"
#include "shared/util/Environment.hxx"
#include "shared/util/GLogConfiguration.hxx"
#include "test/util/TestUtils.hxx"
#include "shared/util/TLog.hxx"

#include <date/tz.h>
#include <gtest/gtest.h>
#include <span>
#include <chrono>


int main(int argc, char** argv)
{
    using namespace std::string_view_literals;
    static constexpr auto erp_testdate = "--erp_testdate="sv;
    CrashHandler::registerSignalHandlers({SIGILL, SIGABRT, SIGSEGV, SIGSYS, SIGFPE});
    if (!Environment::get("ERP_VLOG_MAX_VALUE"))
    {
        Environment::set("ERP_VLOG_MAX_VALUE", "1");
    }
    Environment::set("ERP_SERVER_HOST", "127.0.0.1");

    auto args = std::span(argv, size_t(argc));
    GLogConfiguration::initLogging(args[0]);

    std::optional<std::string> testDate;
    for (std::string_view arg : args)
    {
        if (arg.starts_with(erp_testdate) && arg.size() > erp_testdate.size())
        {
            testDate.emplace(arg.substr(erp_testdate.size()));
        }
    }

    std::optional<testutils::ShiftFhirResourceViewsGuard> shiftGuard;
    if (testDate && testDate != "today")
    {
        const auto testTimestamp = model::Timestamp::fromXsDate(*testDate, model::Timestamp::UTCTimezone);
        auto now = floor<std::chrono::days>(date::utc_clock::now());
        auto offset = now - floor<std::chrono::days>(date::clock_cast<date::utc_clock>(testTimestamp.toChronoTimePoint()));
        LOG(INFO) << "setting offset: " << offset;
        shiftGuard.emplace(offset);
    }

    ::testing::InitGoogleTest(&argc, argv);
    const auto& config = Configuration::instance();
    try
    {
        config.epaFQDNs();
    }
    catch (const std::runtime_error&)
    {
        Environment::set(Configuration::instance().getEnvironmentVariableName(
                             ConfigurationKey::MEDICATION_EXPORTER_EPA_ACCOUNT_LOOKUP_EPA_AS_FQDN),
                         "none:1");
    }
    config.check(Configuration::ProcessType::MedicationExporter);
    Fhir::init<ConfigurationBase::MedicationExporter>(Fhir::Init::later);
    return RUN_ALL_TESTS();
}
