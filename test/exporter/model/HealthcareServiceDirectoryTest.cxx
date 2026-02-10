/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/model/HealthcareServiceDirectory.hxx"
#include "shared/model/ModelException.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>

#include "exporter/model/LocationDirectory.hxx"
#include "exporter/model/OrganizationDirectory.hxx"

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

    const auto& organization = organizationEntries.at(0);
    EXPECT_EQ(organization.getName(), "Apotheke_Test_Inland_SMCB");

    const auto& location = locationEntries.at(0);
    EXPECT_EQ(location.getAddress(), "Hauptstr. 67, 70477, Stuttgart");
}
