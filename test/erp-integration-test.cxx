/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/erp-serverinfo.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/util/Environment.hxx"
#include "shared/util/GLogConfiguration.hxx"
#include "workflow-test/HttpsTestClient.hxx"

#include <gtest/gtest.h>
#include <span>


int main (int argc, char** argv)
{
    Environment::set("ERP_VLOG_MAX_VALUE", "1");
    TestClient::setFactory(&HttpsTestClient::factory);

    auto args = std::span(argv, size_t(argc));
    GLogConfiguration::initLogging(args[0]);
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::Test::RecordProperty("BuildVersion", std::string{ErpServerInfo::BuildVersion()});
    ::testing::Test::RecordProperty("BuildType", std::string{ErpServerInfo::BuildType()});
    ::testing::Test::RecordProperty("ReleaseVersion", std::string{ErpServerInfo::ReleaseVersion()});
    ::testing::Test::RecordProperty("ReleaseDate", std::string{ErpServerInfo::ReleaseDate()});

    Fhir::init<ConfigurationBase::ERP>(Fhir::Init::later);
    return RUN_ALL_TESTS();
}
