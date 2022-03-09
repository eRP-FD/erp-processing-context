/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/enrolment/EnrolmentServer.hxx"
#include "erp/client/HttpsClient.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/enrolment/EnrolmentModel.hxx"
#include "erp/enrolment/EnrolmentRequestHandler.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/hsm/production/ProductionBlobDatabase.hxx"
#include "erp/hsm/TeeTokenUpdater.hxx"
#include "erp/tpm/Tpm.hxx"
#if !defined __APPLE__ && !defined _WIN32
#include "erp/tpm/TpmProduction.hxx"
#endif
#include "erp/util/Base64.hxx"
#include "erp/util/Hash.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "mock/tpm/TpmMock.hxx"
#include "mock/tpm/TpmTestData.hxx"
#include "mock/tpm/TpmTestHelper.hxx"
#include "mock/util/MockConfiguration.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockDatabaseProxy.hxx"
#include "test/util/BlobDatabaseHelper.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/MockAndProductionTestBase.hxx"
#include "test/util/TestConfiguration.hxx"
#include "test/mock/MockBlobDatabase.hxx"

#include <gtest/gtest.h>

#ifdef _WINNT_
#ifdef DELETE
#undef DELETE
#endif
#endif



class EnrolmentServerTest : public MockAndProductionTestBase<Tpm::Factory>
{
public:
    static constexpr uint16_t EnrolmentServerPort = 9191;
    static constexpr std::string_view mBlobId = "(012345678901234567890123456789)"; // 32 bytes, simulates a SHA256.
    static constexpr std::string_view mAkName = "< 012345678901234567890123456789 >"; // 34 bytes, simulates a TPM name object.

    std::shared_ptr<BlobCache> blobCache;

    EnrolmentServerTest(void)
    {
    }

    virtual void SetUp (void) override
    {
        MockAndProductionTestBase::SetUp();
        // If the tpm factory (stored in `parameter`) is missing or does produce an empty reference
        // (because it is a factory that creates a "real" tpm client but that is not currently supported)
        // then skip the test.
        if (IsSkipped() || (*parameter)(*blobCache)==nullptr)
        {
            // TPM is not configured to run in this mode.
            GTEST_SKIP();
            return;
        }

        MockAndProductionTestBase<Tpm::Factory>::SetUp();

        // Clear the database.
        BlobDatabaseHelper::removeUnreferencedBlobs();

        blobCache = createBlobCache();


        if (mServer == nullptr)
        {
            // Create and start the server.
            RequestHandlerManager<EnrolmentServiceContext> handlers;
            EnrolmentServer::addEndpoints(handlers);

            Tpm::Factory tpmFactory = *parameter;
            mServer = std::make_unique<HttpsServer<EnrolmentServiceContext>>(
                "0.0.0.0",
                EnrolmentServerPort,
                std::move(handlers),
                std::make_unique<EnrolmentServiceContext>(
                    std::move(tpmFactory),
                    blobCache, TslTestHelper::createTslManager<TslManager>(), hsmPool()));
            mServer->serve(1);
        }
    }


    virtual void TearDown (void) override
    {
        BlobDatabaseHelper::removeUnreferencedBlobs();

        if (mServer != nullptr)
        {
            mServer->shutDown();
            mServer.reset();
        }
    }


    HttpsClient createClient (void)
    {
        return HttpsClient("127.0.0.1", EnrolmentServerPort, 30 /*connectionTimeoutSeconds*/, false/*enforceServerAuthentication*/);
    }

    Header createHeader (
        const HttpMethod method,
        std::string&& target)
    {
        return Header(
            method,
            std::move(target),
            Header::Version_1_1,
            {},
            HttpStatus::Unknown);
    }

    void checkResponseHeader (const ClientResponse& response, const HttpStatus expectedStatus)
    {
        ASSERT_EQ(response.getHeader().status(), expectedStatus);
        if (!response.getBody().empty())
        {
            ASSERT_EQ(response.getHeader().header(Header::ContentType), ContentMimeType::jsonUtf8);
        }
    }

    std::string createPutBody (
        const std::string_view id,
        const std::string_view data,
        const size_t generation,
        const std::optional<size_t> expirySecondsSinceEpoch,
        const std::optional<size_t> startSecondsSinceEpoch,
        const std::optional<size_t> endSecondsSinceEpoch,
        const std::optional<ErpArray<TpmObjectNameLength>> akName,
        const std::optional<PcrSet> pcrSet)

    {
        std::ostringstream os;
        os << '{';
        os << R"("id":")" + Base64::encode(id)
           << R"(","blob":{"data":")" << Base64::encode(data)
           << R"(","generation":)" + std::to_string(generation)
           << R"(})";

        if (expirySecondsSinceEpoch.has_value() || startSecondsSinceEpoch.has_value() || endSecondsSinceEpoch.has_value())
        {
            os <<  R"(,"metadata":{)";
            if (expirySecondsSinceEpoch.has_value())
            {
                os << R"("expiryDateTime":)" << std::to_string(expirySecondsSinceEpoch.value());
            }
            else
            {
                os << R"("startDateTime":)" << std::to_string(startSecondsSinceEpoch.value())
                   << R"(,"endDateTime":)" << std::to_string(endSecondsSinceEpoch.value());
            }
            os << '}';
        }

