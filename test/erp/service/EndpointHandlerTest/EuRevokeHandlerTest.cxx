// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/model/eu/EuAccessPermission.hxx"
#include "erp/model/eu/GemErpEuPrParAccessAuthorizationResponse.hxx"
#include "erp/service/euAccessPermission/GrantHandler.hxx"
#include "erp/service/euAccessPermission/RevokeHandler.hxx"
#include "test/erp/service/EndpointHandlerTest/EuFixture.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

class EuRevokeHandlerTest : public EuFixture
{
public:

    void expectGrant(const std::string& kvnr)
    {
        auto db = mServiceContext.databaseFactory();
        auto grant = db->retrieveEuAccessPermission(model::Kvnr{kvnr});
        db->commitTransaction();
        ASSERT_TRUE(grant.has_value());
    }
    void expectNoGrant(const std::string& kvnr)
    {
        auto db = mServiceContext.databaseFactory();
        auto grant = db->retrieveEuAccessPermission(model::Kvnr{kvnr});
        db->commitTransaction();
        ASSERT_FALSE(grant.has_value());
    }

};

TEST_F(EuRevokeHandlerTest, notFound)
{
    const std::string kvnr{"X123456789"};
    eu_access_permission::RevokeHandler revokeHandler({});
    ServerRequest request{Header{HttpMethod::DELETE,
                                 "/$revoke-eu-access-permission",
                                 0,
                                 {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                                 HttpStatus::Unknown}};
    request.setAccessToken(mJwtBuilder->makeJwtVersicherter(kvnr));
    ServerResponse serverResponse;
    AccessLog accessLog;
    {
        SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};
        ASSERT_NO_THROW(revokeHandler.preHandleRequestHook(sessionContext));
        expectNoGrant(kvnr);
        ASSERT_NO_THROW(revokeHandler.handleRequest(sessionContext));
        sessionContext.database()->commitTransaction();
    }
    EXPECT_EQ(serverResponse.getHeader().status(), HttpStatus::NoContent);
    expectNoGrant(kvnr);
}

TEST_F(EuRevokeHandlerTest, success)
{
    A_27085.test("test delete grant");
    const std::string kvnr{"X123456789"};
    grant(kvnr);
    eu_access_permission::RevokeHandler revokeHandler({});
    ServerRequest request{Header{HttpMethod::DELETE,
                                 "/$revoke-eu-access-permission",
                                 0,
                                 {{Header::ContentType, ContentMimeType::fhirJsonUtf8}},
                                 HttpStatus::Unknown}};
    request.setAccessToken(mJwtBuilder->makeJwtVersicherter("X123456789"));
    ServerResponse serverResponse;
    AccessLog accessLog;
    {
        SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};
        ASSERT_NO_THROW(revokeHandler.preHandleRequestHook(sessionContext));
        expectGrant(kvnr);
        ASSERT_NO_THROW(revokeHandler.handleRequest(sessionContext));
        sessionContext.database()->commitTransaction();
    }
    EXPECT_EQ(serverResponse.getHeader().status(), HttpStatus::NoContent);
    expectNoGrant(kvnr);
}