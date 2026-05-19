/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include <gtest/gtest.h>// should be first or FRIEND_TEST would not work
#include "erp/pc/popp/PoPPCertificateVerifierService.hxx"
#include "mock/client/TlsCertificateVerifierNoVerificationImplementation.hxx"
#include "mock/tsl/UrlRequestSenderMock.hxx"
#include "shared/crypto/EllipticCurve.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/crypto/Jws.hxx"
#include "shared/model/ProfessionOid.hxx"
#include "shared/server/ThreadPool.hxx"
#include "shared/tsl/TrustStore.hxx"
#include "shared/tsl/X509Certificate.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Configuration.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/ResourceManager.hxx"

#include <boost/algorithm/string.hpp>

class PoPPTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_NO_THROW(entityStatementJwt = std::make_unique<JWT>(entityStatement));
        ASSERT_NO_THROW(jwksJwt = std::make_unique<JWT>(jwks));
        mTslManager = createAndSetupTslManager(false);
    }

public:
    struct Key {
        std::string x;
        std::string y;
        shared_EVP_PKEY key;
    };

    Key mSigningKey{genKey()}; // Used for signing the jwts (Entity Statement and the JWKS).
    std::shared_ptr<TslManager> mTslManager;
    std::string jwksUri{"https://erp-mock-idp:8443/popp/jwks.json"};// As defined in the jwt below
    std::string entityStatement{ResourceManager::instance().getStringResource("test/popp/entityStatement.jwt")};
    std::string jwks{ResourceManager::instance().getStringResource("test/popp/jwks.jwt")};
    std::unique_ptr<JWT> entityStatementJwt;
    std::unique_ptr<JWT> jwksJwt;

    // GEMREQ-start A_28731#invalid_oid
    void prepareInvalidCertificate(X509Certificate& certificate)
    {
        // Generate an almost valid PoPP token certificate which won't be accepted due to a wrong oid role.
        auto userCertificateWithInvalidOid = ResourceManager::instance().getStringResource(
            "test/generated_pki/sub_ca1_ec/certificates/popp_invalid_oid_zd_sig_ec/popp_invalid_oid_zd_sig_ec.der");
        X509Certificate certificateWithInvalidOid = X509Certificate::createFromAsnBytes(
            {reinterpret_cast<const unsigned char*>(userCertificateWithInvalidOid.data()), userCertificateWithInvalidOid.size()});

        const std::vector<std::string> roles = certificateWithInvalidOid.getRoles();
        ASSERT_FALSE(roles.empty()) << "Certificate has no roles";
        ASSERT_TRUE(std::ranges::none_of(roles, [](const auto& role) {
            return role == profession_oid::oid_popp_token;
        })) << "Certificate unexpectedly contains oid_popp_token";

        certificate = certificateWithInvalidOid;
    }
    // GEMREQ-end A_28731#invalid_oid

    std::unique_ptr<UrlRequestSenderMock> prepareData()
    {
        // Create Entity Statement
        auto entityStatementPayload =
            ResourceManager::instance().getStringResource("test/popp/EntityStatement.payload.json");
        boost::replace_all(entityStatementPayload, "##KID##", "2");
        boost::replace_all(entityStatementPayload, "##X##", Base64::toBase64Url(Base64::encode(mSigningKey.x)));
        boost::replace_all(entityStatementPayload, "##Y##", Base64::toBase64Url(Base64::encode(mSigningKey.y)));
        JoseHeader entityStatementHeader{JoseHeader::Algorithm::ES256};
        entityStatementHeader.setKeyId("1");
        entityStatementHeader.setType("entity-statement+jwt");
        Jws esJws(entityStatementHeader, entityStatementPayload, mSigningKey.key);

        // Create Jwks

        // First popp token signing key is trusted. The coordinates are not used, but must be specified since they are validated
        // and I couldn't find a way to extract them from userCertificate.
        auto userCertificate = ResourceManager::instance().getStringResource(
            "test/generated_pki/sub_ca1_ec/certificates/popp_zd_sig_ec/popp_zd_sig_ec.der");
        X509Certificate certificate = X509Certificate::createFromAsnBytes(
            {reinterpret_cast<const unsigned char*>(userCertificate.data()), userCertificate.size()});

        // GEMREQ-start A_28731#load
        X509Certificate certificateWithInvalidOid;
        prepareInvalidCertificate(certificateWithInvalidOid);
        // GEMREQ-end A_28731#load

        // The PoPP verifier requires EC public key coordinates (x, y) to be present.
        // They are not relevant for validation, but must be included (properly encoded)
        // to avoid rejection by the reader.
        auto pk = shared_EVP_PKEY::make(certificate.getPublicKey());
        const auto coords = EllipticCurveUtils::getPaddedXYComponents(pk, EllipticCurve::KeyCoordinateLength);
        pk.release();
        auto x = Base64::encode(coords.x);
        auto y = Base64::encode(coords.y);

        std::string keys = "";
        auto keyTemplate = ResourceManager::instance().getStringResource("test/popp/Jwks.key.json");
        boost::replace_all(keyTemplate, "##KID##", "9");
        boost::replace_all(keyTemplate, "##X##", x);
        boost::replace_all(keyTemplate, "##Y##", y);
        boost::replace_all(keyTemplate, "##CERT##", certificate.toBase64());
        keys += keyTemplate;

        // GEMREQ-start A_28731#append
        {
            auto keyTemplate = ResourceManager::instance().getStringResource("test/popp/Jwks.key.json");
            boost::replace_all(keyTemplate, "##KID##", "9.1");
            boost::replace_all(keyTemplate, "##X##", x);
            boost::replace_all(keyTemplate, "##Y##", y);
            boost::replace_all(keyTemplate, "##CERT##", certificateWithInvalidOid.toBase64());
            keys += ", " + keyTemplate;
        }
        // GEMREQ-end A_28731#append

        // These certificates are copied from the jwks payload, provided for testing purposes.
        // All of them are NOT trusted.
        std::array<std::string, 5> certs{
            // clang-format off
        "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"
            // clang-format on
        };
        for (size_t i = 0; i < certs.size(); ++i)
        {
            auto keyTemplate = ResourceManager::instance().getStringResource("test/popp/Jwks.key.json");
            boost::replace_all(keyTemplate, "##KID##", std::to_string(10 + i));
            boost::replace_all(keyTemplate, "##X##", x);
            boost::replace_all(keyTemplate, "##Y##", y);
            boost::replace_all(keyTemplate, "##CERT##", certs[i]);
            keys += "," + keyTemplate;
        }

        auto jwksPayload = ResourceManager::instance().getStringResource("test/popp/Jwks.payload.json");
        boost::replace_all(jwksPayload, "##KEYS##", keys);
        JoseHeader jwksHeader{JoseHeader::Algorithm::ES256};
        jwksHeader.setKeyId("2");
        jwksHeader.setType("JWT");
        Jws jwksJws(jwksHeader, jwksPayload, mSigningKey.key);
        const auto entityStatementUrl = "https://localhost:4443/entityStatement";
        EnvironmentVariableGuard guard("ERP_POPP_ENTITY_STATEMENT_URL", entityStatementUrl);
        auto senderMock = std::make_unique<UrlRequestSenderMock>(
            TlsCertificateVerifierNoVerificationImplementation::withVerificationDisabledForTesting(),
            std::chrono::seconds{
                Configuration::instance().getIntValue(ConfigurationKey::HTTPCLIENT_CONNECT_TIMEOUT_SECONDS)},
            std::chrono::milliseconds{
                Configuration::instance().getIntValue(ConfigurationKey::HTTPCLIENT_RESOLVE_TIMEOUT_MILLISECONDS)});
        senderMock->setUrlHandler(entityStatementUrl, [esJws](const std::string&) -> ClientResponse {
            return ClientResponse{Header{HttpStatus::OK}, esJws.compactSerialized()};
        });
        senderMock->setUrlHandler(jwksUri, [jwksJws](const std::string&) -> ClientResponse {
            return ClientResponse{Header{HttpStatus::OK}, jwksJws.compactSerialized()};
        });
        return senderMock;
    }

    std::shared_ptr<TslManager> createAndSetupTslManager(bool withRevoked)
    {
        auto userCertificate = ResourceManager::instance().getStringResource(
            "test/generated_pki/sub_ca1_ec/certificates/popp_zd_sig_ec/popp_zd_sig_ec.der");
        X509Certificate certificate = X509Certificate::createFromAsnBytes(
            {reinterpret_cast<const unsigned char*>(userCertificate.data()), userCertificate.size()});

        mTrustStore = TslTestHelper::createTslTrustStore();

        auto iterator = mTrustStore->mServiceInformationMap.find(
            {certificate.getIssuer(), certificate.getAuthorityKeyIdentifier()});
        EXPECT_NE(mTrustStore->mServiceInformationMap.end(), iterator);

        std::map<std::string, const std::vector<MockOcsp::CertificatePair>> ocspResponderKnownCertificateCaPairs = {
            {"http://ocsp-testref.tsl.telematik-test/ocsp",
             {
                 {Certificate::fromBinaryDer(userCertificate),
                  Certificate::fromBase64Der(iterator->second.certificate.toBase64()),
                  withRevoked ? MockOcsp::CertificateOcspTestMode::REVOKED : MockOcsp::CertificateOcspTestMode::SUCCESS}
             }}};

        return TslTestHelper::createTslManager<TslManager>({}, {}, ocspResponderKnownCertificateCaPairs);
    }