        if (akName.has_value())
        {
            os << R"(,"akName":")" << Base64::encode(util::rawToBuffer(akName->data(), akName->size())) << '"';
        }
        if (pcrSet.has_value())
        {
            os << R"(,"pcrSet":)" << pcrSet->toString(true);
        }

        os << '}';
        TVLOG(0) << "request body is '" << os.str() << "'";
        return os.str();
    }

    std::string createDeleteBody (
        const std::string_view id)
    {
        return "{\"id\":\""
               + Base64::encode(id)
               + "\"}";
    }

    size_t toSecondsSinceEpoch (const std::chrono::system_clock::time_point t)
    {
        return std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch()).count();
    }

    const bool usePostgres = TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false);

    std::unique_ptr<Database> database();

    class UseKeyBlobGuard
    {
    public:

        class DummyDerivation {
        public:
            DummyDerivation() : DummyDerivation{{"TEST_USE_POSTGRES", "false"}} {};
            std::shared_ptr<BlobCache> cache = MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm);
            HsmPool mPool{std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(), cache),
                          TeeTokenUpdater::createMockTeeTokenUpdaterFactory()};
            KeyDerivation derivation{mPool};
        private:
            DummyDerivation(const EnvironmentVariableGuard&) {}
        };

        static db_model::HashedKvnr dummyHashedKvnr()
        {
            return DummyDerivation{}.derivation.hashKvnr("X081547110");
        }

        static db_model::HashedTelematikId dummyHashedTelematikId()
        {
            return DummyDerivation{}.derivation.hashTelematikId("X-XY-MyDoc");
        }

        static void deleteTask(EnrolmentServerTest& parent, model::PrescriptionId prescriptionId)
        {
            if (parent.usePostgres)
            {
                pqxx::connection conn{PostgresBackend::defaultConnectString()};
                pqxx::work txn{conn};
                txn.exec_params0("DELETE FROM erp.task WHERE prescription_id = $1", prescriptionId.toDatabaseId());
                txn.commit();
            }
            else
            {
                auto db = parent.database();
                dynamic_cast<MockDatabaseProxy&>(db->getBackend()).deleteTask(prescriptionId);
                db->commitTransaction();
            }
        }

        static void deleteAuditEvent(EnrolmentServerTest& parent, const Uuid& eventId)
        {
            if (parent.usePostgres)
            {
                pqxx::connection conn{PostgresBackend::defaultConnectString()};
                pqxx::work txn{conn};
                txn.exec_params0("DELETE FROM erp.auditevent WHERE id = $1", std::string{eventId});
                txn.commit();
            }
            else
            {
                auto db = parent.database();
                dynamic_cast<MockDatabaseProxy&>(db->getBackend()).deleteAuditEvent(eventId);
                db->commitTransaction();
            }
        }

        void useTaskDerivationKey(EnrolmentServerTest& parent, BlobId blobId)
        {
            auto db = parent.database();
            auto& backend = db->getBackend();
            auto now = model::Timestamp::now();
            auto task = backend.createTask(model::PrescriptionType::apothekenpflichigeArzneimittel,
                                           model::Task::Status::draft, now, now);
            backend.updateTask(std::get<0>(task), {}, blobId, {});
            backend.commitTransaction();
            releaseKeyBlob = [&parent, id = std::get<model::PrescriptionId>(task)]
                             { deleteTask(parent, id); };
        }

        void useCommunicationDerivationKey(EnrolmentServerTest& parent, BlobId blobId)
        {
            auto db = parent.database();
            auto& backend = db->getBackend();
            auto now = model::Timestamp::now();
            auto task = backend.createTask(model::PrescriptionType::apothekenpflichigeArzneimittel,
                                           model::Task::Status::draft, now, now);
            auto comm = backend.insertCommunication(std::get<model::PrescriptionId>(task),
                                                    now, model::Communication::MessageType::DispReq,
                                                    dummyHashedKvnr(),
                                                    dummyHashedTelematikId(),
                                                    std::nullopt, blobId, {}, blobId, {});
            backend.commitTransaction();
            Expect3(comm.has_value(), "Failed to create Communication message.", std::logic_error);
            releaseKeyBlob = [&parent, id = std::get<model::PrescriptionId>(task), commId = *comm]
                {
                    auto db = parent.database();
                    auto& backend = db->getBackend();
                    backend.deleteCommunication(commId , dummyHashedKvnr());
                    backend.commitTransaction();
                    deleteTask(parent, id);
                };
        }

        void useAuditLogDerivationKey(EnrolmentServerTest& parent, BlobId blobId)
        {
            auto db = parent.database();
            auto& backend = db->getBackend();
            auto now = model::Timestamp::now();
            auto task = backend.createTask(model::PrescriptionType::apothekenpflichigeArzneimittel,
                                           model::Task::Status::draft, now, now);
            db_model::AuditData auditEvent(model::AuditEvent::AgentType::machine,model::AuditEventId::GET_Task, {{}},model::AuditEvent::Action::read, dummyHashedKvnr(),
                                           4711, std::get<model::PrescriptionId>(task), blobId);
            Uuid auditId{backend.storeAuditEventData(auditEvent)};
            backend.commitTransaction();
            releaseKeyBlob = [&parent, id = std::get<model::PrescriptionId>(task), auditId]
                {
                    deleteAuditEvent(parent, auditId);
                    deleteTask(parent, id);
                };
        }


        UseKeyBlobGuard(EnrolmentServerTest& parent, BlobType type, BlobId blobId)
        {
            switch (type)
            {
                case BlobType::TaskKeyDerivation:
                    useTaskDerivationKey(parent, blobId);
                    return;
                case BlobType::CommunicationKeyDerivation:
                    useCommunicationDerivationKey(parent, blobId);
                    return;
                case BlobType::AuditLogKeyDerivation:
                    useAuditLogDerivationKey(parent, blobId);
                    return;
                case BlobType::KvnrHashKey:
                case BlobType::TelematikIdHashKey:
                case BlobType::VauSig:
                case BlobType::EndorsementKey:
                case BlobType::AttestationPublicKey:
                case BlobType::AttestationKeyPair:
                case BlobType::Quote:
                case BlobType::EciesKeypair:
                    Fail2("BlobType not supported by UseKeyBlobGuard", std::logic_error);
                    break;
            }
            Fail2("BlobType unknown: " + std::to_string(int(type)), std::logic_error);
        }
        ~UseKeyBlobGuard()
        {
            try
            {
                releaseKeyBlob();
            }
            catch (const std::exception& ex)
            {
                TLOG(ERROR) << "Failed to release KeyBlob." << ex.what() ;
            }
        }

        std::function<void(void)> releaseKeyBlob;

    };


private:
    EnvironmentVariableGuard mainCaDerPathGuard{
        "ERP_TSL_INITIAL_CA_DER_PATH", std::string{TEST_DATA_DIR} + "/tsl/TslSignerCertificateIssuer.der"};

    std::shared_ptr<HsmPool> mHsmPool;
    std::unique_ptr<HttpsServer<EnrolmentServiceContext>> mServer;
    std::unique_ptr<KeyDerivation> mKeyDerivation;
    std::unique_ptr<MockDatabase> mMockDatabase;

    std::shared_ptr<HsmPool> hsmPool();
    KeyDerivation& keyDerivation();
    bool isBlobUsed(BlobId blobId);
    std::shared_ptr<BlobCache> createBlobCache (void)
    {
        if (usePostgres)
        {
            return std::make_shared<BlobCache>(std::make_unique<ProductionBlobDatabase>());
        }
        else
        {
            auto mockBlobDatabase = std::make_unique<MockBlobDatabase>();
            mockBlobDatabase->setBlobInUseTest( [this](BlobId blobId)
                {
                    return isBlobUsed(blobId);
                });
            return std::make_shared<BlobCache>(std::move(mockBlobDatabase));
        }
    }
};

