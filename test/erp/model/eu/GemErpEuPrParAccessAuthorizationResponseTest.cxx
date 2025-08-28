// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/model/eu/GemErpEuPrParAccessAuthorizationResponse.hxx"
#include "erp/model/eu/EuAccessCode.hxx"
#include "erp/model/eu/EuAccessPermission.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>

TEST(GemErpEuPrParAccessAuthorizationResponseTest, test)
{
    testutils::ShiftFhirResourceViewsGuard shift{"EU_2025_10_01",
                                                 date::floor<date::days>(model::Timestamp::now().toChronoTimePoint())};
    std::optional<model::GemErpEuPrParAccessAuthorizationResponse> resource;
    model::EuAccessPermission eua{model::EuAccessCode{SafeString{"abcdef"}}, model::CountryCode{"FR"}};
    ASSERT_NO_THROW((resource = model::GemErpEuPrParAccessAuthorizationResponse{eua, model::Kvnr{"X1234567"}}));
    ASSERT_TRUE(resource);

    auto* accessCode = resource->findParameter("accessCode");
    ASSERT_TRUE(accessCode);

    EXPECT_STREQ(model::NumberAsStringParserDocument::getStringValueFromValue(
                     rapidjson::Pointer{"/valueIdentifier/value"}.Get(*accessCode))
                     .data(),
                 "abcdef");
    auto* countryCode = resource->findParameter("countryCode");
    ASSERT_TRUE(countryCode);
    EXPECT_STREQ(model::NumberAsStringParserDocument::getStringValueFromValue(
                     rapidjson::Pointer{"/valueCoding/code"}.Get(*countryCode))
                     .data(),
                 "FR");
    auto* validUntil = resource->findParameter("validUntil");
    ASSERT_TRUE(validUntil);
    EXPECT_STREQ(model::NumberAsStringParserDocument::getStringValueFromValue(
                     rapidjson::Pointer{"/valueInstant"}.Get(*validUntil))
                     .data(),
                 eua.getValidUntil().toXsDateTime().c_str());
    auto* createdAt = resource->findParameter("createdAt");
    ASSERT_TRUE(createdAt);
    EXPECT_STREQ(model::NumberAsStringParserDocument::getStringValueFromValue(
                     rapidjson::Pointer{"/valueInstant"}.Get(*createdAt))
                     .data(),
                 eua.getCreatedAt().toXsDateTime().c_str());

    std::optional<model::GemErpEuPrParAccessAuthorizationResponse> resource2;
    ASSERT_NO_THROW(resource2 = model::GemErpEuPrParAccessAuthorizationResponse::fromJson(
                        resource->serializeToJsonString(), *StaticData::getJsonValidator()));
    ASSERT_TRUE(resource2);
}
