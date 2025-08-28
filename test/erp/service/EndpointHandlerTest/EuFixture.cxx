// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "test/erp/service/EndpointHandlerTest/EuFixture.hxx"
#include "erp/model/eu/EuAccessPermission.hxx"
#include "erp/model/eu/GemErpEuPrParAccessAuthorizationResponse.hxx"
#include "shared/ErpRequirements.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/StaticData.hxx"

void EuFixture::checkEuAccessAuthorizationResponse(const ServerResponse& serverResponse,
                                                   const ResourceTemplates::EuAccessPermissionOptions& options)
{
    auto now = model::Timestamp::now();
    std::optional<model::GemErpEuPrParAccessAuthorizationResponse> response;
    ASSERT_NO_THROW(response = model::GemErpEuPrParAccessAuthorizationResponse::fromJson(
                        serverResponse.getBody(), *StaticData::getJsonValidator()));
    ASSERT_TRUE(response.has_value());

    auto countryCodeParam = response->findParameter("countryCode");
    ASSERT_NE(countryCodeParam, nullptr);
    auto countryCodeValue = model::NumberAsStringParserDocument::getOptionalStringValue(
        *countryCodeParam, model::ParameterBase::valueCodingCodePointer);
    ASSERT_TRUE(countryCodeValue.has_value());
    EXPECT_EQ(*countryCodeValue, options.twoLetterCountryCode);

    auto accessCodeParam = response->findParameter("accessCode");
    ASSERT_NE(accessCodeParam, nullptr);
    auto accessCodeValue = model::NumberAsStringParserDocument::getOptionalStringValue(
        *accessCodeParam, model::ParameterBase::valueIdentifierValuePointer);
    ASSERT_TRUE(accessCodeValue.has_value());
    EXPECT_EQ(*accessCodeValue, options.accessCode);

    auto createdAtParam = response->findParameter("createdAt");
    ASSERT_NE(createdAtParam, nullptr);
    auto createdAtValue = model::NumberAsStringParserDocument::getOptionalStringValue(
        *createdAtParam, model::ParameterBase::valueInstantPointer);
    ASSERT_TRUE(createdAtValue.has_value());
    model::Timestamp createdAt{model::Timestamp::fromXsDateTime(std::string{*createdAtValue})};
    EXPECT_LT(createdAt - now, std::chrono::minutes{1});

    A_27093.test("validUntil is createdAt + 1h");
    auto validUntilParam = response->findParameter("validUntil");
    ASSERT_NE(validUntilParam, nullptr);
    auto validUntilValue = model::NumberAsStringParserDocument::getOptionalStringValue(
        *validUntilParam, model::ParameterBase::valueInstantPointer);
    ASSERT_TRUE(validUntilValue.has_value());
    model::Timestamp validUntil{model::Timestamp::fromXsDateTime(std::string{*validUntilValue})};
    EXPECT_EQ(validUntil, createdAt + std::chrono::hours{1});
}

void EuFixture::giveConsent(const std::string& kvnr)
{
    auto db = mServiceContext.databaseFactory();
    db->storeConsent(model::Consent{model::ConsentType::EUDISPCONS, model::Kvnr{kvnr}, model::Timestamp::now()});
    db->commitTransaction();
}

void EuFixture::allowCountry(const std::string& country)
{
    mockDatabase->addCountry(country);
}

void EuFixture::grant(const std::string& kvnr, std::string accessCode)
{
    auto db = mServiceContext.databaseFactory();
    db->createEuAccessPermission(model::Kvnr{kvnr}, model::EuAccessPermission{model::EuAccessCode{SafeString{std::move(accessCode)}},
                                                                              model::CountryCode{"FR"}});
    db->commitTransaction();
}
