/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/crypto/VsdmProof.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/service/task/GetTaskHandler.hxx"
#include "shared/enrolment/VsdmHmacKey.hxx"
#include "shared/util/Base64.hxx"
#include "test/erp/pc/popp/PoPPTokenBuilder.hxx"
#include "test/erp/util/EgkProofUtils.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockDatabaseProxy.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/StaticData.hxx"
#undef Expect
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <test/erp/pc/popp/PoPPCertificateVerifierServiceMock.hxx>
#include <erp/service/AuditEventHandler.hxx>


class GMockDatabaseProxy : public MockDatabaseProxy
{
public:
    using MockDatabaseProxy::MockDatabaseProxy;
    MOCK_METHOD(std::vector<db_model::Task>, retrieveAllEgkRedeemableTasksWithAccessCode,
                (const db_model::HashedKvnr& kvnrHashed, const std::optional<UrlArguments>& search), (override));

    MOCK_METHOD(std::vector<db_model::Task>, retrieveAllTasksForPatient,
                (const db_model::HashedKvnr& kvnrHashed, const std::optional<UrlArguments>& search), (override));

    MOCK_METHOD(std::vector<db_model::AuditData>, retrieveAuditEventData,
                (const db_model::HashedKvnr& kvnr, const std::optional<Uuid>& id,
                 const std::optional<model::PrescriptionId>& prescriptionId, const std::optional<UrlArguments>& search),
                (override));

    std::vector<db_model::Task>
    baseRetrieveAllEgkRedeemableTasksWithAccessCode(const db_model::HashedKvnr& kvnrHashed,
                                                    const std::optional<UrlArguments>& search)
    {
        return MockDatabaseProxy::retrieveAllEgkRedeemableTasksWithAccessCode(kvnrHashed, search);
    }

    std::vector<db_model::Task> baseRetrieveAllTasksForPatient(const db_model::HashedKvnr& kvnrHashed,
                                                               const std::optional<UrlArguments>& search)
    {
        return MockDatabaseProxy::retrieveAllTasksForPatient(kvnrHashed, search);
    }

    std::vector<db_model::AuditData>
    baseRetrieveAuditEventData(const db_model::HashedKvnr& kvnr, const std::optional<Uuid>& id,
                               const std::optional<model::PrescriptionId>& prescriptionId,
                               const std::optional<UrlArguments>& search)
    {
        return MockDatabaseProxy::retrieveAuditEventData(kvnr, id, prescriptionId, search);
    }
};

class ReadOnlyDBTest : public testing::Test
{
protected:
    const std::shared_ptr<MockDatabase>& mockDatabase(HsmPool& hsmPool)
    {
        if (! mMockDatabase)
        {
            mMockDatabase = std::make_shared<MockDatabase>(hsmPool);
            mMockDatabase->fillWithStaticData();
        }
        return mMockDatabase;
    }

    PcServiceContext makePcServiceContext()
    {
        EnvironmentVariableGuard noPostgres{TestConfigurationKey::TEST_USE_POSTGRES, "false"};
        auto factories = StaticData::makeMockFactories();
        factories.databaseFactory = std::bind_front(&ReadOnlyDBTest::createDatabase<MockDatabaseProxy>, this);
        factories.readOnlyDatabaseFactory = std::bind_front(&ReadOnlyDBTest::createDatabase<GMockDatabaseProxy>, this);
        factories.poppServiceFactory = [](boost::asio::io_context*, TslManager&, std::shared_ptr<CrlProvider>) {
            auto poppServiceMock = std::make_unique<testing::NiceMock<PoPPCertificateVerifierServiceMock>>();
            setupDefaultMock(*poppServiceMock);
            return poppServiceMock;
        };
        return PcServiceContext{Configuration::instance(), std::move(factories)};
    }

    MOCK_METHOD(std::unique_ptr<GMockDatabaseProxy>, readOnlyProxy, (HsmPool & hsmPool));

    void expectRetrieveAllEgkRedeemableTasksWithAccessCode()
    {
        // expect a single call to create an instance of ReadOnlyDatabase
        EXPECT_CALL(*this, readOnlyProxy).Times(1).WillOnce([&](HsmPool& hsmPool) {
            auto gmockProxy = std::make_unique<GMockDatabaseProxy>(mockDatabase(hsmPool));
            // on the created instance expect retrieval of Tasks
            EXPECT_CALL(*gmockProxy, retrieveAllEgkRedeemableTasksWithAccessCode)
                .Times(1)
                .WillOnce([gmockProxy = gmockProxy.get()](const db_model::HashedKvnr& kvnrHashed,
                                                          const std::optional<UrlArguments>& search) {
                    return gmockProxy->baseRetrieveAllEgkRedeemableTasksWithAccessCode(kvnrHashed, search);
                });

            return gmockProxy;
        });
    }


    VsdmHmacKey keyPackage{'A', '1'};
    std::string kvnr{"X123456788"};
    std::string hcv = VsdmProof2::makeHcv("20250101", "Hauptstr");


private:
    std::shared_ptr<MockDatabase> mMockDatabase;

    template<typename ProxyT>
    std::unique_ptr<ProxyT> makeProxy(HsmPool& hsmPool);

    template<typename ProxyT>
    std::unique_ptr<Database> createDatabase(HsmPool& hsmPool, KeyDerivation& keyDerivation)
    {
        return std::make_unique<DatabaseFrontend>(makeProxy<ProxyT>(hsmPool), hsmPool, keyDerivation);
    }
};