inline bool EnrolmentServerTest::isBlobUsed(BlobId blobId)
{
    auto db = database();
    auto& backend = db->getBackend();
    auto* mockBackend = dynamic_cast<const MockDatabaseProxy*>(&backend);
    Expect3(mockBackend, "isBlobUsed should only be called with MockDatabase", std::logic_error);
    return mockBackend->isBlobUsed(blobId);
}

inline KeyDerivation & EnrolmentServerTest::keyDerivation()
{
    if (!mKeyDerivation)
    {
        mKeyDerivation.reset(new KeyDerivation(*hsmPool()));
    }
    return *mKeyDerivation;
}

inline std::unique_ptr<Database> EnrolmentServerTest::database()
{
    if (usePostgres)
    {
        return std::make_unique<DatabaseFrontend>(std::make_unique<PostgresBackend>(), *hsmPool(), keyDerivation());
    }
    else
    {
        if (!mMockDatabase)
        {
            mMockDatabase.reset(new MockDatabase{*hsmPool()});
        }
        return std::make_unique<DatabaseFrontend>(
            std::make_unique<MockDatabaseProxy>(*mMockDatabase), *hsmPool(), keyDerivation());
    }
}

inline std::shared_ptr<HsmPool> EnrolmentServerTest::hsmPool()
{
    if (!mHsmPool)
    {
        mHsmPool.reset(new HsmPool(
                           std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(), blobCache),
                           TeeTokenUpdater::createMockTeeTokenUpdaterFactory()));
    }
    return mHsmPool;
}

TEST_P(EnrolmentServerTest, GetEnclaveStatus)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::GET, "/Enrolment/EnclaveStatus"),
            ""));

    checkResponseHeader(response, HttpStatus::OK);

    const auto secondsSinceEpoch = toSecondsSinceEpoch(std::chrono::system_clock::now());

    EnrolmentModel model (response.getBody());
    // Verify that the enclave id is a unique id. However this may not be valid for the production version.
    const auto id = model.getString(enrolment::GetEnclaveStatus::responseEnclaveId);
    ASSERT_TRUE(Uuid(id).isValidIheUuid());
    // The difference between the enclave time and "now" should only be as large as it takes to return that value back to us, i.e. very small.
    ASSERT_LT(model.getInt64(enrolment::GetEnclaveStatus::responseEnclaveTime) - secondsSinceEpoch, 1);
    // Initial state is "NotEnrolled".
    ASSERT_EQ(model.getString(enrolment::GetEnclaveStatus::responseEnrolmentStatus), "NotEnrolled");

    EXPECT_EQ(std::string(ErpServerInfo::BuildVersion), model.getString(enrolment::GetEnclaveStatus::responseVersionBuild));
    EXPECT_EQ(std::string(ErpServerInfo::BuildType), model.getString(enrolment::GetEnclaveStatus::responseVersionBuildType));
    EXPECT_EQ(std::string(ErpServerInfo::ReleaseVersion), model.getString(enrolment::GetEnclaveStatus::responseVersionRelease));
    EXPECT_EQ(std::string(ErpServerInfo::ReleaseDate), model.getString(enrolment::GetEnclaveStatus::responseVersionReleasedate));
}


TEST_P(EnrolmentServerTest, GetEndorsementKey)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::GET, "/Enrolment/EndorsementKey"),
            ""));

    ASSERT_EQ(response.getHeader().status(), HttpStatus::OK);

    EnrolmentModel model (response.getBody());

    // At least for the production case we don't have access to expected values, other than what is in the database.
    // Just verify that the values are not empty.
    ASSERT_FALSE(model.getString(enrolment::GetEndorsementKey::responseCertificate).empty());
    ASSERT_FALSE(model.getString(enrolment::GetEndorsementKey::responseEkName).empty());
}


TEST_P(EnrolmentServerTest, GetAttestationKey)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::GET, "/Enrolment/AttestationKey"),
            ""));

    checkResponseHeader(response, HttpStatus::OK);

    EnrolmentModel model (response.getBody());
    // At least for the production case we don't have access to expected values, other than what is in the database.
    // Just verify that the values are not empty.
    ASSERT_FALSE(model.getString(enrolment::GetAttestationKey::responsePublicKey).empty());
    ASSERT_FALSE(model.getString(enrolment::GetAttestationKey::responseAkName).empty());
}


/**
 * Disabled because input data are based on the software attestation process which must be
 * repeated after the TPM has been restarted. And if this test fails the TPM has to be restarted again.
 */
TEST_P(EnrolmentServerTest, DISABLED_PostAuthenticateCredential)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::POST, "/Enrolment/AuthenticateCredential"),
            R"({
                    "secret": "ACCMno94o+hePIgdbvWa6JvvX5vrrFfEpzeZF4X1/8ZTiAAg5ntsPvEsiTaqLOilvDDyJ63A3OV3D6AHP8WKNo3UXH4=",
                    "credential": "ACDICBOju8CAMmpxTJUGOzBAwtbAKpqzEulOS8YRHB/SRj3s2OsgAQeF2vyvmLdqwF6Yb2h8lGl1B43SDTsl8yZ8gEY=",
                    "akName": "AAuYMqOy91+RXl/w1LNlkItLVih0UAJ5qFUVZxo5FO3/5A=="
                }
            )"));


    checkResponseHeader(response, HttpStatus::OK);

    EnrolmentModel model (response.getBody());
    ASSERT_EQ(model.getString(enrolment::PostAuthenticationCredential::responsePlainTextCredential), "Hb3mbn5C6VCKXgLMtD4gEp69dfd+klXaQSlgJdjIn00=");
}