private:
    Key genKey()
    {
        auto hexToBytes = [](const std::string& hex) -> std::vector<uint8_t> {
            std::vector<uint8_t> out(hex.size() / 2);
            OPENSSL_hexstr2buf_ex(out.data(), out.size(), nullptr, hex.c_str(), '\0');// ':');
            return out;
        };
        // Create private key
        auto group = std::unique_ptr<EC_GROUP, decltype(&EC_GROUP_free)>{
            EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1), &EC_GROUP_free};
        BIGNUM* priv = BN_new();
        const BIGNUM* order = EC_GROUP_get0_order(group.get());
        BN_rand_range(priv, order);
        if (BN_is_zero(priv))
        {
            BN_one(priv);
        }
        char* hex = BN_bn2hex(priv);
        std::string privHex(hex);
        OPENSSL_free(hex);
        BN_free(priv);
        const auto privateKey = EllipticCurveUtils::createPrivateKeyHex(privHex, NID_X9_62_prime256v1);
        const auto [x, y] = EllipticCurveUtils::getPublicKeyCoordinatesHex(privateKey);
        std::vector<uint8_t> xBytes = hexToBytes(x);
        std::string xBin(xBytes.begin(), xBytes.end());
        std::vector<uint8_t> yBytes = hexToBytes(y);
        std::string yBin(yBytes.begin(), yBytes.end());
        return {xBin, yBin, privateKey};
    }

    std::unique_ptr<TrustStore> mTrustStore;
    std::optional<Certificate> mIdpCertificate;
};

