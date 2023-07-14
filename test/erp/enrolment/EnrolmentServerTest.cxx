/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/enrolment/EnrolmentServer.hxx"
#include "erp/client/HttpsClient.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/enrolment/EnrolmentModel.hxx"
#include "erp/enrolment/EnrolmentRequestHandler.hxx"
#include "erp/enrolment/VsdmHmacKey.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/crypto/EllipticCurve.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/hsm/production/ProductionBlobDatabase.hxx"
#include "erp/hsm/production/ProductionVsdmKeyBlobDatabase.hxx"
#include "erp/hsm/TeeTokenUpdater.hxx"
#include "erp/tee/OuterTeeResponse.hxx"
#include "erp/tee/OuterTeeRequest.hxx"
#include "erp/tpm/Tpm.hxx"
#if !defined __APPLE__ && !defined _WIN32
#include "erp/tpm/TpmProduction.hxx"
#endif
#include "erp/util/Base64.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/Hash.hxx"
#include "erp/util/Random.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "mock/tpm/TpmMock.hxx"
#include "mock/tpm/TpmTestData.hxx"
#include "mock/tpm/TpmTestHelper.hxx"
#include "mock/util/MockConfiguration.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockDatabaseProxy.hxx"
#include "test/util/BlobDatabaseHelper.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/MockAndProductionTestBase.hxx"
#include "test/util/TestConfiguration.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockVsdmKeyBlobDatabase.hxx"
#include "test/util/StaticData.hxx"

#include <rapidjson/writer.h>
#include <gtest/gtest.h>

#ifdef _WINNT_
#ifdef DELETE
#undef DELETE
#endif
#endif



class EnrolmentServerTest : public MockAndProductionTestBase
{
public:
    static constexpr std::string_view mBlobId = "(012345678901234567890123456789)"; // 32 bytes, simulates a SHA256.
    static constexpr std::string_view mAkName = "< 012345678901234567890123456789 >"; // 34 bytes, simulates a TPM name object.
    std::string defaultBasicAuth = "auth123";
    static constexpr char vsdmOperatorId = '?';

    std::shared_ptr<BlobCache> blobCache;

    EnvironmentVariableGuard enrolmentApiCredentials{ConfigurationKey::ENROLMENT_API_CREDENTIALS, defaultBasicAuth};

    void SetUp (void) override
    {
        MockAndProductionTestBase::SetUp();

        // If the tpm factory (stored in `tpmFactory`) produces an empty reference
        // (because it is a factory that creates a "real" tpm client but that is not currently supported)
        // then skip the test.
        if (tpmFactory(*blobCache) == nullptr || mContext == nullptr)
        {
            GTEST_SKIP();
        }
        blobCache = createBlobCache();

        if (mServer == nullptr)
        {
            // Create and start the server.
            RequestHandlerManager handlers;
            EnrolmentServer::addEndpoints(handlers);
            const auto& config = Configuration::instance();
            mServer = std::make_unique<HttpsServer>(
                "0.0.0.0",
                config.getIntValue(ConfigurationKey::ENROLMENT_SERVER_PORT),
                std::move(handlers),
                *mContext);
            mServer->serve(1, "test");
        }
    }


    void TearDown (void) override
    {
        // Clear the database
        BlobDatabaseHelper::removeUnreferencedBlobs();
        BlobDatabaseHelper::removeTestVsdmKeyBlobs(vsdmOperatorId);

        if (mServer != nullptr)
        {
            mServer->shutDown();
            mServer.reset();
        }
    }


    HttpsClient createClient (void)
    {
        const auto& config = Configuration::instance();
        return HttpsClient("127.0.0.1",
                           gsl::narrow<uint16_t>(config.getIntValue(ConfigurationKey::ENROLMENT_SERVER_PORT)),
                           30 /*connectionTimeoutSeconds*/, false /*enforceServerAuthentication*/);
    }

    Header createHeader(const HttpMethod method, std::string&& target)
    {
        auto header = Header(method, std::move(target), Header::Version_1_1, {}, HttpStatus::Unknown);
        header.addHeaderField(Header::Authorization, "Basic " + defaultBasicAuth);
        return header;
    }

    void checkResponseHeader (const ClientResponse& response, const HttpStatus expectedStatus)
    {
        ASSERT_EQ(response.getHeader().status(), expectedStatus);
        if (!response.getBody().empty())
        {
            ASSERT_EQ(response.getHeader().header(Header::ContentType), static_cast<std::string>(ContentMimeType::jsonUtf8));
        }
    }

    void checkResponseError(const ClientResponse& clientResponse, std::string_view errorDescription)
    {
        ASSERT_FALSE(clientResponse.getBody().empty());
        EnrolmentModel response{clientResponse.getBody()};
        ASSERT_EQ(response.getString("/code"), "INVALID");
        ASSERT_EQ(response.getString("/description"), errorDescription);
    }

    std::string createPutBody (
        const std::string_view id,
        const std::string_view data,
        const size_t generation,
        const std::optional<size_t> expirySecondsSinceEpoch,
        const std::optional<size_t> startSecondsSinceEpoch,
        const std::optional<size_t> endSecondsSinceEpoch,
        const std::optional<ErpArray<TpmObjectNameLength>>& akName,
        const std::optional<PcrSet>& pcrSet)

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

    std::string createEncryptedVsdmKey(const util::Buffer& key)
    {
        auto hsmPool = mContext->getHsmPool().acquire();
        auto& hsmSession = hsmPool.session();
        // create ephemeral key and extract the coordinates
        auto ephemeralKeyPair{EllipticCurve::BrainpoolP256R1->createKeyPair()};
        const auto [x, y] =
            EllipticCurveUtils::getPaddedXYComponents(ephemeralKeyPair, EllipticCurve::KeyCoordinateLength);

        // use the VAU public key for ECIES handling
        auto dh = EllipticCurve::BrainpoolP256R1->createDiffieHellman();
        dh.setPrivatePublicKey(ephemeralKeyPair);
        dh.setPeerPublicKey(hsmSession.getEciesPublicKey());
        const auto iv{SecureRandomGenerator::generate(AesGcm128::IvLength)};
        const auto aesKey = SafeString{
            DiffieHellman::hkdf(dh.createSharedKey(), HsmMockClient::keyDerivationInfo, AesGcm128::KeyLength)};
        // using the derived aesKey, encrypt the hmac key
        const auto encryptedPayload = AesGcm128::encrypt(util::bufferToString(key), aesKey, iv);

        // encode the coordinates with the algorithm explained in A_23463/
        OuterTeeRequest message;
        message.version = '\x01';
        std::copy(x.begin(), x.end(), message.xComponent);
        std::copy(y.begin(), y.end(), message.yComponent);
        const std::string_view ivView = iv;
        std::copy(ivView.begin(), ivView.end(), message.iv);
        message.ciphertext = std::move(encryptedPayload.ciphertext);
        std::copy(encryptedPayload.authenticationTag.begin(), encryptedPayload.authenticationTag.end(),
                  message.authenticationTag);
        return ByteHelper::toHex(message.assemble());
    }