TEST_P(EnrolmentServerTest, PostGetQuote_success)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::POST, "/Enrolment/GetQuote"),
            R"(
                {
                    "nonce": "Mj6+9LCsIeQK78/lWuMDIEsXb+1Ur5V8gar8i8HK9KY=",
                    "pcrSet": [0,3,4],
                    "hashAlgorithm": "SHA256"
                }
            )"));

    checkResponseHeader(response, HttpStatus::OK);

    EnrolmentModel model (response.getBody());
    // There seems to be an element of randomness in the returned values.
    // Verifying the length of the returned values is the best we can do at the moment.
    EXPECT_EQ(Base64::decode(model.getString(enrolment::PostGetQuote::responseQuotedData)).size(), 145);
    EXPECT_EQ(Base64::decode(model.getString(enrolment::PostGetQuote::responseQuoteSignature)).size(), 72);
}


TEST_P(EnrolmentServerTest, PostGetQuote_successWithConfiguredPcrSet)
{
    EnvironmentVariableGuard guard ("ERP_PCR_SET", "[1,2,3]");

    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::POST, "/Enrolment/GetQuote"),
            R"(
                {
                    "nonce": "Mj6+9LCsIeQK78/lWuMDIEsXb+1Ur5V8gar8i8HK9KY=",
                    "pcrSet": [1,2,3],
                    "hashAlgorithm": "SHA256"
                }
            )"));

    checkResponseHeader(response, HttpStatus::OK);

    EnrolmentModel model (response.getBody());
    // There seems to be an element of randomness in the returned values.
    // Verifying the length of the returned values is the best we can do at the moment.
    EXPECT_EQ(Base64::decode(model.getString(enrolment::PostGetQuote::responseQuotedData)).size(), 145);
    EXPECT_EQ(Base64::decode(model.getString(enrolment::PostGetQuote::responseQuoteSignature)).size(), 72);
}


TEST_P(EnrolmentServerTest, PostGetQuote_failForUnexpectedPcrSet)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::POST, "/Enrolment/GetQuote"),
            R"(
                {
                    "nonce": "Mj6+9LCsIeQK78/lWuMDIEsXb+1Ur5V8gar8i8HK9KY=",
                    "pcrSet": [1,2,3],
                    "hashAlgorithm": "SHA256"
                }
            )"));

    ASSERT_EQ(response.getHeader().status(), HttpStatus::BadRequest);
}


TEST_P(EnrolmentServerTest, PostGetQuote_failForInvalidPcrRegister)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::POST, "/Enrolment/GetQuote"),
            R"(
                {
                    "nonce": "dGhlIG5vbmNl",
                    "pcrSet": [16],
                    "hashAlgorithm": "SHA256"
                }
            )"));

    ASSERT_EQ(response.getHeader().status(), HttpStatus::BadRequest);
}


TEST_P(EnrolmentServerTest, PostGetQuote_failForUnsupportedHashAlgorithm)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::POST, "/Enrolment/GetQuote"),
            R"(
                {
                    "nonce": "dGhlIG5vbmNl",
                    "pcrSet": [0],
                    "hashAlgorithm": "SHA1"
                }
            )"));

    ASSERT_EQ(response.getHeader().status(), HttpStatus::BadRequest);
}


TEST_P(EnrolmentServerTest, PutKnownEndorsementKey_success)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::PUT, "/Enrolment/KnownEndorsementKey"),
            createPutBody("123", "black box blob data", 11, {},{},{}, {},{})));

    checkResponseHeader(response, HttpStatus::Created);
    ASSERT_EQ(response.getBody(), "");

    // Retrieve the data from the blob cache and verify that it is the same that we just stored.
    auto entry = blobCache->getBlob(BlobType::EndorsementKey);
    ASSERT_EQ(entry.name, ErpVector::create("123"));
    ASSERT_EQ(entry.blob.data, SafeString("black box blob data"));
    ASSERT_EQ(entry.blob.generation, static_cast<ErpBlob::GenerationId>(11));
}


TEST_P(EnrolmentServerTest, PutKnownEndorsementKey_failForOverwrite)
{
    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/KnownEndorsementKey"),
                createPutBody("123", "black box blob data", 11, {},{},{}, {},{})));
        checkResponseHeader(response, HttpStatus::Created);
    }

    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/KnownEndorsementKey"),
                createPutBody("123", "black box blob data", 11, {},{},{}, {},{})));

        // The second request with identical id is expected to fail.
        ASSERT_EQ(response.getHeader().status(), HttpStatus::BadRequest);
    }
}


TEST_P(EnrolmentServerTest, DeleteKnownEndorsementKey_success)
{
    // Set up the test with an endorsement key that we then can delete.
    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/KnownEndorsementKey"),
                createPutBody("123", "black box blob data", 11, {},{},{}, {},{})));
        checkResponseHeader(response, HttpStatus::Created);
        ASSERT_NO_THROW(blobCache->getBlob(BlobType::EndorsementKey));
    }

    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/KnownEndorsementKey"),
            createDeleteBody("123")));

    checkResponseHeader(response, HttpStatus::NoContent);
    ASSERT_EQ(response.getBody(), "");
    ASSERT_ANY_THROW(blobCache->getBlob(BlobType::EndorsementKey));
}


TEST_P(EnrolmentServerTest, DeleteKnownEndorsementKey_failForUnknownKey)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/KnownEndorsementKey"),
            createDeleteBody("123")));

    ASSERT_EQ(response.getHeader().status(), HttpStatus::NotFound);
}


TEST_P(EnrolmentServerTest, PostKnownAttestationKey_successWithId)
{
    // When only an id is given, then it has to double as ak name and must be 34 bytes long, before base64 encoding.

    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::PUT, "/Enrolment/KnownAttestationKey"),
            createPutBody(mAkName, "black box blob data", 11, {},{},{}, {},{})));

    checkResponseHeader(response, HttpStatus::Created);
    ASSERT_EQ(response.getBody(), "");

    // Retrieve the data from the blob cache and verify that it is the same that we just stored.
    auto entry = blobCache->getBlob(BlobType::AttestationPublicKey);
    ASSERT_EQ(entry.name, ErpVector::create(mAkName));
    ASSERT_TRUE(entry.metaAkName.has_value());
    ASSERT_EQ(entry.metaAkName.value(), ErpArray<TpmObjectNameLength>::create(mAkName));
    ASSERT_EQ(entry.blob.data, SafeString("black box blob data"));
    ASSERT_EQ(entry.blob.generation, static_cast<ErpBlob::GenerationId>(11));
}


