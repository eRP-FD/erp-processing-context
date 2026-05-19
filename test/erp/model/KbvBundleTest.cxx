/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/ErpRequirements.hxx"
#include "shared/model/KbvBundle.hxx"
#include "shared/model/ResourceNames.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>


TEST(KbvBundleTest, PatientIdentifierSystem)
{
    A_23936_01.test("A_23936 / ANFERP-3322 Anpassung Regel für kbv 1.3.0 Profile");
    using namespace std::string_literals;
    auto kbvBundle = model::KbvBundle::fromXmlNoValidation(ResourceTemplates::kbvBundleXml({
        .kbvVersion = ResourceTemplates::Versions::KBV_ERP_1_3_3,
        .forceKvid10Ns = "http://fhir.de/sid/pkv/kvid-10",
    }));
    std::string message = "Als Identifier für den Patienten muss eine VersichertenID (KVNR) angegeben werden.";
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(kbvBundle.additionalValidation(), HttpStatus::BadRequest, message.c_str());
}


TEST(KbvBundleTest, ResourceTemplate_1_4_0)
{
    using namespace std::string_literals;
    testutils::ShiftFhirResourceViewsGuard shift{"KBV_1_4", date::floor<date::days>(std::chrono::system_clock::now())};
    ASSERT_NO_THROW((void) model::KbvBundle::fromXml(
        ResourceTemplates::kbvBundleXml({.kbvVersion = ResourceTemplates::Versions::KBV_ERP_1_4_2}),
        *StaticData::getXmlValidator()));
}
