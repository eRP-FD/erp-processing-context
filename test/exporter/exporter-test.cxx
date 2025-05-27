/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/fhir/Fhir.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Environment.hxx"
#include "shared/util/GLogConfiguration.hxx"

#include <gtest/gtest.h>
#include <span>

int main(int argc, char** argv)
{
    if (!Environment::get("ERP_VLOG_MAX_VALUE"))
    {
        Environment::set("ERP_VLOG_MAX_VALUE", "1");
    }
    Environment::set("ERP_SERVER_HOST", "127.0.0.1");

    auto args = std::span(argv, size_t(argc));
    GLogConfiguration::initLogging(args[0]);
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