TEST_P(EnrolmentServerTest, PostKnownAttestationKey_successWithIdAndAkName)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::PUT, "/Enrolment/KnownAttestationKey"),
            createPutBody(mBlobId, "black box blob data", 11, {},{},{}, ErpArray<TpmObjectNameLength>::create(mAkName), {})));

    checkResponseHeader(response, HttpStatus::Created);
    ASSERT_EQ(response.getBody(), "");

    // Retrieve the data from the blob cache and verify that it is the same that we just stored.
    auto entry = blobCache->getBlob(BlobType::AttestationPublicKey);
    ASSERT_EQ(entry.name, ErpVector::create(mBlobId));
    ASSERT_TRUE(entry.metaAkName.has_value());
    ASSERT_EQ(entry.metaAkName.value(), ErpArray<TpmObjectNameLength>::create(mAkName));
    ASSERT_EQ(entry.blob.data, SafeString("black box blob data"));
    ASSERT_EQ(entry.blob.generation, static_cast<ErpBlob::GenerationId>(11));
}


TEST_P(EnrolmentServerTest, PostKnownAttestationKey_failForOverwrite)
{
    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/KnownAttestationKey"),
                createPutBody(mAkName, "black box blob data", 11, {},{},{}, {},{})));
        checkResponseHeader(response, HttpStatus::Created);
        ASSERT_NO_THROW(blobCache->getBlob(BlobType::AttestationPublicKey));
    }

    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/KnownAttestationKey"),
                createPutBody(mAkName, "black box blob data", 11, {},{},{}, {},{})));

        // The second request with identical id is expected to fail.
        ASSERT_EQ(response.getHeader().status(), HttpStatus::BadRequest);
    }
}


TEST_P(EnrolmentServerTest, DeleteKnownAttestationKey_success)
{
    // Set up the test with an attestation key that we then can delete.
    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/KnownAttestationKey"),
                createPutBody(mAkName, "black box blob data", 11, {},{},{}, {},{})));
        checkResponseHeader(response, HttpStatus::Created);
        ASSERT_NO_THROW(blobCache->getBlob(BlobType::AttestationPublicKey));
    }

    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/KnownAttestationKey"),
            createDeleteBody(mAkName)));

    ASSERT_EQ(response.getHeader().status(), HttpStatus::NoContent);
    ASSERT_EQ(response.getBody(), "");
    ASSERT_ANY_THROW(blobCache->getBlob(BlobType::AttestationPublicKey));
}


TEST_P(EnrolmentServerTest, DeleteKnownAttestationKey_failForUnknownKey)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/KnownAttestationKey"),
            createDeleteBody("123")));

    ASSERT_EQ(response.getHeader().status(), HttpStatus::NotFound);
}


TEST_P(EnrolmentServerTest, PostKnownQuote_successWithoutPcrSet)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::PUT, "/Enrolment/KnownQuote"),
            createPutBody("123", "black box blob data", 11, {},{},{}, {},{})));

    checkResponseHeader(response, HttpStatus::Created);
    ASSERT_EQ(response.getBody(), "");
    auto entry = blobCache->getBlob(BlobType::Quote);
    ASSERT_EQ(entry.name, ErpVector::create("123"));
    ASSERT_EQ(entry.blob.data, SafeString("black box blob data"));
    ASSERT_EQ(entry.blob.generation, static_cast<ErpBlob::GenerationId>(11));
    ASSERT_TRUE(entry.metaPcrSet.has_value());
    ASSERT_EQ(entry.metaPcrSet.value(), PcrSet::defaultSet());
}


TEST_P(EnrolmentServerTest, PostKnownQuote_successWithPcrSet)
{
    const auto pcrSet = PcrSet::fromString("3,4,5");

    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::PUT, "/Enrolment/KnownQuote"),
            createPutBody("123", "black box blob data", 11, {},{},{}, {},pcrSet)));

    checkResponseHeader(response, HttpStatus::Created);
    ASSERT_EQ(response.getBody(), "");
    auto entry = blobCache->getBlob(BlobType::Quote);
    ASSERT_EQ(entry.name, ErpVector::create("123"));
    ASSERT_EQ(entry.blob.data, SafeString("black box blob data"));
    ASSERT_EQ(entry.blob.generation, static_cast<ErpBlob::GenerationId>(11));
    ASSERT_TRUE(entry.metaPcrSet.has_value());
    ASSERT_EQ(entry.metaPcrSet.value(), PcrSet::fromString("3,4,5"));
}


TEST_P(EnrolmentServerTest, PostKnownQuote_failForOverwrite)
{
    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/KnownQuote"),
                createPutBody("123", "black box blob data", 11, {},{},{}, {},{})));
        checkResponseHeader(response, HttpStatus::Created);
        ASSERT_NO_THROW(blobCache->getBlob(BlobType::Quote));
    }

    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/KnownQuote"),
                createPutBody("123", "black box blob data", 11, {},{},{}, {},{})));

        // The second request with identical id is expected to fail.
        ASSERT_EQ(response.getHeader().status(), HttpStatus::BadRequest);
    }
}


TEST_P(EnrolmentServerTest, DeleteKnownQuote_success)
{
    // Set up the test with a known quote that we then can delete.
    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/KnownQuote"),
                createPutBody("123", "black box blob data", 11, {},{},{}, {},{})));
        checkResponseHeader(response, HttpStatus::Created);
        ASSERT_NO_THROW(blobCache->getBlob(BlobType::Quote));
    }

    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/KnownQuote"),
            createDeleteBody("123")));

    ASSERT_EQ(response.getHeader().status(), HttpStatus::NoContent);
    ASSERT_EQ(response.getBody(), "");
    ASSERT_ANY_THROW(blobCache->getBlob(BlobType::Quote));
}


TEST_P(EnrolmentServerTest, DeleteKnownQuote_failForUnknownKey)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/KnownQuote"),
            createDeleteBody("123")));

    ASSERT_EQ(response.getHeader().status(), HttpStatus::NotFound);
}