TEST_F(PoPPTest, testFirstJwt_Structural)
{
    const auto& doc = entityStatementJwt->claimsDocument();
    EXPECT_TRUE(doc.IsObject());
    EXPECT_TRUE(doc.HasMember("metadata"));

    const auto& metadata = doc["metadata"];
    EXPECT_TRUE(metadata.IsObject());
    EXPECT_TRUE(metadata.HasMember("oauth_resource"));

    const auto& res = metadata["oauth_resource"];
    EXPECT_TRUE(res.IsObject());
    EXPECT_TRUE(res.HasMember("signed_jwks_uri"));

    const auto& s = res["signed_jwks_uri"];
    EXPECT_TRUE(s.IsString());
    EXPECT_STREQ(s.GetString(), jwksUri.c_str());
}

TEST_F(PoPPTest, testFirstJwt)
{
    std::string s{};
    const PoPPCertificateVerifier::EntityStatement es(*entityStatementJwt);
    EXPECT_NO_THROW(s = es.getJwksUri());
    EXPECT_STREQ(s.c_str(), jwksUri.c_str());
}

// GEMREQ-start A_26534#test_success
TEST_F(PoPPTest, downloadEntityStatementAndCorruptJwksFail)
{
    const auto entityStatementUrl = "https://localhost:4443/entityStatement";
    EnvironmentVariableGuard guard("ERP_POPP_ENTITY_STATEMENT_URL", entityStatementUrl);

    auto senderMock = std::make_unique<UrlRequestSenderMock>(
        TlsCertificateVerifierNoVerificationImplementation::withVerificationDisabledForTesting(),
        std::chrono::seconds{
            Configuration::instance().getIntValue(ConfigurationKey::HTTPCLIENT_CONNECT_TIMEOUT_SECONDS)},
        std::chrono::milliseconds{
            Configuration::instance().getIntValue(ConfigurationKey::HTTPCLIENT_RESOLVE_TIMEOUT_MILLISECONDS)});

    size_t numberOfChecks1 = 0;
    size_t numberOfChecks2 = 0;

    senderMock->setUrlHandler(entityStatementUrl, [this, &numberOfChecks1](const std::string&) -> ClientResponse {
        numberOfChecks1++;
        return ClientResponse{Header{HttpStatus::OK}, entityStatement};
    });

    senderMock->setUrlHandler(jwksUri, [this, &numberOfChecks2](const std::string&) -> ClientResponse {
        numberOfChecks2++;
        auto parts = String::split(jwks, ".");

        JoseHeader jwksHeader{JoseHeader::Algorithm::ES256};
        jwksHeader.setKeyId("2");
        jwksHeader.setType("JWT");
        Jws jwksJws(jwksHeader, parts[1], mSigningKey.key);
        auto parts2 = String::split(jwksJws.compactSerialized(), ".");

        std::string corruptJwks = parts[0] + "." + parts[1] + "." + parts2[2];
        return ClientResponse{Header{HttpStatus::OK}, corruptJwks};
    });

    ThreadPool threadPool;

    auto& ioContext = threadPool.ioContext();
    threadPool.setUp(1, "PoPPCertificateVerifierThread");

    testing::internal::CaptureStderr();
    PoPPCertificateVerifierService service(ioContext, std::move(senderMock), *mTslManager);

    service.start(std::chrono::milliseconds(250), std::chrono::milliseconds(750), 15, 30);

    std::this_thread::sleep_for(std::chrono::milliseconds(1250));

    threadPool.shutDown();

    // At least one response from each downloaded expected.
    EXPECT_GE(numberOfChecks1, 1);
    EXPECT_GE(numberOfChecks2, 1);

    std::string output = testing::internal::GetCapturedStderr();
    std::cout << output << std::endl;
    EXPECT_TRUE(output.find("Verification failed - invalid signature or payload.") != std::string::npos) << output;
}
// GEMREQ-end A_26534#test_success