    std::string createVsdmKeyPackage(char operatorId, char version,
                                     const std::optional<util::Buffer>& key = std::nullopt)
    {
        auto package =  VsdmHmacKey{operatorId, version};
        package.set(VsdmHmacKey::jsonKey::exp, "2025-31-12");
        if (key)
        {
            const auto hashHex = ByteHelper::toHex(Hash::hmacSha256(*key, ""));
            package.set(VsdmHmacKey::jsonKey::encryptedKey, createEncryptedVsdmKey(*key));
            package.set(VsdmHmacKey::jsonKey::hmacEmptyString, hashHex);
        }
        return package.serializeToString();
    }

    std::string createVsdmKeyPostBody(uint32_t generation, std::string_view vsdmPackage)
    {
        std::ostringstream os;
        auto hsmPool = mContext->getHsmPool().acquire();
        auto& hsmSession = hsmPool.session();
        const ErpVector vsdmKeyData = ErpVector::create(vsdmPackage);
        ErpBlob blob = hsmSession.wrapRawPayload(vsdmKeyData, generation);
        os << R"({"blob":{"generation":)" << blob.generation << R"(,"data":")" << Base64::encode(blob.data) << "\"}}";
        return os.str();
    }

    std::string createVsdmKeyDeleteBody(char operatorId, char version)
    {
        std::ostringstream os;
        os << R"({"vsdm":{"betreiberkennung":")" << operatorId << R"(","version":")" << version << R"("}})";
        return os.str();
    }

    std::vector<VsdmHmacKey> vsdmHmacKeysFromResponse(const std::string& response)
    {
        std::vector<VsdmHmacKey> vsdmKeys;
        EnrolmentModel model(response);
        const auto& document = model.document();
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        for (const auto* it = document.GetArray().Begin(); it != document.GetArray().End(); ++it)
        {
            writer.Reset(buffer);
            buffer.Clear();
            it->Accept(writer);
            vsdmKeys.emplace_back(buffer.GetString());
        }
        return vsdmKeys;
    }

    size_t toSecondsSinceEpoch (const std::chrono::system_clock::time_point t)
    {
        return gsl::narrow<size_t>(std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch()).count());
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
                          TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>()};
            KeyDerivation derivation{mPool};
        private:
            DummyDerivation(const EnvironmentVariableGuard&) {}
        };

        static db_model::HashedKvnr dummyHashedKvnr()
        {
            return DummyDerivation{}.derivation.hashKvnr(model::Kvnr{"X081547110"});
        }

        static db_model::HashedTelematikId dummyHashedTelematikId()
        {
            return DummyDerivation{}.derivation.hashTelematikId(model::TelematikId{"X-XY-MyDoc"});
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
                                                    dummyHashedTelematikId(), blobId, {}, blobId, {});
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

        void useChargeItemDerivationKey(::EnrolmentServerTest& parent, ::BlobId blobId)
        {
            ::db_model::ChargeItem chargeItem{::model::PrescriptionId::fromDatabaseId(
                ::model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 0)};
            chargeItem.blobId = blobId;
            auto db = parent.database();
            auto& backend = db->getBackend();
            backend.storeChargeInformation(chargeItem, ::db_model::HashedKvnr::fromKvnr(model::Kvnr{"X123456789"}, ::SafeString{}));
            backend.commitTransaction();

            releaseKeyBlob = [&parent, id = chargeItem.prescriptionId] {
                parent.database()->getBackend().deleteChargeInformation(id);
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
                case BlobType::ChargeItemKeyDerivation:
                    useChargeItemDerivationKey(parent, blobId);
                    return;
                case BlobType::KvnrHashKey:
                case BlobType::TelematikIdHashKey:
                case BlobType::VauSig:
                case BlobType::EndorsementKey:
                case BlobType::AttestationPublicKey:
                case BlobType::AttestationKeyPair:
                case BlobType::Quote:
                case BlobType::EciesKeypair:
                case BlobType::PseudonameKey:
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

    std::unique_ptr<HsmPool> mHsmPool;
    std::unique_ptr<HttpsServer> mServer;
    std::unique_ptr<KeyDerivation> mKeyDerivation;
    std::unique_ptr<MockDatabase> mMockDatabase;

    HsmPool& hsmPool();
    KeyDerivation& keyDerivation();
    bool isBlobUsed(BlobId blobId);
    std::shared_ptr<BlobCache> createBlobCache (void)
    {
        if (usePostgres)
        {
            return mContext->getBlobCache();
        }
        else
        {
            auto& mockBlobDatabase = dynamic_cast<MockBlobDatabase&>(mContext->getBlobCache()->getBlobDatabase());
            mockBlobDatabase.setBlobInUseTest([this](BlobId blobId) {
                return isBlobUsed(blobId);
            });
            return mContext->getBlobCache();
        }
    }
};

inline bool EnrolmentServerTest::isBlobUsed(BlobId blobId)
{
    auto db = database();
    auto& backend = db->getBackend();
    const auto* mockBackend = dynamic_cast<const MockDatabaseProxy*>(&backend);
    Expect3(mockBackend, "isBlobUsed should only be called with MockDatabase", std::logic_error);
    return mockBackend->isBlobUsed(blobId);
}

inline KeyDerivation & EnrolmentServerTest::keyDerivation()
{
    if (!mKeyDerivation)
    {
        mKeyDerivation = std::make_unique<KeyDerivation>(hsmPool());
    }
    return *mKeyDerivation;
}

inline std::unique_ptr<Database> EnrolmentServerTest::database()
{
    if (usePostgres)
    {
        return std::make_unique<DatabaseFrontend>(std::make_unique<PostgresBackend>(), hsmPool(), keyDerivation());
    }
    else
    {
        if (!mMockDatabase)
        {
            mMockDatabase = std::make_unique<MockDatabase>(hsmPool());
        }
        return std::make_unique<DatabaseFrontend>(
            std::make_unique<MockDatabaseProxy>(*mMockDatabase), hsmPool(), keyDerivation());
    }
}

inline HsmPool & EnrolmentServerTest::hsmPool()
{
    if (!mHsmPool)
    {
        mHsmPool = std::make_unique<HsmPool>(
                           std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(), blobCache),
                           TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>());
    }
    return *mHsmPool;
}

