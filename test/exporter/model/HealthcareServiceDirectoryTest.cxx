/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/model/HealthcareServiceDirectory.hxx"
#include "exporter/model/LocationDirectory.hxx"
#include "exporter/model/OrganizationDirectory.hxx"
#include "exporter/model/trezept/ErpTPrescriptionOrganization.hxx"
#include "shared/model/ModelException.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>

class HealthcareServiceDirectoryTest : public testing::Test
{
public:
};

TEST_F(HealthcareServiceDirectoryTest, parseSample)
{
    const auto bundle = model::Bundle::fromJsonNoValidation(ResourceTemplates::vzdFhirBundleJson());
    const auto healthcareServiceEntries = bundle.getResourcesByType<model::HealthcareServiceDirectory>();
    const auto organizationEntries = bundle.getResourcesByType<model::OrganizationDirectory>();
    const auto locationEntries = bundle.getResourcesByType<model::LocationDirectory>();

    EXPECT_EQ(healthcareServiceEntries.size(), 1);
    EXPECT_EQ(organizationEntries.size(), 1);
    EXPECT_EQ(locationEntries.size(), 1);

    const auto& healthcareService = healthcareServiceEntries.at(0);
    EXPECT_EQ(healthcareService.getOrganizationReference(), "Organization/8659cd8b-e047-443c-a56b-06e8b32b185a");
    EXPECT_EQ(healthcareService.getLocationReference(), "Location/1ec93850-62c8-4b6c-a080-1bfc30634a4f");
    EXPECT_EQ(healthcareService.telecomRaw(), nullptr);

    const auto& organization = organizationEntries.at(0);
    EXPECT_EQ(organization.getName(), "Apotheke_Test_Inland_SMCB");

    const auto& location = locationEntries.at(0);
    EXPECT_EQ(location.getAddress(), "Hauptstr. 67, 70477, Stuttgart");
    const auto* addr = location.addressRaw();
    ASSERT_NE(addr, nullptr);
    EXPECT_EQ(rapidjson::Pointer("/state").Get(*addr), nullptr);
}

TEST_F(HealthcareServiceDirectoryTest, healthcareServiceTelecomAndOrganizationSetTelecom)
{
    const std::string hsJson = R"({
        "resourceType": "HealthcareService",
        "id": "hs-123",
        "providedBy": { "reference": "Organization/org-123" },
        "telecom": [
            { "system": "phone", "value": "089-123456", "use": "work" },
            { "system": "email", "value": "info@apotheke.de", "use": "work" }
        ]
    })";

    const auto healthcareService = model::HealthcareServiceDirectory::fromJsonNoValidation(hsJson);
    const auto* telecom = healthcareService.telecomRaw();
    ASSERT_NE(telecom, nullptr);
    ASSERT_TRUE(telecom->IsArray());
    ASSERT_EQ(telecom->Size(), 2);
    EXPECT_EQ(model::NumberAsStringParserDocument::getStringValueFromValue(&((*telecom)[0]["system"])), "phone");
    EXPECT_EQ(model::NumberAsStringParserDocument::getStringValueFromValue(&((*telecom)[0]["value"])), "089-123456");
    EXPECT_EQ(model::NumberAsStringParserDocument::getStringValueFromValue(&((*telecom)[0]["use"])), "work");
    EXPECT_EQ(model::NumberAsStringParserDocument::getStringValueFromValue(&((*telecom)[1]["system"])), "email");
    EXPECT_EQ(model::NumberAsStringParserDocument::getStringValueFromValue(&((*telecom)[1]["value"])), "info@apotheke.de");
    EXPECT_EQ(model::NumberAsStringParserDocument::getStringValueFromValue(&((*telecom)[1]["use"])), "work");

    model::ErpTPrescriptionOrganization org;
    org.setTelecom(*telecom);
    const auto* orgTelecom = rapidjson::Pointer("/telecom").Get(org.jsonDocument());
    ASSERT_NE(orgTelecom, nullptr);
    ASSERT_TRUE(orgTelecom->IsArray());
    ASSERT_EQ(orgTelecom->Size(), 2);
    EXPECT_EQ(model::NumberAsStringParserDocument::getStringValueFromValue(&((*orgTelecom)[0]["value"])), "089-123456");
    EXPECT_EQ(model::NumberAsStringParserDocument::getStringValueFromValue(&((*orgTelecom)[1]["value"])), "info@apotheke.de");
}

TEST_F(HealthcareServiceDirectoryTest, locationAddressStateAndOrganizationSetAddress)
{
    const std::string locJson = R"({
        "resourceType": "Location",
        "id": "loc-123",
        "address": {
            "use": "work",
            "type": "postal",
            "text": "Musterstr. 10, 80331 München, Bayern, DE",
            "line": ["Musterstr. 10"],
            "city": "München",
            "state": "Bayern",
            "postalCode": "80331",
            "country": "DE"
        }
    })";

    const auto location = model::LocationDirectory::fromJsonNoValidation(locJson);
    EXPECT_EQ(location.getAddress(), "Musterstr. 10, 80331 München, Bayern, DE");
    const auto* address = location.addressRaw();
    ASSERT_NE(address, nullptr);
    ASSERT_TRUE(address->IsObject());
    const auto* state = rapidjson::Pointer("/state").Get(*address);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(model::NumberAsStringParserDocument::getStringValueFromValue(state), "Bayern");

    model::ErpTPrescriptionOrganization org;
    org.setAddress(*address);
    const auto* orgAddress = rapidjson::Pointer("/address").Get(org.jsonDocument());
    ASSERT_NE(orgAddress, nullptr);
    ASSERT_TRUE(orgAddress->IsArray());
    ASSERT_EQ(orgAddress->Size(), 1);
    const auto* orgState = rapidjson::Pointer("/0/state").Get(*orgAddress);
    ASSERT_NE(orgState, nullptr);
    EXPECT_EQ(model::NumberAsStringParserDocument::getStringValueFromValue(orgState), "Bayern");
    EXPECT_EQ(model::NumberAsStringParserDocument::getStringValueFromValue(rapidjson::Pointer("/0/city").Get(*orgAddress)), "München");
}
