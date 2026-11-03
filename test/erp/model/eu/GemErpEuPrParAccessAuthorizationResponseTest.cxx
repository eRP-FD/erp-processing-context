// (C) Copyright IBM Deutschland GmbH 2021, 2026
// (C) Copyright IBM Corp. 2021, 2026
// non-exclusively licensed to gematik GmbH

#include "erp/model/eu/GemErpEuPrParAccessAuthorizationResponse.hxx"
#include "erp/model/eu/EuAccessCode.hxx"
#include "erp/model/eu/EuAccessPermission.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>

TEST(GemErpEuPrParAccessAuthorizationResponseTest, test)
{
    std::optional<model::GemErpEuPrParAccessAuthorizationResponse> resource;
    model::EuAccessPermission eua{model::EuAccessCode{SafeString{"abcdef"}}, model::CountryCode{"FR"}};
    ASSERT_NO_THROW((resource = model::GemErpEuPrParAccessAuthorizationResponse{eua, model::Kvnr{"X1234567"}}));
    ASSERT_TRUE(resource);

    auto* accessCode = resource->findParameter("accessCode");
    ASSERT_TRUE(accessCode);

    EXPECT_EQ(model::NumberAsStringParserDocument::getStringValueFromValue(
                  rapidjson::Pointer{"/valueIdentifier/value"}.Get(*accessCode)),
              "abcdef");
    auto* countryCode = resource->findParameter("countryCode");
    ASSERT_TRUE(countryCode);
    EXPECT_EQ(model::NumberAsStringParserDocument::getStringValueFromValue(
                  rapidjson::Pointer{"/valueCoding/code"}.Get(*countryCode)),
              "FR");
    auto* validUntil = resource->findParameter("validUntil");
    ASSERT_TRUE(validUntil);
    EXPECT_EQ(model::NumberAsStringParserDocument::getStringValueFromValue(
                  rapidjson::Pointer{"/valueInstant"}.Get(*validUntil)),
              eua.getValidUntil().toXsDateTime());
    auto* createdAt = resource->findParameter("createdAt");
    ASSERT_TRUE(createdAt);
    EXPECT_EQ(model::NumberAsStringParserDocument::getStringValueFromValue(
                  rapidjson::Pointer{"/valueInstant"}.Get(*createdAt)),
              eua.getCreatedAt().toXsDateTime());

    std::optional<model::GemErpEuPrParAccessAuthorizationResponse> resource2;
    ASSERT_NO_THROW(resource2 = model::GemErpEuPrParAccessAuthorizationResponse::fromJson(
                        resource->serializeToJsonString(), *StaticData::getJsonValidator()));
    ASSERT_TRUE(resource2);
}
