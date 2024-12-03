/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/VauAutTokenSigner.hxx"
#include "shared/crypto/Jwt.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "mock/hsm/HsmMockClient.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/hsm/BlobCache.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "shared/hsm/HsmSession.hxx"
#include "shared/hsm/production/TeeTokenProductionUpdater.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/String.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/util/BlobDatabaseHelper.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/HsmTestBase.hxx"

#if WITH_HSM_TPM_PRODUCTION > 0
#include "shared/enrolment/EnrolmentHelper.hxx"
#include "shared/hsm/production/HsmProductionClient.hxx"
#include "shared/hsm/production/HsmProductionFactory.hxx"
#endif

#include <gtest/gtest.h>
#include <chrono>


class ParameterSet
{
public:
    std::shared_ptr<BlobCache> blobCache;
    std::shared_ptr<HsmFactory> factory;
    std::shared_ptr<HsmSession> session;
    std::string name;
    bool enabled{};
    MockBlobCache::MockTarget target{};
};
using ParameterSetFactory = std::function<ParameterSet(void)>;

class VauAutTokenSignerTest : public testing::TestWithParam<ParameterSetFactory>
{
public:
    void SetUp() override
    {
        parameters = GetParam()();
        if (! parameters.enabled)
            GTEST_SKIP();
    }

    void TearDown() override
    {
        if (parameters.enabled)
            BlobDatabaseHelper::removeUnreferencedBlobs();
    }

    ParameterSet parameters;
};

namespace
{
ParameterSetFactory createSimulatedParameterSetFactory(void)
{
    ParameterSet parameters;
#if WITH_HSM_TPM_PRODUCTION > 0
    if (HsmTestBase().isHsmSimulatorSupportedAndConfigured())
    {
        return [] {
            ParameterSet parameters;

            parameters.name = "simulated";
            BlobDatabaseHelper::removeUnreferencedBlobs();
            parameters.blobCache = MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::SimulatedHsm);
            parameters.factory =
                std::make_unique<HsmProductionFactory>(std::make_unique<HsmProductionClient>(), parameters.blobCache);
            parameters.session = parameters.factory->connect();
            parameters.enabled = true;
            parameters.target = MockBlobCache::MockTarget::SimulatedHsm;
            // ensure we have a valid TEE token.
            parameters.session->setTeeToken(EnrolmentHelper::getTeeToken(*parameters.session, *parameters.blobCache));

            return parameters;
        };
    }
    else
#endif
    {
        return [] {
            ParameterSet parameters;
            parameters.enabled = false;
            return parameters;
        };
    }
}

ParameterSetFactory createMockedParameterSetFactory(void)
{
    return [] {
        ParameterSet parameters;
        parameters.name = "mocked";
        BlobDatabaseHelper::removeUnreferencedBlobs();
        parameters.blobCache = MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm);
        parameters.factory = std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(), parameters.blobCache);
        parameters.session = parameters.factory->connect();
        parameters.enabled = true;
        parameters.target = MockBlobCache::MockTarget::MockedHsm;
        return parameters;
    };
}
}

TEST_P(VauAutTokenSignerTest, sign)
{
    VauAutTokenSigner signer;
    A_25165_03.test("Test JWT token signature and claims");
    const auto nowTestStart =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
    const auto token = signer.signAuthorizationToken(*parameters.session, "1234");
    const auto nowTestEnd =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
    const auto jwt = JWT(token);
    const auto publicKey = parameters.session->getVauAutPublicKey();
    ASSERT_NO_THROW(jwt.verifySignature(publicKey));
    auto parts = String::split(token, ".");

    auto autKeyBlob = parameters.blobCache->getBlob(BlobType::VauAut);

    auto header = Base64::decodeToString(parts[0]);
    rapidjson::Document headerDoc{};
    headerDoc.Parse(header);
    ASSERT_EQ(std::string{headerDoc["alg"].GetString()}, "ES256");
    ASSERT_EQ(std::string{headerDoc["typ"].GetString()}, "JWT");
    const auto x5cArray = headerDoc["x5c"].GetArray();
    ASSERT_EQ(x5cArray.Size(), 1);
    const auto vauAutCert = Certificate::fromBase64(x5cArray.Begin()->GetString());
    ASSERT_EQ(EVP_PKEY_cmp(vauAutCert.getPublicKey(), publicKey), 1);

    auto payload = Base64::decodeToString(parts[1]);
    rapidjson::Document payloadDoc{};
    payloadDoc.Parse(payload);
    ASSERT_EQ(std::string{payloadDoc["type"].GetString()}, "ePA-Authentisierung über PKI");
    ASSERT_EQ(std::string{payloadDoc["sub"].GetString()}, "9-E-Rezept-Fachdienst");
    ASSERT_EQ(std::string{payloadDoc["challenge"].GetString()}, "1234");
    const auto iat = std::chrono::seconds(payloadDoc["iat"].GetInt64());
    ASSERT_GE(iat, nowTestStart);
    ASSERT_LE(iat, nowTestEnd);
}

INSTANTIATE_TEST_SUITE_P(SimulatedHsm, VauAutTokenSignerTest, testing::Values(createSimulatedParameterSetFactory()),
                         [](auto&) {
                             return "simulated";
                         });

INSTANTIATE_TEST_SUITE_P(MockedHsm, VauAutTokenSignerTest, testing::Values(createMockedParameterSetFactory()),
                         [](auto&) {
                             return "mocked";
                         });