template<>
std::unique_ptr<MockDatabaseProxy> ReadOnlyDBTest::makeProxy(HsmPool& hsmPool)
{
    return std::make_unique<MockDatabaseProxy>(mockDatabase(hsmPool));
}
template<>
std::unique_ptr<GMockDatabaseProxy> ReadOnlyDBTest::makeProxy(HsmPool& hsmPool)
{
    return readOnlyProxy(hsmPool);
}

TEST_F(ReadOnlyDBTest, GetTasksPharmacy_UC_4_12)
{
    expectRetrieveAllEgkRedeemableTasksWithAccessCode();
    auto serviceContext = makePcServiceContext();
    enrolKey(serviceContext, keyPackage);
    auto encryptedProof = VsdmProof2::encrypt(keyPackage, VsdmProof2::DecryptedProof{.revoked = false,
                                                                                     .hcv = hcv,
                                                                                     .iat = model::Timestamp::now(),
                                                                                     .kvnr = model::Kvnr{kvnr}});

    auto pz = encryptedProof.serialize();
    auto pnw = createEncodedPnw(pz);

    ServerResponse serverResponse;

    GetAllTasksHandler handler({});

    const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
    Header requestHeader{HttpMethod::GET, "/Task", 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setAccessToken(jwtPharmacy);
    ServerRequest::QueryParametersType params{{"pnw", pnw},
                                              {"kvnr", kvnr},
                                              {"hcv", Base64::toBase64Url(Base64::encode(hcv))}};
    serverRequest.setQueryParameters(params);
    AccessLog accessLog;
    SessionContext sessionContext{serviceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
}


TEST_F(ReadOnlyDBTest, GetTasksPoPP_UC_4_15)
{
    expectRetrieveAllEgkRedeemableTasksWithAccessCode();
    auto serviceContext = makePcServiceContext();
    auto token = PoPPTokenBuilder{}
                     .withProofMethod("healthid")
                     .withPatientId(kvnr)
                     .withActorId("3-SMC-B-Testkarte-883110000120312")
                     .getToken();
    ServerResponse response;
    const auto jwtPharmacy = JwtBuilder::testBuilder().makeJwtApotheke();
    Header requestHeader{HttpMethod::GET, "/Task", 0, {}, HttpStatus::Unknown};
    requestHeader.addHeaderField(Header::XPoPPToken, token.serialize());
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setAccessToken(jwtPharmacy);
    AccessLog accessLog;
    ServerResponse serverResponse;
    SessionContext sessionContext(serviceContext, serverRequest, serverResponse, accessLog);

    GetAllTasksHandler handler({});
    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
}

TEST_F(ReadOnlyDBTest, GetTasksPatient_UC_3_1)
{
    // expect a single call to create an instance of ReadOnlyDatabase
    EXPECT_CALL(*this, readOnlyProxy).Times(1).WillOnce([&](HsmPool& hsmPool) {
        auto gmockProxy = std::make_unique<GMockDatabaseProxy>(mockDatabase(hsmPool));
        // on the created instance expect retrieval of Tasks
        EXPECT_CALL(*gmockProxy, retrieveAllTasksForPatient)
            .Times(1)
            .WillOnce([gmockProxy = gmockProxy.get()](const db_model::HashedKvnr& kvnrHashed,
                                                      const std::optional<UrlArguments>& search) {
                return gmockProxy->baseRetrieveAllTasksForPatient(kvnrHashed, search);
            });

        return gmockProxy;
    });
    auto serviceContext = makePcServiceContext();
    GetAllTasksHandler handler({});

    auto jwt = JwtBuilder::testBuilder().makeJwtVersicherter("X123456788");

    Header requestHeader{HttpMethod::GET, "/Task/", 0, {}, HttpStatus::Unknown};
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setAccessToken(std::move(jwt));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{serviceContext, serverRequest, serverResponse, accessLog};

    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
}

TEST_F(ReadOnlyDBTest, GetAuditEvent_UC_3_5)
{
    auto serviceContext = makePcServiceContext();
    // expect a single call to create an instance of ReadOnlyDatabase
    EXPECT_CALL(*this, readOnlyProxy).Times(1).WillOnce([&](HsmPool& hsmPool) {
        auto gmockProxy = std::make_unique<GMockDatabaseProxy>(mockDatabase(hsmPool));
        // on the created instance expect retrieval of Audit Events
        EXPECT_CALL(*gmockProxy, retrieveAuditEventData)
            .Times(1)
            .WillOnce([gmockProxy = gmockProxy.get()](const db_model::HashedKvnr& kvnr, const std::optional<Uuid>& id,
                                                      const std::optional<model::PrescriptionId>& prescriptionId,
                                                      const std::optional<UrlArguments>& search) {
                return gmockProxy->baseRetrieveAuditEventData(kvnr, id, prescriptionId, search);
            });
        return gmockProxy;
    });

    GetAllAuditEventsHandler handler({});

    Header requestHeader{HttpMethod::GET, "/AuditEvent/", 0, {{Header::AcceptLanguage, "de"}}, HttpStatus::Unknown};
    auto jwt = JwtBuilder::testBuilder().makeJwtVersicherter("X122446685");
    ServerRequest serverRequest{std::move(requestHeader)};
    serverRequest.setAccessToken(std::move(jwt));
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{serviceContext, serverRequest, serverResponse, accessLog};
    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    ASSERT_NO_THROW(handler.handleRequest(sessionContext));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
}
