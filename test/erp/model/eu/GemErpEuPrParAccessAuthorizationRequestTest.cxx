// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/model/eu/GemErpEuPrParAccessAuthorizationRequest.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>

TEST(GemErpEuPrParAccessAuthorizationRequestTest, test)
{
    testutils::ShiftFhirResourceViewsGuard shift{"EU_2025_10_01",
                                                 date::floor<date::days>(model::Timestamp::now().toChronoTimePoint())};
    ResourceTemplates::EuAccessPermissionOptions optionas;
    auto json = ResourceTemplates::euAccessPermissionRequestJson(optionas);
    std::optional<model::GemErpEuPrParAccessAuthorizationRequest> resource;
    ASSERT_NO_THROW(resource = model::GemErpEuPrParAccessAuthorizationRequest::fromJson(json, *StaticData::getJsonValidator()));
    ASSERT_TRUE(resource);
    EXPECT_STREQ(optionas.accessCode.c_str(),resource->getAccessCode().toString().c_str());
    EXPECT_EQ(optionas.twoLetterCountryCode, resource->getCountryCode().toString());
}