TEST_P(EnrolmentServerTest, GetEnclaveStatus)//NOLINT(readability-function-cognitive-complexity)
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
    ASSERT_LT(gsl::narrow<size_t>(model.getInt64(enrolment::GetEnclaveStatus::responseEnclaveTime)) - secondsSinceEpoch, 1);
    // Initial state is "QuoteKnown", as set up by MockBlobDatabase::createBlobCache()
    ASSERT_EQ(model.getString(enrolment::GetEnclaveStatus::responseEnrolmentStatus), "QuoteKnown");

    EXPECT_EQ(std::string(ErpServerInfo::BuildVersion()), model.getString(enrolment::GetEnclaveStatus::responseVersionBuild));
    EXPECT_EQ(std::string(ErpServerInfo::BuildType()), model.getString(enrolment::GetEnclaveStatus::responseVersionBuildType));
    EXPECT_EQ(std::string(ErpServerInfo::ReleaseVersion()), model.getString(enrolment::GetEnclaveStatus::responseVersionRelease));
    EXPECT_EQ(std::string(ErpServerInfo::ReleaseDate()), model.getString(enrolment::GetEnclaveStatus::responseVersionReleasedate));
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
        ASSERT_NO_FATAL_FAILURE(checkResponseHeader(response, HttpStatus::Created));
    }

    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/KnownEndorsementKey"),
                createPutBody("123", "black box blob data", 11, {},{},{}, {},{})));

        // The second request with identical id is expected to fail.
        ASSERT_EQ(response.getHeader().status(), HttpStatus::Conflict);
    }
}


TEST_P(EnrolmentServerTest, DeleteKnownEndorsementKey_success)//NOLINT(readability-function-cognitive-complexity)
{
    // Set up the test with an endorsement key that we then can delete.
    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/KnownEndorsementKey"),
                createPutBody("123", "black box blob data", 11, {},{},{}, {},{})));
        checkResponseHeader(response, HttpStatus::Created);
        ASSERT_NO_THROW(blobCache->getBlob(BlobType::EndorsementKey, ErpVector::create("123")));
    }

    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/KnownEndorsementKey"),
            createDeleteBody("123")));

    checkResponseHeader(response, HttpStatus::NoContent);
    ASSERT_EQ(response.getBody(), "");
    ASSERT_ANY_THROW(blobCache->getBlob(BlobType::EndorsementKey, ErpVector::create("123")));
}


TEST_P(EnrolmentServerTest, DeleteKnownEndorsementKey_failForUnknownKey)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/KnownEndorsementKey"),
            createDeleteBody("123")));

    ASSERT_EQ(response.getHeader().status(), HttpStatus::NotFound);
}


TEST_P(EnrolmentServerTest, PostKnownAttestationKey_successWithId)//NOLINT(readability-function-cognitive-complexity)
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


TEST_P(EnrolmentServerTest, PostKnownAttestationKey_successWithIdAndAkName)//NOLINT(readability-function-cognitive-complexity)
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
        ASSERT_EQ(response.getHeader().status(), HttpStatus::Conflict);
    }
}


TEST_P(EnrolmentServerTest, DeleteKnownAttestationKey_success)//NOLINT(readability-function-cognitive-complexity)
{
    // Set up the test with an attestation key that we then can delete.
    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/KnownAttestationKey"),
                createPutBody(mAkName, "black box blob data", 11, {},{},{}, {},{})));
        ASSERT_NO_FATAL_FAILURE(checkResponseHeader(response, HttpStatus::Created));
        ASSERT_NO_THROW(blobCache->getBlob(BlobType::AttestationPublicKey, ErpVector::create(mAkName)));
    }

    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/KnownAttestationKey"),
            createDeleteBody(mAkName)));

    ASSERT_EQ(response.getHeader().status(), HttpStatus::NoContent);
    ASSERT_EQ(response.getBody(), "");
    ASSERT_ANY_THROW(blobCache->getBlob(BlobType::AttestationPublicKey, ErpVector::create(mAkName)));
}


TEST_P(EnrolmentServerTest, DeleteKnownAttestationKey_failForUnknownKey)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/KnownAttestationKey"),
            createDeleteBody("123")));

    ASSERT_EQ(response.getHeader().status(), HttpStatus::NotFound);
}


TEST_P(EnrolmentServerTest, PostKnownQuote_successWithoutPcrSet)//NOLINT(readability-function-cognitive-complexity)
{
#if WITH_HSM_TPM_PRODUCTION > 0
    if (GetParam() == MockBlobCache::MockTarget::MockedHsm)
    {
        GTEST_SKIP_("PostKnownQuote requires HSM simulator");
    }
#endif
    const auto quoteBlob = blobCache->getBlob(BlobType::Quote);

    auto response = createClient().send(ClientRequest(createHeader(HttpMethod::PUT, "/Enrolment/KnownQuote"),
                                                      createPutBody("random-quote", quoteBlob.blob.data, 11, {}, {}, {}, {}, {})));

    ASSERT_NO_FATAL_FAILURE(checkResponseHeader(response, HttpStatus::Created));
    ASSERT_EQ(response.getBody(), "");
    // we have to directly ask the db because the cache may filter it out because the quote is invalid
    auto entry = blobCache->getBlobDatabase().getBlob(BlobType::Quote, ErpVector::create("random-quote"));
    ASSERT_EQ(entry.name, ErpVector::create("random-quote"));
    ASSERT_EQ(entry.blob.data, quoteBlob.blob.data);
    ASSERT_EQ(entry.blob.generation, static_cast<ErpBlob::GenerationId>(11));
    ASSERT_TRUE(entry.pcrSet.has_value());
    ASSERT_EQ(entry.pcrSet.value(), PcrSet::defaultSet());
}


