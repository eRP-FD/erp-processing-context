/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/fhir/Fhir.hxx"
#include "shared/util/Environment.hxx"
#include "shared/util/TLog.hxx"

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

    Fhir::init<ConfigurationBase::ERP>(Fhir::Init::later);

    return RUN_ALL_TESTS();
}
