// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/model/eu/EuAccessPermission.hxx"
#include "erp/model/eu/GemErpEuPrParAccessAuthorizationResponse.hxx"
#include "erp/service/euAccessPermission/GrantHandler.hxx"
#include "erp/service/euAccessPermission/ReadHandler.hxx"
#include "erp/service/euAccessPermission/RevokeHandler.hxx"
#include "test/erp/service/EndpointHandlerTest/EuFixture.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

class EuReadHandlerTest : public EuFixture
{
public:
};

TEST_F(EuReadHandlerTest, notFound)
{
    const std::string kvnr{"X123456789"};
    eu_access_permission::ReadHandler readHandler({});
    ServerRequest request{Header{HttpMethod::GET,
                                 "/$read-eu-access-permission",
                                 0,
                                 {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                                 HttpStatus::Unknown}};
    request.setAccessToken(mJwtBuilder->makeJwtVersicherter(kvnr));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};
    ASSERT_NO_THROW(readHandler.preHandleRequestHook(sessionContext));
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(readHandler.handleRequest(sessionContext), HttpStatus::NotFound,
                                      "Zugriffsberechtigung nicht gefunden");
}

TEST_F(EuReadHandlerTest, outdated)
{
    const std::string kvnr{"X123456789"};

    auto db = mServiceContext.databaseFactory();
    db->createEuAccessPermission(model::Kvnr{kvnr},
                                 model::EuAccessPermission{model::EuAccessCode{SafeString{"aaaaaa"}},
                                                           model::CountryCode{"FR"}, model::Timestamp::now() - 1h});
    db->commitTransaction();
    db.reset();

    eu_access_permission::ReadHandler readHandler({});
    ServerRequest request{Header{HttpMethod::GET,
                                 "/$read-eu-access-permission",
                                 0,
                                 {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                                 HttpStatus::Unknown}};
    request.setAccessToken(mJwtBuilder->makeJwtVersicherter(kvnr));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};
    ASSERT_NO_THROW(readHandler.preHandleRequestHook(sessionContext));
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(readHandler.handleRequest(sessionContext), HttpStatus::NotFound,
                                      "Zugriffsberechtigung nicht mehr gültig.");
}

TEST_F(EuReadHandlerTest, success)
{
    const std::string kvnr{"X123456789"};
    grant(kvnr);
    eu_access_permission::ReadHandler readHandler({});
    ServerRequest request{Header{HttpMethod::GET,
                                 "/$read-eu-access-permission",
                                 0,
                                 {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                                 HttpStatus::Unknown}};
    request.setAccessToken(mJwtBuilder->makeJwtVersicherter(kvnr));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};
    ASSERT_NO_THROW(readHandler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(readHandler.handleRequest(sessionContext));
    EXPECT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
    EXPECT_EQ(serverResponse.getHeader().contentType(), std::string{ContentMimeType::fhirJsonUtf8});
    A_27087.test("check GEM_ERP_PR_PAR_EU_Access_Authorization_Response");
    checkEuAccessAuthorizationResponse(serverResponse, {.twoLetterCountryCode = "FR", .accessCode = "aaaaaa"});
}