TEST_P(EnrolmentServerTest, PostKnownQuote_successWithPcrSet)//NOLINT(readability-function-cognitive-complexity)
{
#if WITH_HSM_TPM_PRODUCTION > 0
    if (GetParam() == MockBlobCache::MockTarget::MockedHsm)
    {
        GTEST_SKIP_("PostKnownQuote requires HSM simulator");
    }
#endif
    const auto pcrSet = PcrSet::fromString("0,3,4");

    const auto quoteBlob = blobCache->getBlob(BlobType::Quote);

    auto response = createClient().send(
        ClientRequest(createHeader(HttpMethod::PUT, "/Enrolment/KnownQuote"),
                      createPutBody("random-quotepcr", quoteBlob.blob.data, 11, {}, {}, {}, {}, pcrSet)));

    ASSERT_NO_FATAL_FAILURE(checkResponseHeader(response, HttpStatus::Created));
    ASSERT_EQ(response.getBody(), "");
    auto entry = blobCache->getBlobDatabase().getBlob(BlobType::Quote, ErpVector::create("random-quotepcr"));
    ASSERT_EQ(entry.name, ErpVector::create("random-quotepcr"));
    ASSERT_EQ(entry.blob.data, quoteBlob.blob.data);
    ASSERT_EQ(entry.blob.generation, static_cast<ErpBlob::GenerationId>(11));
    ASSERT_TRUE(entry.pcrSet.has_value());
    ASSERT_EQ(entry.pcrSet.value(), PcrSet::fromString("0,3,4"));
}


TEST_P(EnrolmentServerTest, PostKnownQuote_failForOverwrite)
{
#if WITH_HSM_TPM_PRODUCTION > 0
    if (GetParam() == MockBlobCache::MockTarget::MockedHsm)
    {
        GTEST_SKIP_("PostKnownQuote requires HSM simulator");
    }
#endif
    {
        const auto quoteBlob = blobCache->getBlob(BlobType::Quote);
        auto response = createClient().send(
            ClientRequest(createHeader(HttpMethod::PUT, "/Enrolment/KnownQuote"),
                          createPutBody("random-quote123", quoteBlob.blob.data, 11, {}, {}, {}, {}, {})));
        ASSERT_NO_FATAL_FAILURE(checkResponseHeader(response, HttpStatus::Created));
        ASSERT_NO_THROW(blobCache->getBlobDatabase().getBlob(BlobType::Quote, ErpVector::create("random-quote123")));
    }

    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/KnownQuote"),
                createPutBody("random-quote123", "black box blob data", 11, {},{},{}, {},{})));

        // The second request with identical id is expected to fail.
        ASSERT_EQ(response.getHeader().status(), HttpStatus::BadRequest);
    }
}


TEST_P(EnrolmentServerTest, DeleteKnownQuote_success)//NOLINT(readability-function-cognitive-complexity)
{
#if WITH_HSM_TPM_PRODUCTION > 0
    if (GetParam() == MockBlobCache::MockTarget::MockedHsm)
    {
        GTEST_SKIP_("PostKnownQuote requires HSM simulator");
    }
#endif
    // Set up the test with a known quote that we then can delete.
    {
        const auto quoteBlob = blobCache->getBlob(BlobType::Quote);
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/KnownQuote"),
                createPutBody("some-quote", quoteBlob.blob.data, 11, {},{},{}, {},{})));
        ASSERT_NO_FATAL_FAILURE(checkResponseHeader(response, HttpStatus::Created));
        ASSERT_NO_THROW(blobCache->getBlobDatabase().getBlob(BlobType::Quote, ErpVector::create("some-quote")));
    }

    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/KnownQuote"),
            createDeleteBody("some-quote")));

    ASSERT_EQ(response.getHeader().status(), HttpStatus::NoContent);
    ASSERT_EQ(response.getBody(), "");
    ASSERT_ANY_THROW(blobCache->getBlobDatabase().getBlob(BlobType::Quote, ErpVector::create("some-quote")));
}


TEST_P(EnrolmentServerTest, DeleteKnownQuote_failForUnknownKey)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/KnownQuote"),
            createDeleteBody("123")));

    ASSERT_EQ(response.getHeader().status(), HttpStatus::NotFound);
}


TEST_P(EnrolmentServerTest, PostEciesKeypairBlob_success)//NOLINT(readability-function-cognitive-complexity)
{
    const auto expiry = std::chrono::system_clock::now() + std::chrono::seconds(60);
    const auto expirySeconds = toSecondsSinceEpoch(expiry);

    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::PUT, "/Enrolment/EciesKeypair"),
            createPutBody("123", "black box blob data", 11, expirySeconds,{},{}, {},{})));
    ASSERT_NO_FATAL_FAILURE(checkResponseHeader(response, HttpStatus::Created));
    ASSERT_EQ(response.getBody(), "");

    const auto entry = blobCache->getBlob(BlobType::EciesKeypair, ErpVector::create("123"));
    ASSERT_EQ(entry.name, ErpVector::create("123"));
    ASSERT_EQ(entry.blob.data, SafeString("black box blob data"));
    ASSERT_EQ(entry.blob.generation, static_cast<ErpBlob::GenerationId>(11));
    ASSERT_TRUE(entry.expiryDateTime.has_value());
    ASSERT_EQ(toSecondsSinceEpoch(entry.expiryDateTime.value()), expirySeconds);
}


