// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH


#include "erp/model/eu/GemErpEuPrParAccessAuthorizationResponse.hxx"
#include "erp/service/euAccessPermission/GrantHandler.hxx"
#include "test/erp/service/EndpointHandlerTest/EuFixture.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"

class EuGrantHandlerTest : public EuFixture
{
public:
};

TEST_F(EuGrantHandlerTest, noConsent)
{
    auto json = ResourceTemplates::euAccessPermissionRequestJson({});
    eu_access_permission::GrantHandler grantHandler({});
    ServerRequest request{Header{HttpMethod::POST,
                                 "/$grant-eu-access-permission",
                                 0,
                                 {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                                 HttpStatus::Unknown}};
    request.setBody(json);

    request.setAccessToken(mJwtBuilder->makeJwtVersicherter("X123456789"));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};
    ASSERT_NO_THROW(grantHandler.preHandleRequestHook(sessionContext));
    A_27089.test("test without consent");
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(
        grantHandler.handleRequest(sessionContext), HttpStatus::Forbidden,
        "Das Erstellen einer Zugriffsberechtigung ist erst zulässig, wenn eine Einwilligung durch den Nutzer zum "
        "Einlösen von E-Rezepten im europäischen Ausland erteilt wurde.");
}

TEST_F(EuGrantHandlerTest, wrongCountry)
{
    const std::string kvnr{"X123456789"};
    giveConsent(kvnr);
    auto json =
        ResourceTemplates::euAccessPermissionRequestJson({.twoLetterCountryCode = "FR", .accessCode = "aaaaaa"});
    eu_access_permission::GrantHandler grantHandler({});
    ServerRequest request{Header{HttpMethod::POST,
                                 "/$grant-eu-access-permission",
                                 0,
                                 {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                                 HttpStatus::Unknown}};
    request.setBody(json);

    request.setAccessToken(mJwtBuilder->makeJwtVersicherter(kvnr));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};
    ASSERT_NO_THROW(grantHandler.preHandleRequestHook(sessionContext));

    A_27090.test("test with wrong country");
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(grantHandler.handleRequest(sessionContext), HttpStatus::Conflict,
                                      "Für das angefragte Land ist Einlösen von E-Rezepten nicht möglich.");

    allowCountry("BE");// still wrong;
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(grantHandler.handleRequest(sessionContext), HttpStatus::Conflict,
                                      "Für das angefragte Land ist Einlösen von E-Rezepten nicht möglich.");
}

TEST_F(EuGrantHandlerTest, badAccessCode)
{
    const std::string kvnr{"X123456789"};
    giveConsent(kvnr);
    allowCountry("FR");
    auto json = ResourceTemplates::euAccessPermissionRequestJson({.twoLetterCountryCode = "FR", .accessCode = "BAD_ACCESS_CODE"});
    eu_access_permission::GrantHandler grantHandler({});
    ServerRequest request{Header{HttpMethod::POST,
                                 "/$grant-eu-access-permission",
                                 0,
                                 {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                                 HttpStatus::Unknown}};
    request.setBody(json);
    request.setAccessToken(mJwtBuilder->makeJwtVersicherter(kvnr));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};
    ASSERT_NO_THROW(grantHandler.preHandleRequestHook(sessionContext));
    A_27091.test("test with bad access code");
    EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(grantHandler.handleRequest(sessionContext), HttpStatus::BadRequest,
    "FHIR-Validation error", "Parameters.parameter[1].valueIdentifier.value: error: workflow-eu-access-code-1: Format of the AccessCode must be a 6-digit code in the Form [a-z, A-Z, 0-9]. (from profile: https://gematik.de/fhir/erp-eu/StructureDefinition/GEM_ERPEU_PR_Access_Code|1.0.0); ");
}

TEST_F(EuGrantHandlerTest, success)
{
    const std::string kvnr{"X123456789"};
    giveConsent(kvnr);
    allowCountry("FR");
    ResourceTemplates::EuAccessPermissionOptions options{.twoLetterCountryCode = "FR", .accessCode = "aaaaaa"};
    auto json = ResourceTemplates::euAccessPermissionRequestJson(options);
    eu_access_permission::GrantHandler grantHandler({});
    ServerRequest request{Header{HttpMethod::POST,
                                 "/$grant-eu-access-permission",
                                 0,
                                 {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                                 HttpStatus::Unknown}};
    request.setBody(json);

    request.setAccessToken(mJwtBuilder->makeJwtVersicherter(kvnr));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};
    ASSERT_NO_THROW(grantHandler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(grantHandler.handleRequest(sessionContext));

    A_27094.test("check GEM_ERP_PR_PAR_EU_Access_Authorization_Response");
    checkEuAccessAuthorizationResponse(serverResponse, options);
}
