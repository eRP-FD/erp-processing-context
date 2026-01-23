/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "erp/service/consent/ConsentPostHandler.hxx"
#include "fhirtools/converter/FhirConverter.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTestFixture.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <boost/algorithm/string.hpp>
#include <date/tz.h>

class ConsentPostHandlerTest : public EndpointHandlerTest
{
};

TEST_F(ConsentPostHandlerTest, Success)
{
    model::Timestamp yesterday{
        std::chrono::floor<std::chrono::seconds>((model::Timestamp::now() - 24h).toChronoTimePoint())};
    auto consentJSON = ResourceTemplates::consentJson({
        .consentCategory = model::ConsentType::CHARGCONS,
        .kvnr = "X764228538",
        .timestamp = yesterday.toXsDateTime(),
    });
    Header requestHeader{HttpMethod::POST,
                         "/Consent",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                         HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setBody(consentJSON);
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X764228538"));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
    ConsentPostHandler consentPostHandler({});
    EXPECT_NO_THROW(consentPostHandler.preHandleRequestHook(sessionContext));
    EXPECT_NO_THROW(consentPostHandler.handleRequest(sessionContext));
    std::optional<model::Consent> consent;
    EXPECT_NO_THROW(consent = model::Consent::fromJson(serverResponse.getBody(), *StaticData::getJsonValidator()));
    ASSERT_TRUE(consent.has_value());
    A_27143.test("timestamp is set");
    EXPECT_GE(consent->dateTime(), yesterday + 24h);
    EXPECT_EQ(serverResponse.getBody().find(consent->dateTime().toXsDateTime()), std::string::npos);
    EXPECT_NE(serverResponse.getBody().find(consent->dateTime().toXsDateTimeWithoutFractionalSeconds()),
              std::string::npos);
}

TEST_F(ConsentPostHandlerTest, WrongProfileErp12831)
{
    auto consentJSON = ResourceTemplates::consentJson({
        .consentCategory = model::ConsentType::CHARGCONS,
        .kvnr = "X764228538",
    });
    boost::algorithm::replace_all(consentJSON, model::resource::structure_definition::consent,
                                  "https://gematik.de/fhir/StructureDefinition/ErxConsent");

    Header requestHeader{HttpMethod::POST,
                         "/Consent",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                         HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setBody(consentJSON);
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X764228538"));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ConsentPostHandler consentPostHandler({});
    EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(
        consentPostHandler.handleRequest(sessionContext), HttpStatus::BadRequest, "parsing / validation error",
        "Unsupported profile type: https://gematik.de/fhir/StructureDefinition/ErxConsent|" +
            ResourceTemplates::Versions::GEM_ERPCHRG_current().renderVersion());
}


TEST_F(ConsentPostHandlerTest, WrongProfileType)
{
    auto consentJSON = ResourceTemplates::consentJson({
        .consentCategory = model::ConsentType::CHARGCONS,
        .kvnr = "X764228538",
    });
    boost::algorithm::replace_all(consentJSON, model::resource::structure_definition::consent,
                                  model::resource::structure_definition::chargeItem);

    Header requestHeader{HttpMethod::POST,
                         "/Consent",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                         HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setBody(consentJSON);
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X764228538"));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ConsentPostHandler consentPostHandler({});
    EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(consentPostHandler.handleRequest(sessionContext), HttpStatus::BadRequest,
                                          "parsing / validation error",
                                          "Profile Not allowed on this endpoint: GEM_ERPCHRG_PR_ChargeItem");
}

TEST_F(ConsentPostHandlerTest, WrongType)
{
    auto chargeItemXML = ResourceTemplates::chargeItemXml({
        .kvnr = model::Kvnr{"X764228538", model::Kvnr::Type::pkv},
        .prescriptionId =
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 1000),
        .dispenseBundleBase64 = "SGFsbG8gV2VsdCE=",
        .operation = ResourceTemplates::ChargeItemOptions::OperationType::Post,
    });
    auto chargeItemJSON = Fhir::instance().converter().xmlStringToJson(chargeItemXML);
    Header requestHeader{HttpMethod::POST,
                         "/Consent",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                         HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setBody(chargeItemJSON.serializeToJsonString());
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X764228538"));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};

    ConsentPostHandler consentPostHandler({});
    EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(consentPostHandler.handleRequest(sessionContext), HttpStatus::BadRequest,
                                          "parsing / validation error",
                                          "Profile Not allowed on this endpoint: GEM_ERPCHRG_PR_ChargeItem");
}


struct ConsentPostValidityTestParam {
    using VersionType = ResourceTemplates::ConsentOptions::VersionType;
    std::string date;
    VersionType version;
    model::ConsentType consentType;
    bool expectSuccess = true;
};

class ConsentPostValidityTest : public EndpointHandlerTest,
                                public testing::WithParamInterface<ConsentPostValidityTestParam>
{
};


TEST_P(ConsentPostValidityTest, run)
{
    date::local_days today{std::chrono::floor<std::chrono::days>(
        date::make_zoned(model::Timestamp::GermanTimezone, std::chrono::system_clock::now()).get_local_time())};
    date::local_days date;
    std::istringstream datestream{GetParam().date};
    date::from_stream(datestream, "%Y-%m-%d", date);
    std::vector<EnvironmentVariableGuard> guards;
    guards.emplace_back(ConfigurationKey::FHIR_REFERENCE_TIME_OFFSET_DAYS, std::to_string((today - date).count()));
    guards.emplace_back(ConfigurationKey::FEATURE_EU, "true");
    auto consentJSON = ResourceTemplates::consentJson({
        .consentCategory = GetParam().consentType,
        .kvnr = "X764228538",
        .timestamp = date::format("%Y-%m-%d", today),
        .version = GetParam().version,
    });
    Header requestHeader{HttpMethod::POST,
                         "/Consent",
                         0,
                         {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                         HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setBody(consentJSON);
    serverRequest.setAccessToken(mJwtBuilder->makeJwtVersicherter("X764228538"));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, serverRequest, serverResponse, accessLog};
    ConsentPostHandler consentPostHandler({});
    EXPECT_NO_THROW(consentPostHandler.preHandleRequestHook(sessionContext));
    if (GetParam().expectSuccess)
    {
        EXPECT_NO_THROW(consentPostHandler.handleRequest(sessionContext));
        if (GetParam().consentType != model::ConsentType::EUDISPCONS)
        {
            std::optional<model::Consent> consent;
            EXPECT_NO_THROW(consent = model::Consent::fromJson(serverResponse.getBody(), *StaticData::getJsonValidator()));
            ASSERT_TRUE(consent.has_value());
        }
    }
    else
    {
        EXPECT_ERP_EXCEPTION_WITH_MESSAGE(consentPostHandler.handleRequest(sessionContext), HttpStatus::BadRequest,
                                          "parsing / validation error");
    }
}

INSTANTIATE_TEST_SUITE_P(success, ConsentPostValidityTest,
                         testing::ValuesIn(std::list<ConsentPostValidityTestParam>{
                             {
                                 .date = "2025-10-01",
                                 .version = ResourceTemplates::Versions::GEM_ERPCHRG_1_0,
                                 .consentType = model::ConsentType::CHARGCONS,
                             },
                             {
                                 .date = "2025-10-01",
                                 .version = ResourceTemplates::Versions::GEM_ERPCHRG_1_1,
                                 .consentType = model::ConsentType::CHARGCONS,
                             },
                             {
                                 .date = "2025-10-01",
                                 .version = ResourceTemplates::Versions::GEM_ERPEU_1_1,
                                 .consentType = model::ConsentType::EUDISPCONS,
                             },
                             {
                                 .date = "2026-03-31",
                                 .version = ResourceTemplates::Versions::GEM_ERPCHRG_1_0,
                                 .consentType = model::ConsentType::CHARGCONS,
                             },
                             {
                                 .date = "2026-03-31",
                                 .version = ResourceTemplates::Versions::GEM_ERPCHRG_1_1,
                                 .consentType = model::ConsentType::CHARGCONS,
                             },
                             {
                                 .date = "2026-03-31",
                                 .version = ResourceTemplates::Versions::GEM_ERPEU_1_1,
                                 .consentType = model::ConsentType::EUDISPCONS,
                             },
                             {
                                 .date = "2026-04-01",
                                 .version = ResourceTemplates::Versions::GEM_ERPCHRG_1_1,
                                 .consentType = model::ConsentType::CHARGCONS,
                             },
                             {
                                 .date = "2026-04-01",
                                 .version = ResourceTemplates::Versions::GEM_ERPEU_1_1,
                                 .consentType = model::ConsentType::EUDISPCONS,
                             },
                         }));
INSTANTIATE_TEST_SUITE_P(fail, ConsentPostValidityTest,
                         testing::ValuesIn(std::list<ConsentPostValidityTestParam>{
                             {
                                 .date = "2025-09-30",
                                 .version = ResourceTemplates::Versions::GEM_ERPEU_1_1,
                                 .consentType = model::ConsentType::EUDISPCONS,
                                 .expectSuccess = false,
                             },
                             {
                                 .date = "2026-04-01",
                                 .version = ResourceTemplates::Versions::GEM_ERPCHRG_1_0,
                                 .consentType = model::ConsentType::CHARGCONS,
                                 .expectSuccess = false,
                             },
                         }));