TEST_P(EnrolmentServerTest, DeleteEciesKeypairBlob_success)//NOLINT(readability-function-cognitive-complexity)
{
    const auto expiry = std::chrono::system_clock::now() + std::chrono::seconds(60);
    const auto expirySeconds = toSecondsSinceEpoch(expiry);

    // Add an ecies keypair to delete.
    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/EciesKeypair"),
                createPutBody("123", "black box blob data", 11, expirySeconds,{},{}, {},{})));
        ASSERT_NO_FATAL_FAILURE(checkResponseHeader(response, HttpStatus::Created));
        ASSERT_NO_THROW(blobCache->getBlob(BlobType::EciesKeypair, ErpVector::create("123")));
    }

    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/EciesKeypair"),
            createDeleteBody("123")));

    ASSERT_NO_FATAL_FAILURE(checkResponseHeader(response, HttpStatus::NoContent));
    ASSERT_EQ(response.getBody(), "");
    ASSERT_ANY_THROW(blobCache->getBlob(BlobType::EciesKeypair, ErpVector::create("123")));
}


TEST_P(EnrolmentServerTest, PostAndDeleteDerivationKey_success)//NOLINT(readability-function-cognitive-complexity)
{
    struct Descriptor {std::string name; BlobType type;};
    for (const Descriptor& resource : {Descriptor{"Task",          BlobType::TaskKeyDerivation},
                                       Descriptor{"Communication", BlobType::CommunicationKeyDerivation},
                                       Descriptor{"AuditLog", BlobType::AuditLogKeyDerivation},
                                       Descriptor{"ChargeItem", ::BlobType::ChargeItemKeyDerivation}})
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
                    createPutBody(blobName, "black box blob data", 11, {}, {}, {}, {},{})));
            checkResponseHeader(response, HttpStatus::Created);
            ASSERT_EQ(response.getBody(), "");

            const auto entry = blobCache->getBlob(resource.type, ErpVector::create(blobName));
            ASSERT_EQ(entry.name, ErpVector::create(blobName));
            ASSERT_EQ(entry.blob.data, SafeString("black box blob data"));
            ASSERT_EQ(entry.blob.generation, static_cast<ErpBlob::GenerationId>(11));
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



TEST_P(EnrolmentServerTest, PostAndDeleteDerivationKey_usedKeys)//NOLINT(readability-function-cognitive-complexity)
{
    struct Descriptor {std::string name; BlobType type;};
    for (const Descriptor& resource : {Descriptor{"Task",          BlobType::TaskKeyDerivation},
                                       Descriptor{"Communication", BlobType::CommunicationKeyDerivation},
                                       Descriptor{"AuditLog", BlobType::AuditLogKeyDerivation},
                                       Descriptor{"ChargeItem", ::BlobType::ChargeItemKeyDerivation}})
    {
        BlobId blobId{};
        std::string name = resource.name + "123";
        // Upload the resource
        {
            auto response = createClient().send(
                ClientRequest(
                    createHeader(HttpMethod::PUT, "/Enrolment/" + resource.name + "/DerivationKey"),
                    createPutBody(name, "black box blob data", 11, {},{} , {}, {},{})));
            checkResponseHeader(response, HttpStatus::Created);
            ASSERT_EQ(response.getBody(), "");

            const auto entry = blobCache->getBlob(resource.type, ErpVector::create(name));
            ASSERT_EQ(entry.name, ErpVector::create(name));
            ASSERT_EQ(entry.blob.data, SafeString("black box blob data"));
            ASSERT_EQ(entry.blob.generation, static_cast<ErpBlob::GenerationId>(11));
            ASSERT_FALSE(entry.startDateTime.has_value());
            ASSERT_FALSE(entry.endDateTime.has_value());
            blobId = entry.id;
        }

        // Try to delete used Blob
        {
            UseKeyBlobGuard useBlob{*this, resource.type, blobId};
            auto response = createClient().send(
                ClientRequest(
                    createHeader(HttpMethod::DELETE, "/Enrolment/" + resource.name + "/DerivationKey"),
                    createDeleteBody(name)));

            checkResponseHeader(response, HttpStatus::Conflict);
            EXPECT_EQ(response.getBody(), "");
            EXPECT_NO_THROW(blobCache->getBlob(resource.type, blobId));
        }

        // And delete it.
        {
            auto response = createClient().send(
                ClientRequest(
                    createHeader(HttpMethod::DELETE, "/Enrolment/" + resource.name + "/DerivationKey"),
                    createDeleteBody(name)));

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
    const auto entry = blobCache->getBlob(BlobType::KvnrHashKey, ErpVector::create("123"));
    ASSERT_EQ(entry.name, ErpVector::create("123"));
    ASSERT_EQ(entry.blob.data, SafeString("black box blob data"));
    ASSERT_EQ(entry.blob.generation, static_cast<ErpBlob::GenerationId>(11));
}


TEST_P(EnrolmentServerTest, DeleteKvnrHashKey_success)//NOLINT(readability-function-cognitive-complexity)
{
    // Set up a hash key that can be deleted.
    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/KvnrHashKey"),
                createPutBody("123", "black box blob data", 11, {},{},{}, {},{})));
        ASSERT_NO_FATAL_FAILURE(checkResponseHeader(response, HttpStatus::Created));
        ASSERT_EQ(response.getBody(), "");
        ASSERT_NO_THROW(blobCache->getBlob(BlobType::KvnrHashKey, ErpVector::create("123")));
    }

    // And delete it.
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/KvnrHashKey"),
            createDeleteBody("123")));

    ASSERT_NO_FATAL_FAILURE(checkResponseHeader(response, HttpStatus::NoContent));
    ASSERT_EQ(response.getBody(), "");
    ASSERT_ANY_THROW(blobCache->getBlob(BlobType::KvnrHashKey, ErpVector::create("123")));
}


TEST_P(EnrolmentServerTest, PutTelematikIdHashKey_success)
{
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::PUT, "/Enrolment/TelematikIdHashKey"),
            createPutBody("123", "black box blob data", 11, {},{},{}, {},{})));

    ASSERT_NO_FATAL_FAILURE(checkResponseHeader(response, HttpStatus::Created));
    ASSERT_EQ(response.getBody(), "");
    const auto entry = blobCache->getBlob(BlobType::TelematikIdHashKey, ErpVector::create("123"));
    ASSERT_EQ(entry.name, ErpVector::create("123"));
    ASSERT_EQ(entry.blob.data, SafeString("black box blob data"));
    ASSERT_EQ(entry.blob.generation, static_cast<ErpBlob::GenerationId>(11));
}