TEST_P(EnrolmentServerTest, PostEciesKeypairBlob_success)
{
    const auto expiry = std::chrono::system_clock::now() + std::chrono::seconds(60);
    const auto expirySeconds = toSecondsSinceEpoch(expiry);

    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::PUT, "/Enrolment/EciesKeypair"),
            createPutBody("123", "black box blob data", 11, expirySeconds,{},{}, {},{})));
    checkResponseHeader(response, HttpStatus::Created);
    ASSERT_EQ(response.getBody(), "");

    const auto entry = blobCache->getBlob(BlobType::EciesKeypair);
    ASSERT_EQ(entry.name, ErpVector::create("123"));
    ASSERT_EQ(entry.blob.data, SafeString("black box blob data"));
    ASSERT_EQ(entry.blob.generation, static_cast<ErpBlob::GenerationId>(11));
    ASSERT_TRUE(entry.expiryDateTime.has_value());
    ASSERT_EQ(toSecondsSinceEpoch(entry.expiryDateTime.value()), expirySeconds);
}


TEST_P(EnrolmentServerTest, DeleteEciesKeypairBlob_success)
{
    const auto expiry = std::chrono::system_clock::now() + std::chrono::seconds(60);
    const auto expirySeconds = toSecondsSinceEpoch(expiry);

    // Add an ecies keypair to delete.
    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/EciesKeypair"),
                createPutBody("123", "black box blob data", 11, expirySeconds,{},{}, {},{})));
        checkResponseHeader(response, HttpStatus::Created);
        ASSERT_NO_THROW(blobCache->getBlob(BlobType::EciesKeypair));
    }

    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/EciesKeypair"),
            createDeleteBody("123")));

    checkResponseHeader(response, HttpStatus::NoContent);
    ASSERT_EQ(response.getBody(), "");
    ASSERT_ANY_THROW(blobCache->getBlob(BlobType::EciesKeypair));
}


TEST_P(EnrolmentServerTest, PostAndDeleteDerivationKey_success)
{
    const auto start = std::chrono::system_clock::now() - std::chrono::seconds(60);
    const auto end = std::chrono::system_clock::now() + std::chrono::seconds(60);
    const auto startSeconds = toSecondsSinceEpoch(start);
    const auto endSeconds = toSecondsSinceEpoch(end);

    struct Descriptor {std::string name; BlobType type;};
    for (const Descriptor& resource : {Descriptor{"Task",          BlobType::TaskKeyDerivation},
                                       Descriptor{"Communication", BlobType::CommunicationKeyDerivation},
                                       Descriptor{"AuditLog",      BlobType::AuditLogKeyDerivation}})
    {
        std::string blobName = resource.name + "PostAndDeleteDerivationKey_success";
        std::optional<BlobId> initialBlobId;
        try
        {
            initialBlobId = blobCache->getBlob(resource.type).id;
        }
        catch (const ErpException&)
        { // if there is no initial blob, `initialId` will remain empty
        }

        // Upload the resource
        {
            auto response = createClient().send(
                ClientRequest(
                    createHeader(HttpMethod::PUT, "/Enrolment/" + resource.name + "/DerivationKey"),
                    createPutBody(blobName, "black box blob data", 11, {},startSeconds,endSeconds, {},{})));
            checkResponseHeader(response, HttpStatus::Created);
            ASSERT_EQ(response.getBody(), "");

            const auto entry = blobCache->getBlob(resource.type);
            ASSERT_EQ(entry.name, ErpVector::create(blobName));
            ASSERT_EQ(entry.blob.data, SafeString("black box blob data"));
            ASSERT_EQ(entry.blob.generation, static_cast<ErpBlob::GenerationId>(11));
            ASSERT_TRUE(entry.startDateTime.has_value());
            ASSERT_EQ(toSecondsSinceEpoch(entry.startDateTime.value()), startSeconds);
            ASSERT_TRUE(entry.endDateTime.has_value());
            ASSERT_EQ(toSecondsSinceEpoch(entry.endDateTime.value()), endSeconds);
        }

        // And delete it.
        {
            auto response = createClient().send(
                ClientRequest(
                    createHeader(HttpMethod::DELETE, "/Enrolment/" + resource.name + "/DerivationKey"),
                    createDeleteBody(blobName)));
            checkResponseHeader(response, HttpStatus::NoContent);
            ASSERT_EQ(response.getBody(), "");
            std::optional<BlobId> finalBlobId;
            try
            {
                finalBlobId = blobCache->getBlob(resource.type).id;
            }
            catch (const ErpException&)
            { // if there is no initial blob, `finalBlobId` will remain empty
            }
            ASSERT_EQ(initialBlobId, finalBlobId);
        }
    }
}



TEST_P(EnrolmentServerTest, PostAndDeleteDerivationKey_usedKeys)
{
    const auto start = std::chrono::system_clock::now() - std::chrono::seconds(60);
    const auto end = std::chrono::system_clock::now() + std::chrono::seconds(60);
    const auto startSeconds = toSecondsSinceEpoch(start);
    const auto endSeconds = toSecondsSinceEpoch(end);

    struct Descriptor {std::string name; BlobType type;};
    for (const Descriptor& resource : {Descriptor{"Task",          BlobType::TaskKeyDerivation},
                                       Descriptor{"Communication", BlobType::CommunicationKeyDerivation},
                                       Descriptor{"AuditLog",      BlobType::AuditLogKeyDerivation}})
    {
        BlobId blobId;
        // Upload the resource
        {
            auto response = createClient().send(
                ClientRequest(
                    createHeader(HttpMethod::PUT, "/Enrolment/" + resource.name + "/DerivationKey"),
                    createPutBody(resource.name + "123", "black box blob data", 11, {},startSeconds,endSeconds, {},{})));
            checkResponseHeader(response, HttpStatus::Created);
            ASSERT_EQ(response.getBody(), "");

            const auto entry = blobCache->getBlob(resource.type);
            ASSERT_EQ(entry.name, ErpVector::create(resource.name + "123"));
            ASSERT_EQ(entry.blob.data, SafeString("black box blob data"));
            ASSERT_EQ(entry.blob.generation, static_cast<ErpBlob::GenerationId>(11));
            ASSERT_TRUE(entry.startDateTime.has_value());
            ASSERT_EQ(toSecondsSinceEpoch(entry.startDateTime.value()), startSeconds);
            ASSERT_TRUE(entry.endDateTime.has_value());
            ASSERT_EQ(toSecondsSinceEpoch(entry.endDateTime.value()), endSeconds);
            blobId = entry.id;
        }

        // Try to delete used Blob
        {
            UseKeyBlobGuard useBlob{*this, resource.type, blobId};
            auto response = createClient().send(
                ClientRequest(
                    createHeader(HttpMethod::DELETE, "/Enrolment/" + resource.name + "/DerivationKey"),
                    createDeleteBody(resource.name + "123")));

            checkResponseHeader(response, HttpStatus::Conflict);
            EXPECT_EQ(response.getBody(), "");
            EXPECT_NO_THROW(blobCache->getBlob(resource.type, blobId));
        }

        // And delete it.
        {
            auto response = createClient().send(
                ClientRequest(
                    createHeader(HttpMethod::DELETE, "/Enrolment/" + resource.name + "/DerivationKey"),
                    createDeleteBody(resource.name + "123")));

            checkResponseHeader(response, HttpStatus::NoContent);
            EXPECT_EQ(response.getBody(), "");
            EXPECT_ANY_THROW(blobCache->getBlob(resource.type, blobId));
        }
    }
}


