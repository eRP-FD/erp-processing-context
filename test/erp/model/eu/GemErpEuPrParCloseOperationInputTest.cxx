/*
* (C) Copyright IBM Deutschland GmbH 2021, 2025
* (C) Copyright IBM Corp. 2021, 2025
*
* non-exclusively licensed to gematik GmbH
*/

#include "erp/model/eu/GemErpEuPrParCloseOperationInput.hxx"
#include "erp/model/eu/GemErpEuPrMedication.hxx"
#include "shared/model/GemErpEuPrMedicationDispense.hxx"
#include "erp/model/eu/GemErpEuPrTaskInput.hxx"
#include "shared/model/Timestamp.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>


TEST(GemErpEuPrParCloseOperationInputTest, test) {
    auto xmlStr = ResourceTemplates::euCloseTaskXml({
        .kvnr = "X123456789",
        .prescriptionId = "160.000.000.004.715.74",
        .whenHandedOver = "2025-10-01",
        .accessCode = "ABC123",
        .countryCode = "BE",
    });
    std::optional<model::GemErpEuPrParCloseOperationInput> resource;
    ASSERT_NO_THROW(resource =
                        model::GemErpEuPrParCloseOperationInput::fromXml(xmlStr, *StaticData::getXmlValidator()));
    ASSERT_TRUE(resource);
    EXPECT_STREQ("X123456789", resource->kvnr().id().c_str());
    EXPECT_STREQ("ABC123", std::string{resource->accessCode().toString()}.c_str());
    EXPECT_STREQ("BE", resource->countryCode().toString().c_str());
    EXPECT_STREQ("Sanches", std::string{resource->practitionerName()}.c_str());
    EXPECT_STREQ("2262", std::string{resource->practitionerRole()}.c_str());

    auto s = resource->practitionerId(resource->practitionerReference("PractitionerRole/Example-EU-PractitionerRole"));
    std::cout << s;
    EXPECT_STREQ("Super Pharmacia", std::string{resource->pointOfCare()}.c_str());
    EXPECT_STREQ("1.2.276.0.76.4.54", std::string{resource->healthcareFacilityType()}.c_str());
}