TEST_P(EnrolmentServerTest, DeleteTelematikIdHashKey_success)//NOLINT(readability-function-cognitive-complexity)
{
    // Set up a hash key that can be deleted.
    {
        auto response = createClient().send(
            ClientRequest(
                createHeader(HttpMethod::PUT, "/Enrolment/TelematikIdHashKey"),
                createPutBody("123", "black box blob data", 11, {},{},{}, {},{})));
        checkResponseHeader(response, HttpStatus::Created);
        ASSERT_EQ(response.getBody(), "");
        ASSERT_NO_THROW(blobCache->getBlob(BlobType::TelematikIdHashKey, ErpVector::create("123")));
    }

    // And delete it.
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/TelematikIdHashKey"),
            createDeleteBody("123")));

    checkResponseHeader(response, HttpStatus::NoContent);
    ASSERT_EQ(response.getBody(), "");
    ASSERT_ANY_THROW(blobCache->getBlob(BlobType::TelematikIdHashKey, ErpVector::create("123")));
}


TEST_P(EnrolmentServerTest, PostVauSig_success)
{
    const auto body = createPutBody("123", "black box blob data", 11, {},{},{}, {},{});
    auto model = EnrolmentModel(body);
    const auto pemCert = Certificate::fromBase64(std::string{tpm::vauSigCertificate_base64}).toPem();
    model.set("/certificate", Base64::encode(pemCert));
    auto response = createClient().send(
        ClientRequest(createHeader(HttpMethod::POST, "/Enrolment/VauSig"), model.serializeToString()));
    checkResponseHeader(response, HttpStatus::Created);
    ASSERT_EQ(response.getBody(), "");
    const auto entry = blobCache->getBlob(BlobType::VauSig, ErpVector::create("123"));
    ASSERT_EQ(entry.name, ErpVector::create("123"));
    ASSERT_EQ(entry.blob.data, SafeString("black box blob data"));
    ASSERT_EQ(entry.blob.generation, static_cast<ErpBlob::GenerationId>(11));
    ASSERT_TRUE(entry.certificate.has_value());
    ASSERT_EQ(entry.certificate.value(), pemCert);
}

TEST_P(EnrolmentServerTest, PostVauSigInvalid)
{
    const auto body = createPutBody("123", "black box blob data", 11, {},{},{}, {},{});
    auto model = EnrolmentModel(body);
    model.set("/certificate", tpm::vauSigCertificate_base64);
    auto response = createClient().send(
        ClientRequest(createHeader(HttpMethod::POST, "/Enrolment/VauSig"), model.serializeToString()));
    checkResponseHeader(response, HttpStatus::BadRequest);
}


TEST_P(EnrolmentServerTest, DeleteVauSig_success)//NOLINT(readability-function-cognitive-complexity)
{
    // Set up a hash key that can be deleted.
    {
        const auto body = createPutBody("123", "black box blob data", 11, {},{},{}, {},{});
        auto model = EnrolmentModel(body);
        const auto pemCert = Certificate::fromBase64(std::string{tpm::vauSigCertificate_base64}).toPem();
        model.set("/certificate", Base64::encode(pemCert));
        auto response = createClient().send(
            ClientRequest(createHeader(HttpMethod::POST, "/Enrolment/VauSig"), model.serializeToString()));
        ASSERT_NO_FATAL_FAILURE(checkResponseHeader(response, HttpStatus::Created));
        ASSERT_EQ(response.getBody(), "");
        ASSERT_NO_THROW(blobCache->getBlob(BlobType::VauSig, ErpVector::create("123")));
    }

    // And delete it.
    auto response = createClient().send(
        ClientRequest(
            createHeader(HttpMethod::DELETE, "/Enrolment/VauSig"),
            createDeleteBody("123")));

    ASSERT_NO_FATAL_FAILURE(checkResponseHeader(response, HttpStatus::NoContent));
    ASSERT_EQ(response.getBody(), "");
    ASSERT_ANY_THROW(blobCache->getBlob(BlobType::VauSig, ErpVector::create("123")));
}

