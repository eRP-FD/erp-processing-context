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


int main (int argc, char** argv)
{
    Environment::set("ERP_VLOG_MAX_VALUE", "1");
    TestClient::setFactory(&HttpsTestClient::factory);

    GLogConfiguration::init_logging(argv[0]);
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::Test::RecordProperty("BuildVersion", ErpServerInfo::BuildVersion().data());
    ::testing::Test::RecordProperty("BuildType", ErpServerInfo::BuildType().data());
    ::testing::Test::RecordProperty("ReleaseVersion", ErpServerInfo::ReleaseVersion().data());
    ::testing::Test::RecordProperty("ReleaseDate", ErpServerInfo::ReleaseDate().data());

    Fhir::init<ConfigurationBase::ERP>(Fhir::Init::later);
    return RUN_ALL_TESTS();
}