TEST_P(EnrolmentServerTest, PutKvnrHashKey_success)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::PUT, "/Enrolment/KvnrHashKey"),
            createPutBody("123", "black box blob data", 11, {},{},{}, {},{})));

    checkResponseHeader(response, HttpStatus::Created);
    ASSERT_EQ(response.getBody(), "");
    const auto entry = blobCache->getBlob(BlobType::KvnrHashKey);
    ASSERT_EQ(entry.name, ErpVector::create("123"));
    ASSERT_EQ(entry.blob.data, SafeString("black box blob data"));
    ASSERT_EQ(entry.blob.generation, static_cast<ErpBlob::GenerationId>(11));
}


TEST_P(EnrolmentServerTest, DeleteKvnrHashKey_success)
{
    // Set up a hash key that can be deleted.
    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/KvnrHashKey"),
                createPutBody("123", "black box blob data", 11, {},{},{}, {},{})));
        checkResponseHeader(response, HttpStatus::Created);
        ASSERT_EQ(response.getBody(), "");
        ASSERT_NO_THROW(blobCache->getBlob(BlobType::KvnrHashKey));
    }

    // And delete it.
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/KvnrHashKey"),
            createDeleteBody("123")));

    checkResponseHeader(response, HttpStatus::NoContent);
    ASSERT_EQ(response.getBody(), "");
    ASSERT_ANY_THROW(blobCache->getBlob(BlobType::KvnrHashKey));
}


TEST_P(EnrolmentServerTest, PutTelematikIdHashKey_success)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::PUT, "/Enrolment/TelematikIdHashKey"),
            createPutBody("123", "black box blob data", 11, {},{},{}, {},{})));

    checkResponseHeader(response, HttpStatus::Created);
    ASSERT_EQ(response.getBody(), "");
    const auto entry = blobCache->getBlob(BlobType::TelematikIdHashKey);
    ASSERT_EQ(entry.name, ErpVector::create("123"));
    ASSERT_EQ(entry.blob.data, SafeString("black box blob data"));
    ASSERT_EQ(entry.blob.generation, static_cast<ErpBlob::GenerationId>(11));
}


TEST_P(EnrolmentServerTest, DeleteTelematikIdHashKey_success)
{
    // Set up a hash key that can be deleted.
    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/TelematikIdHashKey"),
                createPutBody("123", "black box blob data", 11, {},{},{}, {},{})));
        checkResponseHeader(response, HttpStatus::Created);
        ASSERT_EQ(response.getBody(), "");
        ASSERT_NO_THROW(blobCache->getBlob(BlobType::TelematikIdHashKey));
    }

    // And delete it.
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/TelematikIdHashKey"),
            createDeleteBody("123")));

    checkResponseHeader(response, HttpStatus::NoContent);
    ASSERT_EQ(response.getBody(), "");
    ASSERT_ANY_THROW(blobCache->getBlob(BlobType::TelematikIdHashKey));
}




TEST_P(EnrolmentServerTest, PutVauSig_success)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::PUT, "/Enrolment/VauSig"),
            createPutBody("123", "black box blob data", 11, {},{},{}, {},{})));
    checkResponseHeader(response, HttpStatus::Created);
    ASSERT_EQ(response.getBody(), "");
    const auto entry = blobCache->getBlob(BlobType::VauSig);
    ASSERT_EQ(entry.name, ErpVector::create("123"));
    ASSERT_EQ(entry.blob.data, SafeString("black box blob data"));
    ASSERT_EQ(entry.blob.generation, static_cast<ErpBlob::GenerationId>(11));
}


TEST_P(EnrolmentServerTest, DeleteVauSig_success)
{
    // Set up a hash key that can be deleted.
    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/VauSig"),
            createPutBody("123", "black box blob data", 11, {},{},{}, {},{})));
        checkResponseHeader(response, HttpStatus::Created);
        ASSERT_EQ(response.getBody(), "");
        ASSERT_NO_THROW(blobCache->getBlob(BlobType::VauSig));
    }

    // And delete it.
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/VauSig"),
            createDeleteBody("123")));

    checkResponseHeader(response, HttpStatus::NoContent);
    ASSERT_EQ(response.getBody(), "");
    ASSERT_ANY_THROW(blobCache->getBlob(BlobType::VauSig));
}


/**
 * This test will, in this order, store endorsement key, attestation key and quote and verify that each
 * operation results in a change of the enrolment status from NotEnrolled to EkKnown to AtKnown to QuoteKnown.
 * After that it will delete the endorsement key and verify that the enrolment status goes back to NotEnrolled.
 */