TEST_F(PoPPTest, checkJwtsSuccess)
{
    EXPECT_NO_THROW({
        PoPPCertificateVerifier::EntityStatement esDoc(*entityStatementJwt);

        PoPPCertificateVerifier::Jwks jwksDoc(*jwksJwt);

        auto jwk = esDoc.validateKeys();

        auto publicKey = esDoc.calculatePublicKey(jwk);

        EXPECT_NO_THROW(esDoc.verifySignature(publicKey));

        EXPECT_NO_THROW(jwksDoc.verifySignature(publicKey));
    });
}

TEST_F(PoPPTest, downloadFailedEntityStatement)
{
    const auto entityStatementUrl = "https://localhost:4443/entityStatement";
    EnvironmentVariableGuard guard("ERP_POPP_ENTITY_STATEMENT_URL", entityStatementUrl);

    auto senderMock = std::make_unique<UrlRequestSenderMock>(
        TlsCertificateVerifierNoVerificationImplementation::withVerificationDisabledForTesting(),
        std::chrono::seconds{
            Configuration::instance().getIntValue(ConfigurationKey::HTTPCLIENT_CONNECT_TIMEOUT_SECONDS)},
        std::chrono::milliseconds{
            Configuration::instance().getIntValue(ConfigurationKey::HTTPCLIENT_RESOLVE_TIMEOUT_MILLISECONDS)});

    size_t numberOfChecks1 = 0;
    size_t numberOfChecks2 = 0;

    senderMock->setUrlHandler(entityStatementUrl, [this, &numberOfChecks1](const std::string&) -> ClientResponse {
        numberOfChecks1++;
        if (numberOfChecks1 > 4){
            return ClientResponse{Header{HttpStatus::OK}, entityStatement};
        }
        return ClientResponse{Header{HttpStatus::NotFound}, ""};
    });

    senderMock->setUrlHandler(jwksUri, [this, &numberOfChecks2](const std::string&) -> ClientResponse {
        numberOfChecks2++;
        return ClientResponse{Header{HttpStatus::OK}, jwks};
    });

    ThreadPool threadPool;

    auto& ioContext = threadPool.ioContext();
    threadPool.setUp(1, "PoPPCertificateVerifierThread");

    PoPPCertificateVerifierService service(ioContext, std::move(senderMock), *mTslManager);

    // 0                    1                           2                    3                    4                    5                    6          X                          [s]
    // 250 .. 250 .. 250 .. 250 .. 250 .. 250 .. 250 .. 250 .. 250 .. 250 .. 250 .. 250 .. 250 .. 250 .. 250 .. 250 .. 250 .. 250 .. 250 .. 250 .. 250 .. 250 .. 250 .. 250       [ms]
    // |             |      500 ..        500 ..        500 ..        500 ..        500 .. |      500 ..        500 ..        500 ..        500 ..        500 ..        500 ..    [ms]
    // |      |      |      |      |                                                       |                                                       |
    // |      |      |      |      |                                                       |                                                       |
    // |      |      |      |      |                                                       |                                                       |
    // <--   error DL ES  -->      |                                                       |                                                       |
    //                             |                                                       |                                                       |
    //                             |                                                       |                                                       |
    //                             ES download                                             |                                                       |
    //                                                                                     |                                                       |
    //                                                                                     ES update                                               |
    //                                                                                                                                             |
    //                                                                                                                                             |
    //                                                                                                                                             ES update

    // Downloader starts  with 2  s interval, switches immediately to 250 ms interval.
    // After 4 failed attempts, the fifth works and it switches back to 2 s interval checks. Downloads the jwks
    // Now jwks doc is downloadded every 500 ms.

    service.start(std::chrono::milliseconds(500), std::chrono::milliseconds(2000), 15, 30,
                  std::chrono::milliseconds(250));

    std::this_thread::sleep_for(std::chrono::milliseconds(6300));

    EXPECT_GE(numberOfChecks1, 5);
    EXPECT_GT(numberOfChecks2, 5);
    threadPool.shutDown();
}