TEST_P(EnrolmentServerTest, PostVsdmKey_success)//NOLINT(readability-function-cognitive-complexity)
{
    const char version = '1';
    // Add vsdm hash key that can be deleted.
    {
        const auto key = Random::randomBinaryData(VsdmHmacKey::keyLength);
        const auto vsdmPackage = createVsdmKeyPackage(vsdmOperatorId, version, key);
        auto response = createClient().send(ClientRequest(createHeader(HttpMethod::POST, "/Enrolment/VsdmHmacKey"),
                                                          createVsdmKeyPostBody(0, vsdmPackage)));
        checkResponseHeader(response, HttpStatus::Created);
        ASSERT_TRUE(response.getBody().empty());
        VsdmKeyBlobDatabase::Entry entry;
        ASSERT_NO_THROW(entry = mContext->getVsdmKeyBlobDatabase().getBlob(vsdmOperatorId, version));
    }

    // get all keys
    {
        auto response = createClient().send(ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/VsdmHmacKey"), ""));
        checkResponseHeader(response, HttpStatus::OK);
        ASSERT_FALSE(response.getBody().empty());
        const auto vsdmHmacKeys = vsdmHmacKeysFromResponse(response.getBody());
        bool found = false;
        for (const auto& key : vsdmHmacKeys)
        {
            EXPECT_FALSE(key.hasValue(VsdmHmacKey::jsonKey::encryptedKey));
            EXPECT_FALSE(key.hasValue(VsdmHmacKey::jsonKey::clearTextKey));
            EXPECT_EQ(key.getString(VsdmHmacKey::jsonKey::hmacEmptyString),
                      key.getString(VsdmHmacKey::jsonKey::hmacEmptyStringCalculated));
            if (key.operatorId() == vsdmOperatorId && key.version() == version)
            {
                found = true;
            }
        }
        EXPECT_TRUE(found) << "Could not find key for operator '" << vsdmOperatorId << "' and version '" << version << "'";
    }

    // And finally delete it.
    {
        auto response = createClient().send(ClientRequest(createHeader(HttpMethod::DELETE, "/Enrolment/VsdmHmacKey"),
                                                          createVsdmKeyDeleteBody(vsdmOperatorId, version)));

        checkResponseHeader(response, HttpStatus::NoContent);
        ASSERT_EQ(response.getBody(), "");
        ASSERT_ANY_THROW(mContext->getVsdmKeyBlobDatabase().getBlob(vsdmOperatorId, version));
    }
}

TEST_P(EnrolmentServerTest, PostVsdmKeyDuplicate)
{
    const char version = '1';
    // Add a first vsdm hash key
    {
        const auto key = Random::randomBinaryData(VsdmHmacKey::keyLength);
        const auto vsdmPackage = createVsdmKeyPackage(vsdmOperatorId, version, key);
        auto response = createClient().send(ClientRequest(createHeader(HttpMethod::POST, "/Enrolment/VsdmHmacKey"),
                                                          createVsdmKeyPostBody(0, vsdmPackage)));
        checkResponseHeader(response, HttpStatus::Created);
        ASSERT_TRUE(response.getBody().empty());
        VsdmKeyBlobDatabase::Entry entry;
        ASSERT_NO_THROW(entry = mContext->getVsdmKeyBlobDatabase().getBlob(vsdmOperatorId, version));
    }

    // Add a second with the same version
    {
        const auto key = Random::randomBinaryData(VsdmHmacKey::keyLength);
        const auto vsdmPackage = createVsdmKeyPackage(vsdmOperatorId, version, key);
        auto response = createClient().send(ClientRequest(createHeader(HttpMethod::POST, "/Enrolment/VsdmHmacKey"),
                                                          createVsdmKeyPostBody(0, vsdmPackage)));
        checkResponseHeader(response, HttpStatus::Conflict);
        ASSERT_TRUE(response.getBody().empty());
        VsdmKeyBlobDatabase::Entry entry;
        ASSERT_NO_THROW(entry = mContext->getVsdmKeyBlobDatabase().getBlob(vsdmOperatorId, version));
    }

    // new version should be allowed
    const char anotherVersion = '2';
    {
        const auto key = Random::randomBinaryData(VsdmHmacKey::keyLength);
        const auto vsdmPackage = createVsdmKeyPackage(vsdmOperatorId, anotherVersion, key);
        auto response = createClient().send(ClientRequest(createHeader(HttpMethod::POST, "/Enrolment/VsdmHmacKey"),
                                                          createVsdmKeyPostBody(0, vsdmPackage)));
        checkResponseHeader(response, HttpStatus::Created);
        ASSERT_TRUE(response.getBody().empty());
        VsdmKeyBlobDatabase::Entry entry;
        ASSERT_NO_THROW(entry = mContext->getVsdmKeyBlobDatabase().getBlob(vsdmOperatorId, anotherVersion));
    }
}

TEST_P(EnrolmentServerTest, PostVsdmKeyInvalid)
{
    const char version = '1';
    // try to add invalid keys
    {
        const auto key = Random::randomBinaryData(100);
        const auto vsdmPackage = createVsdmKeyPackage(vsdmOperatorId, version, key);
        EnrolmentModel model{vsdmPackage};
        model.set("/vsdm/encryptedKey", ByteHelper::toHex(util::bufferToString(key)));
        auto response = createClient().send(ClientRequest(createHeader(HttpMethod::POST, "/Enrolment/VsdmHmacKey"),
                                                          createVsdmKeyPostBody(0, vsdmPackage)));
        checkResponseHeader(response, HttpStatus::BadRequest);
        EXPECT_NO_FATAL_FAILURE(checkResponseError(response, "Invalid hmac key length"));
    }

    // try to add a key that is too short
    {
        const auto key = Random::randomBinaryData(31);
        const auto vsdmPackage = createVsdmKeyPackage(vsdmOperatorId, version, key);
        EnrolmentModel model{vsdmPackage};
        model.set("/vsdm/encryptedKey", ByteHelper::toHex(util::bufferToString(key)));
        auto response = createClient().send(ClientRequest(createHeader(HttpMethod::POST, "/Enrolment/VsdmHmacKey"),
                                                          createVsdmKeyPostBody(0, vsdmPackage)));
        checkResponseHeader(response, HttpStatus::BadRequest);
        EXPECT_NO_FATAL_FAILURE(checkResponseError(response, "Invalid hmac key length"));
    }

    // try to add keys with missing hash
    {
        const auto vsdmPackage = createVsdmKeyPackage(vsdmOperatorId, version);
        auto response = createClient().send(ClientRequest(createHeader(HttpMethod::POST, "/Enrolment/VsdmHmacKey"),
                                                          createVsdmKeyPostBody(0, vsdmPackage)));
        checkResponseHeader(response, HttpStatus::BadRequest);
        EXPECT_NO_FATAL_FAILURE(checkResponseError(response, "value for key '/encrypted_key' is missing"));
    }

    // try to add keys with invalid hash
    {
        const auto key = Random::randomBinaryData(VsdmHmacKey::keyLength);
        const auto data = createVsdmKeyPackage(vsdmOperatorId, version, key);
        auto vsdmPackage = EnrolmentModel{data};
        vsdmPackage.set("/hmac_empty_string", "beef");
        auto response = createClient().send(ClientRequest(createHeader(HttpMethod::POST, "/Enrolment/VsdmHmacKey"),
                                                          createVsdmKeyPostBody(0, vsdmPackage.serializeToString())));
        checkResponseHeader(response, HttpStatus::BadRequest);
        EXPECT_NO_FATAL_FAILURE(checkResponseError(response, "VSDM hash validation failed"));
    }
}

TEST_P(EnrolmentServerTest, DeleteVsdmKey_Fail)
{
    const char version = '1';
    // try to delete non-existent blob
    {
        auto response = createClient().send(ClientRequest(createHeader(HttpMethod::DELETE, "/Enrolment/VsdmHmacKey"),
                                                          createVsdmKeyDeleteBody(vsdmOperatorId, version)));

        checkResponseHeader(response, HttpStatus::NotFound);
        ASSERT_EQ(response.getBody(), "");
        ASSERT_ANY_THROW(mContext->getVsdmKeyBlobDatabase().getBlob(vsdmOperatorId, version));
    }
}

/**
 * This test will, in this order, store endorsement key, attestation key and quote and verify that each
 * operation results in a change of the enrolment status from NotEnrolled to EkKnown to AtKnown to QuoteKnown.
 * After that it will delete the endorsement key and verify that the enrolment status goes back to NotEnrolled.
 */
TEST_P(EnrolmentServerTest, EnrolmentStatus)//NOLINT(readability-function-cognitive-complexity)
{
    auto quoteBlob = blobCache->getBlob(BlobType::Quote);
    auto akBlob = blobCache->getBlob(BlobType::AttestationPublicKey);
    auto ekBlob = blobCache->getBlob(BlobType::EndorsementKey);
    const std::set<BlobType> typeList{BlobType::EndorsementKey, BlobType::AttestationPublicKey, BlobType::Quote};
    auto blobs = blobCache->getAllBlobsSortedById();
    for (const auto& blob: blobs)
    {
        if (typeList.contains(blob.type))
        {
            blobCache->deleteBlob(blob.type, blob.name);
        }
    }

    // Store an endorsement key.
    // Verify that the enclave status changed to EkKnown.
    {
        blobCache->storeBlob(std::move(ekBlob));
        auto statusResponse = createClient().send(ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/EnclaveStatus"), ""));
        ASSERT_NO_FATAL_FAILURE(checkResponseHeader(statusResponse, HttpStatus::OK));
        EnrolmentModel model (statusResponse.getBody());
        ASSERT_EQ(model.getString(enrolment::GetEnclaveStatus::responseEnrolmentStatus), "EkKnown");
    }


    // Store an attestation key.
    // Verify that the enclave status changed to AtKnown.
    {
        blobCache->storeBlob(std::move(akBlob));
        auto statusResponse = createClient().send(ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/EnclaveStatus"), ""));
        ASSERT_NO_FATAL_FAILURE(checkResponseHeader(statusResponse, HttpStatus::OK));
        EnrolmentModel model (statusResponse.getBody());
        ASSERT_EQ(model.getString(enrolment::GetEnclaveStatus::responseEnrolmentStatus), "AkKnown");
    }

    // Store a quote.
    // Verify that the enclave status changed to QuoteKnown.
    {
        blobCache->storeBlob(std::move(quoteBlob));
        auto statusResponse = createClient().send(ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/EnclaveStatus"), ""));
        ASSERT_NO_FATAL_FAILURE(checkResponseHeader(statusResponse, HttpStatus::OK));
        EnrolmentModel model (statusResponse.getBody());
        ASSERT_EQ(model.getString(enrolment::GetEnclaveStatus::responseEnrolmentStatus), "QuoteKnown");
    }

    // Delete the endorsement key.
    // Verify that the enclave status changed back to NotEnrolled.
    {
        auto ekBlob = blobCache->getBlob(BlobType::EndorsementKey);
        blobCache->deleteBlob(BlobType::EndorsementKey, ekBlob.name);
        auto statusResponse = createClient().send(ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/EnclaveStatus"), ""));
        ASSERT_NO_FATAL_FAILURE(checkResponseHeader(statusResponse, HttpStatus::OK));
        EnrolmentModel model (statusResponse.getBody());
        ASSERT_EQ(model.getString(enrolment::GetEnclaveStatus::responseEnrolmentStatus), "NotEnrolled");
    }
}

TEST_P(EnrolmentServerTest, TestBasicAuthorization)
{
    const std::string auth = "c3VwZXJ1c2VyOnN1cGVyc2VjcmV0";

    {
        // Test with supported authorization type ("basic") and proper credentials.
        EnvironmentVariableGuard enrolmentApiCredentials{"ERP_ENROLMENT_API_CREDENTIALS", auth};
        auto request = ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/EndorsementKey"), "");
        request.setHeader(Header::Authorization, "Basic " + auth);
        auto response = createClient().send(request);
        checkResponseHeader(response, HttpStatus::OK);
    }

    {
        // Test with wrong credentials.
        EnvironmentVariableGuard enrolmentApiCredentials{"ERP_ENROLMENT_API_CREDENTIALS", "123"};
        auto request = ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/EndorsementKey"), "");
        request.setHeader(Header::Authorization, "Basic " + auth);
        auto response = createClient().send(request);
        ASSERT_EQ(response.getHeader().status(), HttpStatus::Unauthorized);
    }

    {
        // Test with unsupported authorization type ("digest").
        EnvironmentVariableGuard enrolmentApiCredentials{"ERP_ENROLMENT_API_CREDENTIALS", "123"};
        auto request = ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/EndorsementKey"), "");
        request.setHeader(Header::Authorization, "Digest " + auth);
        auto response = createClient().send(request);
        ASSERT_EQ(response.getHeader().status(), HttpStatus::Unauthorized);
    }

    {
        // Test with unsupported authorization type ("digest").
        EnvironmentVariableGuard enrolmentApiCredentials{"ERP_ENROLMENT_API_CREDENTIALS", "123"};
        auto request = ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/EndorsementKey"), "");
        request.setHeader(Header::Authorization, "somethingbad");
        auto response = createClient().send(request);
        ASSERT_EQ(response.getHeader().status(), HttpStatus::Unauthorized);
    }

    {
        // Test with empty authorization header field.
        EnvironmentVariableGuard enrolmentApiCredentials{"ERP_ENROLMENT_API_CREDENTIALS", "123"};
        auto request = ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/EndorsementKey"), "");
        request.setHeader(Header::Authorization, "");
        auto response = createClient().send(request);
        ASSERT_EQ(response.getHeader().status(), HttpStatus::Unauthorized);
    }

    {
        // Test with missing unsupported authorization header field.
        EnvironmentVariableGuard enrolmentApiCredentials{"ERP_ENROLMENT_API_CREDENTIALS", "123"};
        auto request = ClientRequest(createHeader(HttpMethod::GET, "/Enrolment/EndorsementKey"), "");
        auto response = createClient().send(request);
        ASSERT_EQ(response.getHeader().status(), HttpStatus::Unauthorized);
    }
}

INSTANTIATE_TEST_SUITE_P(EnrolmentServer, EnrolmentServerTest,
                         testing::Values(MockBlobCache::MockTarget::SimulatedHsm, MockBlobCache::MockTarget::MockedHsm),
                         [](const ::testing::TestParamInfo<MockBlobCache::MockTarget>& info) {
                             return info.param == MockBlobCache::MockTarget::SimulatedHsm ? "simulated" : "mocked";
                         });