TEST_P(EnrolmentServerTest, EnrolmentStatus)
{
    // Store an endorsement key.
    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/KnownEndorsementKey"),
                createPutBody("ek-id", "black box blob data", 11, {},{},{}, {},{})));
        checkResponseHeader(response, HttpStatus::Created);
        ASSERT_NO_THROW(blobCache->getBlob(BlobType::EndorsementKey));
    }

    // Verify that the enclave status changed to EkKnown.
    {
        auto statusResponse = createClient().send(ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/EnclaveStatus"), ""));
        checkResponseHeader(statusResponse, HttpStatus::OK);
        EnrolmentModel model (statusResponse.getBody());
        ASSERT_EQ(model.getString(enrolment::GetEnclaveStatus::responseEnrolmentStatus), "EkKnown");
    }

    // Store an attestation key.
    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/KnownAttestationKey"),
                createPutBody(mAkName, "black box blob data", 11, {},{},{}, {},{})));
        ASSERT_EQ(response.getHeader().status(), HttpStatus::Created);
        ASSERT_NO_THROW(blobCache->getBlob(BlobType::AttestationPublicKey));
    }

    // Verify that the enclave status changed to AtKnown.
    {
        auto statusResponse = createClient().send(ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/EnclaveStatus"), ""));
        checkResponseHeader(statusResponse, HttpStatus::OK);
        EnrolmentModel model (statusResponse.getBody());
        ASSERT_EQ(model.getString(enrolment::GetEnclaveStatus::responseEnrolmentStatus), "AkKnown");
    }

    // Store a quote.
    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/KnownQuote"),
                createPutBody("quote-id", "black box blob data", 11, {},{},{}, {},{})));
        checkResponseHeader(response, HttpStatus::Created);
        ASSERT_NO_THROW(blobCache->getBlob(BlobType::Quote));
    }

    // Verify that the enclave status changed to QuoteKnown.
    {
        auto statusResponse = createClient().send(ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/EnclaveStatus"), ""));
        checkResponseHeader(statusResponse, HttpStatus::OK);
        EnrolmentModel model (statusResponse.getBody());
        ASSERT_EQ(model.getString(enrolment::GetEnclaveStatus::responseEnrolmentStatus), "QuoteKnown");
    }

    // Delete the endorsement key.
    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::DELETE, "/Enrolment/KnownEndorsementKey"),
                createDeleteBody("ek-id")));
        checkResponseHeader(response, HttpStatus::NoContent);
        ASSERT_ANY_THROW(blobCache->getBlob(BlobType::EndorsementKey));
    }

    // Verify that the enclave status changed back to NotEnrolled.
    {
        auto statusResponse = createClient().send(ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/EnclaveStatus"), ""));
        checkResponseHeader(statusResponse, HttpStatus::OK);
        EnrolmentModel model (statusResponse.getBody());
        ASSERT_EQ(model.getString(enrolment::GetEnclaveStatus::responseEnrolmentStatus), "NotEnrolled");
    }
}

TEST_P(EnrolmentServerTest, TestBasicAuthorization)
{
    const std::string auth = "c3VwZXJ1c2VyOnN1cGVyc2VjcmV0";

    {
        // Test with supported authorization type ("basic") and proper credentials.
        EnvironmentVariableGuard disableEnrolmentApiAuth{"DEBUG_DISABLE_ENROLMENT_API_AUTH", "false"};
        EnvironmentVariableGuard enrolmentApiCredentials{"ERP_ENROLMENT_API_CREDENTIALS", auth};
        auto request = ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/EndorsementKey"), "");
        request.setHeader(Header::Authorization, "Basic " + auth);
        auto response = createClient().send(request);
        checkResponseHeader(response, HttpStatus::OK);
    }

    {
        // Test with wrong credentials.
        EnvironmentVariableGuard disableEnrolmentApiAuth{"DEBUG_DISABLE_ENROLMENT_API_AUTH", "false"};
        EnvironmentVariableGuard enrolmentApiCredentials{"ERP_ENROLMENT_API_CREDENTIALS", "123"};
        auto request = ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/EndorsementKey"), "");
        request.setHeader(Header::Authorization, "Basic " + auth);
        auto response = createClient().send(request);
        ASSERT_EQ(response.getHeader().status(), HttpStatus::Unauthorized);
    }

    {
        // Test with unsupported authorization type ("digest").
        EnvironmentVariableGuard disableEnrolmentApiAuth{"DEBUG_DISABLE_ENROLMENT_API_AUTH", "false"};
        EnvironmentVariableGuard enrolmentApiCredentials{"ERP_ENROLMENT_API_CREDENTIALS", "123"};
        auto request = ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/EndorsementKey"), "");
        request.setHeader(Header::Authorization, "Digest " + auth);
        auto response = createClient().send(request);
        ASSERT_EQ(response.getHeader().status(), HttpStatus::Unauthorized);
    }

    {
        // Test with unsupported authorization type ("digest").
        EnvironmentVariableGuard disableEnrolmentApiAuth{"DEBUG_DISABLE_ENROLMENT_API_AUTH", "false"};
        EnvironmentVariableGuard enrolmentApiCredentials{"ERP_ENROLMENT_API_CREDENTIALS", "123"};
        auto request = ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/EndorsementKey"), "");
        request.setHeader(Header::Authorization, "somethingbad");
        auto response = createClient().send(request);
        ASSERT_EQ(response.getHeader().status(), HttpStatus::Unauthorized);
    }

    {
        // Test with empty authorization header field.
        EnvironmentVariableGuard disableEnrolmentApiAuth{"DEBUG_DISABLE_ENROLMENT_API_AUTH", "false"};
        EnvironmentVariableGuard enrolmentApiCredentials{"ERP_ENROLMENT_API_CREDENTIALS", "123"};
        auto request = ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/EndorsementKey"), "");
        request.setHeader(Header::Authorization, "");
        auto response = createClient().send(request);
        ASSERT_EQ(response.getHeader().status(), HttpStatus::Unauthorized);
    }

    {
        // Test with missing unsupported authorization header field.
        EnvironmentVariableGuard disableEnrolmentApiAuth{"DEBUG_DISABLE_ENROLMENT_API_AUTH", "false"};
        EnvironmentVariableGuard enrolmentApiCredentials{"ERP_ENROLMENT_API_CREDENTIALS", "123"};
        auto request = ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/EndorsementKey"), "");
        auto response = createClient().send(request);
        ASSERT_EQ(response.getHeader().status(), HttpStatus::Unauthorized);
    }
}

InstantiateMockAndProductionTestSuite(
    EnrolmentServer,
    EnrolmentServerTest,
    []{return std::make_unique<Tpm::Factory>(TpmTestHelper::createProductionTpmFactory());},
    []{return std::make_unique<Tpm::Factory>(TpmTestHelper::createMockTpmFactory());});