TEST_F(PoPPTest, cacheAccess)
{
    const auto entityStatementUrl = "https://localhost:4443/entityStatement";
    EnvironmentVariableGuard guard("ERP_POPP_ENTITY_STATEMENT_URL", entityStatementUrl);

    auto senderMock = std::make_unique<UrlRequestSenderMock>(
        TlsCertificateVerifierNoVerificationImplementation::withVerificationDisabledForTesting(),
        std::chrono::seconds{
            Configuration::instance().getIntValue(ConfigurationKey::HTTPCLIENT_CONNECT_TIMEOUT_SECONDS)},
        std::chrono::milliseconds{
            Configuration::instance().getIntValue(ConfigurationKey::HTTPCLIENT_RESOLVE_TIMEOUT_MILLISECONDS)});

    senderMock->setUrlHandler(entityStatementUrl, [this](const std::string&) -> ClientResponse {
        return ClientResponse{Header{HttpStatus::OK}, entityStatement};
    });

    senderMock->setUrlHandler(jwksUri, [this](const std::string&) -> ClientResponse {
        return ClientResponse{Header{HttpStatus::OK}, jwks};
    });

    ThreadPool threadPool;

    auto& ioContext = threadPool.ioContext();
    threadPool.setUp(1, "PoPPCertificateVerifierThread");

    PoPPCertificateVerifierService service(ioContext, std::move(senderMock), *mTslManager);

    service.start(std::chrono::milliseconds(250), std::chrono::milliseconds(750), 15, 30);

    std::this_thread::sleep_for(std::chrono::milliseconds(1250));

    // 601857134404305878027055269357303334835889092329
    // 23937256187821930331210572555161848889
    // 18677040925714219938770134872662775880
    // 218058879878812571680388163091216921702

    EXPECT_FALSE(service.getKey(""));

    std::optional<PoPPCertificateVerifier::PoPPCertificateCache::Key> key =
        service.getKey("601857134404305878027055269357303334835889092329");
    EXPECT_FALSE(key.has_value());

    std::this_thread::sleep_for(std::chrono::milliseconds(1250));

    threadPool.shutDown();
}

// GEMREQ-start A_27016#valid_cert
// GEMREQ-start A_28731#test
TEST_F(PoPPTest, jwksCertificatesCheck)
{
    std::unique_ptr<UrlRequestSenderMock> senderMock;
    ASSERT_NO_FATAL_FAILURE(senderMock = prepareData());

    ThreadPool threadPool;
    auto& ioContext = threadPool.ioContext();
    threadPool.setUp(1, "PoPPCertificateVerifierThread");
    PoPPCertificateVerifierService service(ioContext, std::move(senderMock), *mTslManager);
    service.start(std::chrono::milliseconds(250), std::chrono::milliseconds(750), 15, 30);

    std::this_thread::sleep_for(std::chrono::milliseconds(1250));

    EXPECT_TRUE(service.getKey("9").has_value());
    EXPECT_FALSE(service.getKey("9.1").has_value());
// GEMREQ-end A_28731#test
    EXPECT_FALSE(service.getKey("10").has_value());
    EXPECT_FALSE(service.getKey("11").has_value());
    EXPECT_FALSE(service.getKey("12").has_value());
    EXPECT_FALSE(service.getKey("13").has_value());
    EXPECT_FALSE(service.getKey("14").has_value());

    ASSERT_NO_THROW(service.healthCheck());
    const auto healthData = service.getHealthData();
    ASSERT_EQ(healthData.size(), 7);

    auto key = service.getKey("9");
    EXPECT_TRUE(key.has_value());
    EXPECT_TRUE(key->certificate);
    // Compare calculated public key with the public key from the certificate, these must match.
    EVP_PKEY* k1 = key->certificate->getPublicKey();
    EXPECT_TRUE(k1);
    shared_EVP_PKEY k2 = shared_EVP_PKEY::make(k1);
    EXPECT_TRUE(k2);
    EXPECT_STREQ(EllipticCurveUtils::publicKeyToPem(k2).c_str(),
                 EllipticCurveUtils::publicKeyToPem(key->publicKey).c_str());
    k2.release();

    threadPool.shutDown();
}
// GEMREQ-end A_27016#valid_cert

// GEMREQ-start A_27016#revoked_cert
TEST_F(PoPPTest, jwksCertificatesCheckRevoked)
{
    mTslManager = createAndSetupTslManager(true);

    auto senderMock = prepareData();

    ThreadPool threadPool;
    auto& ioContext = threadPool.ioContext();
    threadPool.setUp(1, "PoPPCertificateVerifierThread");
    PoPPCertificateVerifierService service(ioContext, std::move(senderMock), *mTslManager);
    service.start(std::chrono::milliseconds(250), std::chrono::milliseconds(750), 15, 30);

    std::this_thread::sleep_for(std::chrono::milliseconds(1250));

    // This is revoked and therefore removed from the cache / not available.
    EXPECT_FALSE(service.getKey("9").has_value());

    EXPECT_FALSE(service.getKey("10").has_value());
    EXPECT_FALSE(service.getKey("11").has_value());
    EXPECT_FALSE(service.getKey("12").has_value());
    EXPECT_FALSE(service.getKey("13").has_value());
    EXPECT_FALSE(service.getKey("14").has_value());

    ASSERT_THROW(service.healthCheck(), std::runtime_error);

    threadPool.shutDown();
}
// GEMREQ-end A_27016#revoked_cert
