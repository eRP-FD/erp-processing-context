/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include <gtest/gtest.h>// should be first or FRIEND_TEST would not work

#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/crypto/Certificate.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/tsl/OcspHelper.hxx"
#include "erp/tsl/OcspService.hxx"
#include "erp/tsl/TslManager.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Hash.hxx"
#include "erp/util/SafeString.hxx"
#include "test/erp/pc/CFdSigErpTestHelper.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "mock/tsl/UrlRequestSenderMock.hxx"
#include "test/util/CertificateDirLoader.h"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestConfiguration.hxx"

#include "test_config.h"
#include "test/util/ResourceManager.hxx"

using namespace ::std::string_view_literals;

class CadesBesSignatureTest : public ::testing::Test
{
public:
    static constexpr char certificate[] =
        "-----BEGIN CERTIFICATE-----\n"
        "MIICLjCCAdSgAwIBAgIUAL2a72Us3iEdkk6+UA3nIS4RQ4kwCgYIKoZIzj0EAwQw\n"
        "bDELMAkGA1UEBhMCREUxEDAOBgNVBAgMB0hhbWJ1cmcxEDAOBgNVBAcMB0hhbWJ1\n"
        "cmcxHTAbBgNVBAoMFElCTSBEZXV0c2NobGFuZCBHbWJIMQwwCgYDVQQLDANFUlAx\n"
        "DDAKBgNVBAMMA1ZBVTAeFw0yMDEyMTcxMTQ0MjZaFw0yMTEyMTcxMTQ0MjZaMGwx\n"
        "CzAJBgNVBAYTAkRFMRAwDgYDVQQIDAdIYW1idXJnMRAwDgYDVQQHDAdIYW1idXJn\n"
        "MR0wGwYDVQQKDBRJQk0gRGV1dHNjaGxhbmQgR21iSDEMMAoGA1UECwwDRVJQMQww\n"
        "CgYDVQQDDANWQVUwWjAUBgcqhkjOPQIBBgkrJAMDAggBAQcDQgAEhjQhKDDa1FfK\n"
        "BTBeZocTQWa5whpl/+v1VfTnXfsEiIhm5LaENiTL2kPJfqiZaLxB/VNXb4LAPvp9\n"
        "YBufrKwrKaNTMFEwHQYDVR0OBBYEFCHwxRVjmkkfxzQ79FWkM49QgxcuMB8GA1Ud\n"
        "IwQYMBaAFCHwxRVjmkkfxzQ79FWkM49QgxcuMA8GA1UdEwEB/wQFMAMBAf8wCgYI\n"
        "KoZIzj0EAwQDSAAwRQIgQMr3w4RL0X7CbwAx2Y9yHrPAMsDdzd20KIOrDOP4ow8C\n"
        "IQCHgwCMbPC5uP2PSa9DsS4ZtvECVo81HCIBMlMAYQ8Pqg==\n"
        "-----END CERTIFICATE-----\n";

    static constexpr char privateKey[] =
        "-----BEGIN EC PRIVATE KEY-----\n"
        "MHgCAQEEIKVKVoW4D3H9Xr7pFlmvqYyEfFyGTiM1hEFGZ1r8WV48oAsGCSskAwMC\n"
        "CAEBB6FEA0IABIY0ISgw2tRXygUwXmaHE0FmucIaZf/r9VX05137BIiIZuS2hDYk\n"
        "y9pDyX6omWi8Qf1TV2+CwD76fWAbn6ysKyk=\n"
        "-----END EC PRIVATE KEY-----\n";

    std::unique_ptr<EnvironmentVariableGuard> mCaDerPathGuard;

    void SetUp() override
    {
        mCaDerPathGuard = std::make_unique<EnvironmentVariableGuard>(
            "ERP_TSL_INITIAL_CA_DER_PATH",
            ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/ca.der"));
    }

    void TearDown() override
    {
        mCaDerPathGuard.reset();
    }
};


TEST_F(CadesBesSignatureTest, roundtrip)
{
    std::string_view myText = "The text to be signed";
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{CFdSigErpTestHelper::cFdSigErpPrivateKey()});
    auto cert = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErp());
    std::string signedText;
    {
        CadesBesSignature cms{cert, privKey, std::string{myText}};
        ASSERT_NO_THROW(signedText = Base64::encode(cms.get()));
    }
    {
        const auto& testConfig = TestConfiguration::instance();
        const auto& cadesBesTrustedCertDir =
            testConfig.getOptionalStringValue(TestConfigurationKey::TEST_CADESBES_TRUSTED_CERT_DIR, "test/cadesBesSignature/certificates");
        auto certs = CertificateDirLoader::loadDir(cadesBesTrustedCertDir);
        CadesBesSignature cms{certs, signedText};
        EXPECT_EQ(cms.payload(), myText);
    }
}


TEST_F(CadesBesSignatureTest, DISABLED_GematikExampleFile)
{
    const auto raw = FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/cadesBesSignature/KBV-FHIR-Verordnung.xml.p7s");
    // the CadesBesSignature class expects the input encoded in base64
    CadesBesSignature cadesBesSignature(Base64::encode(raw));

    ASSERT_FALSE(cadesBesSignature.payload().empty());
}


TEST_F(CadesBesSignatureTest, KBVTestingExampleFile)
{
    const auto raw = FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/cadesBesSignature/KBV-from-testing.p7s");
    CadesBesSignature cadesBesSignature(raw);

    ASSERT_FALSE(cadesBesSignature.payload().empty());
}


TEST_F(CadesBesSignatureTest, roundtripWithoutTsl)
{
    std::string_view myText = "The text to be signed";
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{privateKey});
    auto cert = Certificate::fromPem(certificate);
    std::string signedText;
    {
        CadesBesSignature cadesBesSignature{cert, privKey, std::string{myText}};
        ASSERT_NO_THROW(signedText = Base64::encode(cadesBesSignature.get()));
    }
    {
        CadesBesSignature cadesBesSignature{signedText};
        EXPECT_EQ(cadesBesSignature.payload(), myText);
    }
}


TEST_F(CadesBesSignatureTest, getSigningTime)
{
    // Extracted manually from KBV-from-testing.p7s
    model::Timestamp referenceTime = model::Timestamp::fromXsDateTime("2021-02-25T10:04:04Z");

    CadesBesSignature cadesBesSignature(
        FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/cadesBesSignature/KBV-from-testing.p7s"));
    auto signingTime = cadesBesSignature.getSigningTime();
    ASSERT_TRUE(signingTime.has_value());
    ASSERT_EQ(signingTime.value(), referenceTime);
}


TEST_F(CadesBesSignatureTest, getMessageDigest)
{
    // Extracted manually from kbv_bundle.p7s
    const auto referenceDigest = "ZDYUYul4VgUZHdmpy1hxS57lwRTw3ayuW8qfaMN90Fo="sv;

    CadesBesSignature cadesBesSignature(
        Base64::encode(FileHelper::readFileAsString(::std::string(TEST_DATA_DIR) + "/issues/ERP-5822/kbv_bundle.p7s")));
    const auto messageDigest = cadesBesSignature.getMessageDigest();
    ASSERT_TRUE(messageDigest.has_value());
    EXPECT_EQ(Base64::encode(messageDigest.value()), referenceDigest);
}


TEST_F(CadesBesSignatureTest, setSigningTime)//NOLINT(readability-function-cognitive-complexity)
{
    std::string_view myText = "The text to be signed";
    auto signingTime = model::Timestamp::fromXsDateTime("2019-02-25T08:05:05Z");
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{privateKey});
    auto cert = Certificate::fromPem(certificate);
    std::string signedText;
    {
        CadesBesSignature cadesBesSignature{cert, privKey, std::string{myText}, signingTime};
        ASSERT_NO_THROW(signedText = Base64::encode(cadesBesSignature.get()));
    }
    {
        CadesBesSignature cadesBesSignature{signedText};
        EXPECT_EQ(cadesBesSignature.payload(), myText);
        auto signingTime1 = cadesBesSignature.getSigningTime();
        ASSERT_TRUE(signingTime1.has_value());
        ASSERT_EQ(signingTime1.value(), signingTime1);
    }
}


TEST_F(CadesBesSignatureTest, validateWithBna)
{

    std::string_view myText = "The text to be signed";
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.prv.pem")});

    auto cert = Certificate::fromPem(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem"));

    auto certCA = Certificate::fromBase64Der(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ehca-testref.sig-test.telematik-test:8080/status/qocsp",
             {{cert, certCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    std::string signedText;
    {
        CadesBesSignature cadesBesSignature{ cert, privKey, std::string{myText}};
        ASSERT_NO_THROW(signedText = Base64::encode(cadesBesSignature.get()));
    }
    {
        CadesBesSignature cadesBesSignature{signedText, *manager};
        EXPECT_EQ(cadesBesSignature.payload(), myText);
    }
}


TEST_F(CadesBesSignatureTest, GematikExampleWithOcsp)
{
    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>();

    // To accept old OCSP-Response the OCSP grace period is set to ca 50 years
    EnvironmentVariableGuard environmentGuard("ERP_OCSP_QES_GRACE_PERIOD", "1576800000");
    const auto raw =
        FileHelper::readFileAsString(
            std::string(TEST_DATA_DIR)
            + "/cadesBesSignature/4fe2013d-ae94-441a-a1b1-78236ae65680_S_SECUN_secu_kon_4.8.2_4.1.3.p7s");
    CadesBesSignature cadesBesSignature(raw, *manager);

    ASSERT_FALSE(cadesBesSignature.payload().empty());
}


TEST_F(CadesBesSignatureTest, GematikExampleWithOutdatedOcsp)//NOLINT(readability-function-cognitive-complexity)
{
    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>();

    // The old OCSP-Response is outdated, and thus an OCSP-Request should be send.
    // The test framework is not configured to support the OCSP-Request that should produce OCSP_NOT_AVAILABLE error.
    const auto raw =
        FileHelper::readFileAsString(
            std::string(TEST_DATA_DIR)
            + "/cadesBesSignature/4fe2013d-ae94-441a-a1b1-78236ae65680_S_SECUN_secu_kon_4.8.2_4.1.3.p7s");
    EXPECT_TSL_ERROR_THROW(CadesBesSignature(raw, *manager),
               {TslErrorCode::OCSP_NOT_AVAILABLE},
               HttpStatus::InternalServerError);
}


TEST_F(CadesBesSignatureTest, validateGematik)
{
    // TODO: The two certificates below, QES-noType.base64.der and QES-noTypeCA.base64.der
    //       are kind-of secret and must not made public together with the source code.
    //       Hopefully we can obtain non-sensitive replacements from Achelos. If that
    //       is not possible then remove the certificates and also this test before making
    //       the source code publicly accessible.
    auto cert = Certificate::fromBase64Der(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/QES-noType.base64.der"));

    auto certCA = Certificate::fromBase64Der(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/QES-noTypeCA.base64.der"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://qocsp-eA.medisign.de:8080/ocsp",
                {{cert, certCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    std::string signedText = "MIJF2QYJKoZIhvcNAQcCoIJFyjCCRcYCAQUxDzANBglghkgBZQMEAgEFADCCNWQGCSqGSIb3DQEHAaCCNVUEgjVRPEJ1bmRsZSB4bWxucz0iaHR0cDovL2hsNy5vcmcvZmhpciI+DQogIDxpZCB2YWx1ZT0iNmEyMDM4MDUtZTA2OS00MzU3LWE4ZGUtMzBjNmM1M2FhMDg2IiAvPg0KICA8bWV0YT4NCiAgICA8bGFzdFVwZGF0ZWQgdmFsdWU9IjIwMjEtMDYtMTdUMTQ6MjY6NTYuMTEwMDMxKzAyOjAwIiAvPg0KICAgIDxwcm9maWxlIHZhbHVlPSJodHRwczovL2ZoaXIua2J2LmRlL1N0cnVjdHVyZURlZmluaXRpb24vS0JWX1BSX0VSUF9CdW5kbGV8MS4wLjEiIC8+DQogIDwvbWV0YT4NCiAgPGlkZW50aWZpZXI+DQogICAgPHN5c3RlbSB2YWx1ZT0iaHR0cHM6Ly9nZW1hdGlrLmRlL2ZoaXIvTmFtaW5nU3lzdGVtL1ByZXNjcmlwdGlvbklEIiAvPg0KICAgIDx2YWx1ZSB2YWx1ZT0iMTYwLjAwMC4wMDAuMDAwLjAwNC40NSIgLz4NCiAgPC9pZGVudGlmaWVyPg0KICA8dHlwZSB2YWx1ZT0iZG9jdW1lbnQiIC8+DQogIDx0aW1lc3RhbXAgdmFsdWU9IjIwMjEtMDYtMTdUMTQ6MjY6NTYuMTEwMDMxKzAyOjAwIiAvPg0KICA8ZW50cnk+DQogICAgPGZ1bGxVcmwgdmFsdWU9Imh0dHA6Ly9sb2NhbGhvc3Q6NDc5OS9maGlyL0NvbXBvc2l0aW9uL2MyNzk4ZWNjLWZhYTItNDc4OC05YTlhLWZjZmM5Mjk4NGZjZSIgLz4NCiAgICA8cmVzb3VyY2U+DQogICAgICA8Q29tcG9zaXRpb24+DQogICAgICAgIDxpZCB2YWx1ZT0iYzI3OThlY2MtZmFhMi00Nzg4LTlhOWEtZmNmYzkyOTg0ZmNlIiAvPg0KICAgICAgICA8bWV0YT4NCiAgICAgICAgICA8cHJvZmlsZSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9TdHJ1Y3R1cmVEZWZpbml0aW9uL0tCVl9QUl9FUlBfQ29tcG9zaXRpb258MS4wLjEiIC8+DQogICAgICAgIDwvbWV0YT4NCiAgICAgICAgPGV4dGVuc2lvbiB1cmw9Imh0dHBzOi8vZmhpci5rYnYuZGUvU3RydWN0dXJlRGVmaW5pdGlvbi9LQlZfRVhfRk9SX0xlZ2FsX2Jhc2lzIj4NCiAgICAgICAgICA8dmFsdWVDb2Rpbmc+DQogICAgICAgICAgICA8c3lzdGVtIHZhbHVlPSJodHRwczovL2ZoaXIua2J2LmRlL0NvZGVTeXN0ZW0vS0JWX0NTX1NGSElSX0tCVl9TVEFUVVNLRU5OWkVJQ0hFTiIgLz4NCiAgICAgICAgICAgIDxjb2RlIHZhbHVlPSIwMCIgLz4NCiAgICAgICAgICA8L3ZhbHVlQ29kaW5nPg0KICAgICAgICA8L2V4dGVuc2lvbj4NCiAgICAgICAgPHN0YXR1cyB2YWx1ZT0iZmluYWwiIC8+DQogICAgICAgIDx0eXBlPg0KICAgICAgICAgIDxjb2Rpbmc+DQogICAgICAgICAgICA8c3lzdGVtIHZhbHVlPSJodHRwczovL2ZoaXIua2J2LmRlL0NvZGVTeXN0ZW0vS0JWX0NTX1NGSElSX0tCVl9GT1JNVUxBUl9BUlQiIC8+DQogICAgICAgICAgICA8Y29kZSB2YWx1ZT0iZTE2QSIgLz4NCiAgICAgICAgICA8L2NvZGluZz4NCiAgICAgICAgPC90eXBlPg0KICAgICAgICA8c3ViamVjdD4NCiAgICAgICAgICA8cmVmZXJlbmNlIHZhbHVlPSJQYXRpZW50LzNiZTk4Njg1LThhYmEtNGE3Ni1hOTMwLWYxMTlhZDMwYWU2ZiIgLz4NCiAgICAgICAgPC9zdWJqZWN0Pg0KICAgICAgICA8ZGF0ZSB2YWx1ZT0iMjAyMS0wNi0xN1QwMjoyNjo1NloiIC8+DQogICAgICAgIDxhdXRob3I+DQogICAgICAgICAgPHJlZmVyZW5jZSB2YWx1ZT0iUHJhY3RpdGlvbmVyL2ZhM2U0MzIwLTBiYTMtNDVlNy1iYzlmLTkxNDU1MTYxYjZiYSIgLz4NCiAgICAgICAgICA8dHlwZSB2YWx1ZT0iUHJhY3RpdGlvbmVyIiAvPg0KICAgICAgICA8L2F1dGhvcj4NCiAgICAgICAgPGF1dGhvcj4NCiAgICAgICAgICA8dHlwZSB2YWx1ZT0iRGV2aWNlIiAvPg0KICAgICAgICAgIDxpZGVudGlmaWVyPg0KICAgICAgICAgICAgPHN5c3RlbSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9OYW1pbmdTeXN0ZW0vS0JWX05TX0ZPUl9QcnVlZm51bW1lciIgLz4NCiAgICAgICAgICAgIDx2YWx1ZSB2YWx1ZT0iWS8xLzE4MDcvMzYvMjQ0IiAvPg0KICAgICAgICAgIDwvaWRlbnRpZmllcj4NCiAgICAgICAgPC9hdXRob3I+DQogICAgICAgIDx0aXRsZSB2YWx1ZT0iZWxla3Ryb25pc2NoZSBBcnpuZWltaXR0ZWx2ZXJvcmRudW5nIiAvPg0KICAgICAgICA8Y3VzdG9kaWFuPg0KICAgICAgICAgIDxyZWZlcmVuY2UgdmFsdWU9Ik9yZ2FuaXphdGlvbi8xZjNhYzRjNS00NDFkLTRjNDItYWQ5Zi0yMjY2YTgyNWRmNjgiIC8+DQogICAgICAgIDwvY3VzdG9kaWFuPg0KICAgICAgICA8c2VjdGlvbj4NCiAgICAgICAgICA8Y29kZT4NCiAgICAgICAgICAgIDxjb2Rpbmc+DQogICAgICAgICAgICAgIDxzeXN0ZW0gdmFsdWU9Imh0dHBzOi8vZmhpci5rYnYuZGUvQ29kZVN5c3RlbS9LQlZfQ1NfRVJQX1NlY3Rpb25fVHlwZSIgLz4NCiAgICAgICAgICAgICAgPGNvZGUgdmFsdWU9IlByZXNjcmlwdGlvbiIgLz4NCiAgICAgICAgICAgIDwvY29kaW5nPg0KICAgICAgICAgIDwvY29kZT4NCiAgICAgICAgICA8ZW50cnk+DQogICAgICAgICAgICA8cmVmZXJlbmNlIHZhbHVlPSJNZWRpY2F0aW9uUmVxdWVzdC8zYzhhMzMwMy02MjViLTQyZjMtOGMwMC1jMmJhZWRhOTJkNWEiIC8+DQogICAgICAgICAgPC9lbnRyeT4NCiAgICAgICAgPC9zZWN0aW9uPg0KICAgICAgICA8c2VjdGlvbj4NCiAgICAgICAgICA8Y29kZT4NCiAgICAgICAgICAgIDxjb2Rpbmc+DQogICAgICAgICAgICAgIDxzeXN0ZW0gdmFsdWU9Imh0dHBzOi8vZmhpci5rYnYuZGUvQ29kZVN5c3RlbS9LQlZfQ1NfRVJQX1NlY3Rpb25fVHlwZSIgLz4NCiAgICAgICAgICAgICAgPGNvZGUgdmFsdWU9IkNvdmVyYWdlIiAvPg0KICAgICAgICAgICAgPC9jb2Rpbmc+DQogICAgICAgICAgPC9jb2RlPg0KICAgICAgICAgIDxlbnRyeT4NCiAgICAgICAgICAgIDxyZWZlcmVuY2UgdmFsdWU9IkNvdmVyYWdlLzJjNzRjZDVkLWU2MDgtNGIzOC1hZGJhLWNlMWE5YzUzN2Q1YSIgLz4NCiAgICAgICAgICA8L2VudHJ5Pg0KICAgICAgICA8L3NlY3Rpb24+DQogICAgICA8L0NvbXBvc2l0aW9uPg0KICAgIDwvcmVzb3VyY2U+DQogIDwvZW50cnk+DQogIDxlbnRyeT4NCiAgICA8ZnVsbFVybCB2YWx1ZT0iaHR0cDovL2xvY2FsaG9zdDo0Nzk5L2ZoaXIvTWVkaWNhdGlvbi9mMzZiNGY1NS0yNjcwLTQ5YTgtYWYzOS1hZmY0OTY4NDU4M2IiIC8+DQogICAgPHJlc291cmNlPg0KICAgICAgPE1lZGljYXRpb24+DQogICAgICAgIDxpZCB2YWx1ZT0iZjM2YjRmNTUtMjY3MC00OWE4LWFmMzktYWZmNDk2ODQ1ODNiIiAvPg0KICAgICAgICA8bWV0YT4NCiAgICAgICAgICA8cHJvZmlsZSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9TdHJ1Y3R1cmVEZWZpbml0aW9uL0tCVl9QUl9FUlBfTWVkaWNhdGlvbl9QWk58MS4wLjEiIC8+DQogICAgICAgIDwvbWV0YT4NCiAgICAgICAgPGV4dGVuc2lvbiB1cmw9Imh0dHBzOi8vZmhpci5rYnYuZGUvU3RydWN0dXJlRGVmaW5pdGlvbi9LQlZfRVhfRVJQX01lZGljYXRpb25fQ2F0ZWdvcnkiPg0KICAgICAgICAgIDx2YWx1ZUNvZGluZz4NCiAgICAgICAgICAgIDxzeXN0ZW0gdmFsdWU9Imh0dHBzOi8vZmhpci5rYnYuZGUvQ29kZVN5c3RlbS9LQlZfQ1NfRVJQX01lZGljYXRpb25fQ2F0ZWdvcnkiIC8+DQogICAgICAgICAgICA8Y29kZSB2YWx1ZT0iMDAiIC8+DQogICAgICAgICAgPC92YWx1ZUNvZGluZz4NCiAgICAgICAgPC9leHRlbnNpb24+DQogICAgICAgIDxleHRlbnNpb24gdXJsPSJodHRwczovL2ZoaXIua2J2LmRlL1N0cnVjdHVyZURlZmluaXRpb24vS0JWX0VYX0VSUF9NZWRpY2F0aW9uX1ZhY2NpbmUiPg0KICAgICAgICAgIDx2YWx1ZUJvb2xlYW4gdmFsdWU9ImZhbHNlIiAvPg0KICAgICAgICA8L2V4dGVuc2lvbj4NCiAgICAgICAgPGV4dGVuc2lvbiB1cmw9Imh0dHA6Ly9maGlyLmRlL1N0cnVjdHVyZURlZmluaXRpb24vbm9ybWdyb2Vzc2UiPg0KICAgICAgICAgIDx2YWx1ZUNvZGUgdmFsdWU9Ik4xIiAvPg0KICAgICAgICA8L2V4dGVuc2lvbj4NCiAgICAgICAgPGNvZGU+DQogICAgICAgICAgPGNvZGluZz4NCiAgICAgICAgICAgIDxzeXN0ZW0gdmFsdWU9Imh0dHA6Ly9maGlyLmRlL0NvZGVTeXN0ZW0vaWZhL3B6biIgLz4NCiAgICAgICAgICAgIDxjb2RlIHZhbHVlPSIwNjk2OTczNiIgLz4NCiAgICAgICAgICA8L2NvZGluZz4NCiAgICAgICAgICA8dGV4dCB2YWx1ZT0iTmlmZWRpcGluLXJhdGlvcGhhcm3CriAyMG1nIDMwIFJldGFyZHRibC4gTjEiIC8+DQogICAgICAgIDwvY29kZT4NCiAgICAgICAgPGZvcm0+DQogICAgICAgICAgPGNvZGluZz4NCiAgICAgICAgICAgIDxzeXN0ZW0gdmFsdWU9Imh0dHBzOi8vZmhpci5rYnYuZGUvVmFsdWVTZXQvS0JWX1ZTX1NGSElSX0tCVl9EQVJSRUlDSFVOR1NGT1JNIiAvPg0KICAgICAgICAgICAgPGNvZGUgdmFsdWU9IlJFVCIgLz4NCiAgICAgICAgICA8L2NvZGluZz4NCiAgICAgICAgPC9mb3JtPg0KICAgICAgICA8YW1vdW50Pg0KICAgICAgICAgIDxudW1lcmF0b3I+DQogICAgICAgICAgICA8dmFsdWUgdmFsdWU9IjMwIiAvPg0KICAgICAgICAgICAgPHVuaXQgdmFsdWU9IlJFVCIgLz4NCiAgICAgICAgICAgIDxzeXN0ZW0gdmFsdWU9Imh0dHA6Ly91bml0c29mbWVhc3VyZS5vcmciIC8+DQogICAgICAgICAgICA8Y29kZSB2YWx1ZT0ie1N0dWVja30iIC8+DQogICAgICAgICAgPC9udW1lcmF0b3I+DQogICAgICAgICAgPGRlbm9taW5hdG9yPg0KICAgICAgICAgICAgPHZhbHVlIHZhbHVlPSIxIiAvPg0KICAgICAgICAgIDwvZGVub21pbmF0b3I+DQogICAgICAgIDwvYW1vdW50Pg0KICAgICAgPC9NZWRpY2F0aW9uPg0KICAgIDwvcmVzb3VyY2U+DQogIDwvZW50cnk+DQogIDxlbnRyeT4NCiAgICA8ZnVsbFVybCB2YWx1ZT0iaHR0cDovL2xvY2FsaG9zdDo0Nzk5L2ZoaXIvTWVkaWNhdGlvblJlcXVlc3QvM2M4YTMzMDMtNjI1Yi00MmYzLThjMDAtYzJiYWVkYTkyZDVhIiAvPg0KICAgIDxyZXNvdXJjZT4NCiAgICAgIDxNZWRpY2F0aW9uUmVxdWVzdD4NCiAgICAgICAgPGlkIHZhbHVlPSIzYzhhMzMwMy02MjViLTQyZjMtOGMwMC1jMmJhZWRhOTJkNWEiIC8+DQogICAgICAgIDxtZXRhPg0KICAgICAgICAgIDxwcm9maWxlIHZhbHVlPSJodHRwczovL2ZoaXIua2J2LmRlL1N0cnVjdHVyZURlZmluaXRpb24vS0JWX1BSX0VSUF9QcmVzY3JpcHRpb258MS4wLjEiIC8+DQogICAgICAgIDwvbWV0YT4NCiAgICAgICAgPGV4dGVuc2lvbiB1cmw9Imh0dHBzOi8vZmhpci5rYnYuZGUvU3RydWN0dXJlRGVmaW5pdGlvbi9LQlZfRVhfRVJQX1N0YXR1c0NvUGF5bWVudCI+DQogICAgICAgICAgPHZhbHVlQ29kaW5nPg0KICAgICAgICAgICAgPHN5c3RlbSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9Db2RlU3lzdGVtL0tCVl9DU19FUlBfU3RhdHVzQ29QYXltZW50IiAvPg0KICAgICAgICAgICAgPGNvZGUgdmFsdWU9IjAiIC8+DQogICAgICAgICAgPC92YWx1ZUNvZGluZz4NCiAgICAgICAgPC9leHRlbnNpb24+DQogICAgICAgIDxleHRlbnNpb24gdXJsPSJodHRwczovL2ZoaXIua2J2LmRlL1N0cnVjdHVyZURlZmluaXRpb24vS0JWX0VYX0VSUF9FbWVyZ2VuY3lTZXJ2aWNlc0ZlZSI+DQogICAgICAgICAgPHZhbHVlQm9vbGVhbiB2YWx1ZT0iZmFsc2UiIC8+DQogICAgICAgIDwvZXh0ZW5zaW9uPg0KICAgICAgICA8ZXh0ZW5zaW9uIHVybD0iaHR0cHM6Ly9maGlyLmtidi5kZS9TdHJ1Y3R1cmVEZWZpbml0aW9uL0tCVl9FWF9FUlBfQlZHIj4NCiAgICAgICAgICA8dmFsdWVCb29sZWFuIHZhbHVlPSJmYWxzZSIgLz4NCiAgICAgICAgPC9leHRlbnNpb24+DQogICAgICAgIDxleHRlbnNpb24gdXJsPSJodHRwczovL2ZoaXIua2J2LmRlL1N0cnVjdHVyZURlZmluaXRpb24vS0JWX0VYX0VSUF9NdWx0aXBsZV9QcmVzY3JpcHRpb24iPg0KICAgICAgICAgIDxleHRlbnNpb24gdXJsPSJLZW5uemVpY2hlbiI+DQogICAgICAgICAgICA8dmFsdWVCb29sZWFuIHZhbHVlPSJmYWxzZSIgLz4NCiAgICAgICAgICA8L2V4dGVuc2lvbj4NCiAgICAgICAgPC9leHRlbnNpb24+DQogICAgICAgIDxzdGF0dXMgdmFsdWU9ImFjdGl2ZSIgLz4NCiAgICAgICAgPGludGVudCB2YWx1ZT0ib3JkZXIiIC8+DQogICAgICAgIDxtZWRpY2F0aW9uUmVmZXJlbmNlPg0KICAgICAgICAgIDxyZWZlcmVuY2UgdmFsdWU9Ik1lZGljYXRpb24vZjM2YjRmNTUtMjY3MC00OWE4LWFmMzktYWZmNDk2ODQ1ODNiIiAvPg0KICAgICAgICA8L21lZGljYXRpb25SZWZlcmVuY2U+DQogICAgICAgIDxzdWJqZWN0Pg0KICAgICAgICAgIDxyZWZlcmVuY2UgdmFsdWU9IlBhdGllbnQvM2JlOTg2ODUtOGFiYS00YTc2LWE5MzAtZjExOWFkMzBhZTZmIiAvPg0KICAgICAgICA8L3N1YmplY3Q+DQogICAgICAgIDxhdXRob3JlZE9uIHZhbHVlPSIyMDIxLTA2LTE3IiAvPg0KICAgICAgICA8cmVxdWVzdGVyPg0KICAgICAgICAgIDxyZWZlcmVuY2UgdmFsdWU9IlByYWN0aXRpb25lci9mYTNlNDMyMC0wYmEzLTQ1ZTctYmM5Zi05MTQ1NTE2MWI2YmEiIC8+DQogICAgICAgIDwvcmVxdWVzdGVyPg0KICAgICAgICA8aW5zdXJhbmNlPg0KICAgICAgICAgIDxyZWZlcmVuY2UgdmFsdWU9IkNvdmVyYWdlLzJjNzRjZDVkLWU2MDgtNGIzOC1hZGJhLWNlMWE5YzUzN2Q1YSIgLz4NCiAgICAgICAgPC9pbnN1cmFuY2U+DQogICAgICAgIDxkb3NhZ2VJbnN0cnVjdGlvbj4NCiAgICAgICAgICA8ZXh0ZW5zaW9uIHVybD0iaHR0cHM6Ly9maGlyLmtidi5kZS9TdHJ1Y3R1cmVEZWZpbml0aW9uL0tCVl9FWF9FUlBfRG9zYWdlRmxhZyI+DQogICAgICAgICAgICA8dmFsdWVCb29sZWFuIHZhbHVlPSJmYWxzZSIgLz4NCiAgICAgICAgICA8L2V4dGVuc2lvbj4NCiAgICAgICAgPC9kb3NhZ2VJbnN0cnVjdGlvbj4NCiAgICAgICAgPGRpc3BlbnNlUmVxdWVzdD4NCiAgICAgICAgICA8cXVhbnRpdHk+DQogICAgICAgICAgICA8dmFsdWUgdmFsdWU9IjEiIC8+DQogICAgICAgICAgICA8c3lzdGVtIHZhbHVlPSJodHRwOi8vdW5pdHNvZm1lYXN1cmUub3JnIiAvPg0KICAgICAgICAgICAgPGNvZGUgdmFsdWU9IntQYWNrYWdlfSIgLz4NCiAgICAgICAgICA8L3F1YW50aXR5Pg0KICAgICAgICA8L2Rpc3BlbnNlUmVxdWVzdD4NCiAgICAgICAgPHN1YnN0aXR1dGlvbj4NCiAgICAgICAgICA8YWxsb3dlZEJvb2xlYW4gdmFsdWU9InRydWUiIC8+DQogICAgICAgIDwvc3Vic3RpdHV0aW9uPg0KICAgICAgPC9NZWRpY2F0aW9uUmVxdWVzdD4NCiAgICA8L3Jlc291cmNlPg0KICA8L2VudHJ5Pg0KICA8ZW50cnk+DQogICAgPGZ1bGxVcmwgdmFsdWU9Imh0dHA6Ly9sb2NhbGhvc3Q6NDc5OS9maGlyL1BhdGllbnQvM2JlOTg2ODUtOGFiYS00YTc2LWE5MzAtZjExOWFkMzBhZTZmIiAvPg0KICAgIDxyZXNvdXJjZT4NCiAgICAgIDxQYXRpZW50Pg0KICAgICAgICA8aWQgdmFsdWU9IjNiZTk4Njg1LThhYmEtNGE3Ni1hOTMwLWYxMTlhZDMwYWU2ZiIgLz4NCiAgICAgICAgPG1ldGE+DQogICAgICAgICAgPHByb2ZpbGUgdmFsdWU9Imh0dHBzOi8vZmhpci5rYnYuZGUvU3RydWN0dXJlRGVmaW5pdGlvbi9LQlZfUFJfRk9SX1BhdGllbnR8MS4wLjMiIC8+DQogICAgICAgIDwvbWV0YT4NCiAgICAgICAgPGlkZW50aWZpZXI+DQogICAgICAgICAgPHR5cGU+DQogICAgICAgICAgICA8Y29kaW5nPg0KICAgICAgICAgICAgICA8c3lzdGVtIHZhbHVlPSJodHRwOi8vZmhpci5kZS9Db2RlU3lzdGVtL2lkZW50aWZpZXItdHlwZS1kZS1iYXNpcyIgLz4NCiAgICAgICAgICAgICAgPGNvZGUgdmFsdWU9IkdLViIgLz4NCiAgICAgICAgICAgIDwvY29kaW5nPg0KICAgICAgICAgIDwvdHlwZT4NCiAgICAgICAgICA8c3lzdGVtIHZhbHVlPSJodHRwOi8vZmhpci5kZS9OYW1pbmdTeXN0ZW0vZ2t2L2t2aWQtMTAiIC8+DQogICAgICAgICAgPHZhbHVlIHZhbHVlPSJNMTIzNDU2Nzg5IiAvPg0KICAgICAgICA8L2lkZW50aWZpZXI+DQogICAgICAgIDxuYW1lPg0KICAgICAgICAgIDx1c2UgdmFsdWU9Im9mZmljaWFsIiAvPg0KICAgICAgICAgIDxmYW1pbHkgdmFsdWU9Ik11c3Rlcm1hbm4iPg0KICAgICAgICAgICAgPGV4dGVuc2lvbiB1cmw9Imh0dHA6Ly9obDcub3JnL2ZoaXIvU3RydWN0dXJlRGVmaW5pdGlvbi9odW1hbm5hbWUtb3duLW5hbWUiPg0KICAgICAgICAgICAgICA8dmFsdWVTdHJpbmcgdmFsdWU9Ik11c3Rlcm1hbm4iIC8+DQogICAgICAgICAgICA8L2V4dGVuc2lvbj4NCiAgICAgICAgICA8L2ZhbWlseT4NCiAgICAgICAgICA8Z2l2ZW4gdmFsdWU9IkVyaWthIiAvPg0KICAgICAgICA8L25hbWU+DQogICAgICAgIDxiaXJ0aERhdGUgdmFsdWU9IjE5MDAtMDEtMDEiIC8+DQogICAgICAgIDxhZGRyZXNzPg0KICAgICAgICAgIDx0eXBlIHZhbHVlPSJib3RoIiAvPg0KICAgICAgICAgIDxjaXR5IHZhbHVlPSIiIC8+DQogICAgICAgICAgPHBvc3RhbENvZGUgdmFsdWU9IjAwMDAwIiAvPg0KICAgICAgICAgIDxjb3VudHJ5IHZhbHVlPSJEIiAvPg0KICAgICAgICA8L2FkZHJlc3M+DQogICAgICA8L1BhdGllbnQ+DQogICAgPC9yZXNvdXJjZT4NCiAgPC9lbnRyeT4NCiAgPGVudHJ5Pg0KICAgIDxmdWxsVXJsIHZhbHVlPSJodHRwOi8vbG9jYWxob3N0OjQ3OTkvZmhpci9QcmFjdGl0aW9uZXIvZmEzZTQzMjAtMGJhMy00NWU3LWJjOWYtOTE0NTUxNjFiNmJhIiAvPg0KICAgIDxyZXNvdXJjZT4NCiAgICAgIDxQcmFjdGl0aW9uZXI+DQogICAgICAgIDxpZCB2YWx1ZT0iZmEzZTQzMjAtMGJhMy00NWU3LWJjOWYtOTE0NTUxNjFiNmJhIiAvPg0KICAgICAgICA8bWV0YT4NCiAgICAgICAgICA8cHJvZmlsZSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9TdHJ1Y3R1cmVEZWZpbml0aW9uL0tCVl9QUl9GT1JfUHJhY3RpdGlvbmVyfDEuMC4zIiAvPg0KICAgICAgICA8L21ldGE+DQogICAgICAgIDxpZGVudGlmaWVyPg0KICAgICAgICAgIDx0eXBlPg0KICAgICAgICAgICAgPGNvZGluZz4NCiAgICAgICAgICAgICAgPHN5c3RlbSB2YWx1ZT0iaHR0cDovL3Rlcm1pbm9sb2d5LmhsNy5vcmcvQ29kZVN5c3RlbS92Mi0wMjAzIiAvPg0KICAgICAgICAgICAgICA8Y29kZSB2YWx1ZT0iTEFOUiIgLz4NCiAgICAgICAgICAgIDwvY29kaW5nPg0KICAgICAgICAgIDwvdHlwZT4NCiAgICAgICAgICA8c3lzdGVtIHZhbHVlPSJodHRwczovL2ZoaXIua2J2LmRlL05hbWluZ1N5c3RlbS9LQlZfTlNfQmFzZV9BTlIiIC8+DQogICAgICAgICAgPHZhbHVlIHZhbHVlPSI2NjI2NDI2MDEiIC8+DQogICAgICAgIDwvaWRlbnRpZmllcj4NCiAgICAgICAgPG5hbWU+DQogICAgICAgICAgPHVzZSB2YWx1ZT0ib2ZmaWNpYWwiIC8+DQogICAgICAgICAgPGZhbWlseSB2YWx1ZT0iUHJheGlzIERyLiBTaGFoaW4gQS4gTWlya291aGkiPg0KICAgICAgICAgICAgPGV4dGVuc2lvbiB1cmw9Imh0dHA6Ly9obDcub3JnL2ZoaXIvU3RydWN0dXJlRGVmaW5pdGlvbi9odW1hbm5hbWUtb3duLW5hbWUiPg0KICAgICAgICAgICAgICA8dmFsdWVTdHJpbmcgdmFsdWU9IlByYXhpcyBEci4gU2hhaGluIEEuIE1pcmtvdWhpIiAvPg0KICAgICAgICAgICAgPC9leHRlbnNpb24+DQogICAgICAgICAgPC9mYW1pbHk+DQogICAgICAgICAgPGdpdmVuIHZhbHVlPSItIiAvPg0KICAgICAgICA8L25hbWU+DQogICAgICAgIDxxdWFsaWZpY2F0aW9uPg0KICAgICAgICAgIDxjb2RlPg0KICAgICAgICAgICAgPGNvZGluZz4NCiAgICAgICAgICAgICAgPHN5c3RlbSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9Db2RlU3lzdGVtL0tCVl9DU19GT1JfUXVhbGlmaWNhdGlvbl9UeXBlIiAvPg0KICAgICAgICAgICAgICA8Y29kZSB2YWx1ZT0iMDEwIiAvPg0KICAgICAgICAgICAgPC9jb2Rpbmc+DQogICAgICAgICAgPC9jb2RlPg0KICAgICAgICA8L3F1YWxpZmljYXRpb24+DQogICAgICAgIDxxdWFsaWZpY2F0aW9uPg0KICAgICAgICAgIDxjb2RlPg0KICAgICAgICAgICAgPHRleHQgdmFsdWU9IkFyenQiIC8+DQogICAgICAgICAgPC9jb2RlPg0KICAgICAgICA8L3F1YWxpZmljYXRpb24+DQogICAgICA8L1ByYWN0aXRpb25lcj4NCiAgICA8L3Jlc291cmNlPg0KICA8L2VudHJ5Pg0KICA8ZW50cnk+DQogICAgPGZ1bGxVcmwgdmFsdWU9Imh0dHA6Ly9sb2NhbGhvc3Q6NDc5OS9maGlyL09yZ2FuaXphdGlvbi8xZjNhYzRjNS00NDFkLTRjNDItYWQ5Zi0yMjY2YTgyNWRmNjgiIC8+DQogICAgPHJlc291cmNlPg0KICAgICAgPE9yZ2FuaXphdGlvbj4NCiAgICAgICAgPGlkIHZhbHVlPSIxZjNhYzRjNS00NDFkLTRjNDItYWQ5Zi0yMjY2YTgyNWRmNjgiIC8+DQogICAgICAgIDxtZXRhPg0KICAgICAgICAgIDxwcm9maWxlIHZhbHVlPSJodHRwczovL2ZoaXIua2J2LmRlL1N0cnVjdHVyZURlZmluaXRpb24vS0JWX1BSX0ZPUl9Pcmdhbml6YXRpb258MS4wLjMiIC8+DQogICAgICAgIDwvbWV0YT4NCiAgICAgICAgPGlkZW50aWZpZXI+DQogICAgICAgICAgPHR5cGU+DQogICAgICAgICAgICA8Y29kaW5nPg0KICAgICAgICAgICAgICA8c3lzdGVtIHZhbHVlPSJodHRwOi8vdGVybWlub2xvZ3kuaGw3Lm9yZy9Db2RlU3lzdGVtL3YyLTAyMDMiIC8+DQogICAgICAgICAgICAgIDxjb2RlIHZhbHVlPSJCU05SIiAvPg0KICAgICAgICAgICAgPC9jb2Rpbmc+DQogICAgICAgICAgPC90eXBlPg0KICAgICAgICAgIDxzeXN0ZW0gdmFsdWU9Imh0dHBzOi8vZmhpci5rYnYuZGUvTmFtaW5nU3lzdGVtL0tCVl9OU19CYXNlX0JTTlIiIC8+DQogICAgICAgICAgPHZhbHVlIHZhbHVlPSI3Mjg0MTkyMDAiIC8+DQogICAgICAgIDwvaWRlbnRpZmllcj4NCiAgICAgICAgPG5hbWUgdmFsdWU9IlByYXhpcyBEci4gU2hhaGluIEEuIE1pcmtvdWhpIiAvPg0KICAgICAgICA8dGVsZWNvbT4NCiAgICAgICAgICA8c3lzdGVtIHZhbHVlPSJwaG9uZSIgLz4NCiAgICAgICAgICA8dmFsdWUgdmFsdWU9IjAzMC82ODQgMzUzNiIgLz4NCiAgICAgICAgPC90ZWxlY29tPg0KICAgICAgICA8dGVsZWNvbT4NCiAgICAgICAgICA8c3lzdGVtIHZhbHVlPSJmYXgiIC8+DQogICAgICAgICAgPHZhbHVlIHZhbHVlPSIwMzAvNjgwIDU1NjQ2IiAvPg0KICAgICAgICA8L3RlbGVjb20+DQogICAgICAgIDxhZGRyZXNzPg0KICAgICAgICAgIDx0eXBlIHZhbHVlPSJib3RoIiAvPg0KICAgICAgICAgIDxsaW5lIHZhbHVlPSJIYXVwdHN0cmHDn2UgMTMxIj4NCiAgICAgICAgICAgIDxleHRlbnNpb24gdXJsPSJodHRwOi8vaGw3Lm9yZy9maGlyL1N0cnVjdHVyZURlZmluaXRpb24vaXNvMjEwOTAtQURYUC1ob3VzZU51bWJlciI+DQogICAgICAgICAgICAgIDx2YWx1ZVN0cmluZyB2YWx1ZT0iMTMxIiAvPg0KICAgICAgICAgICAgPC9leHRlbnNpb24+DQogICAgICAgICAgICA8ZXh0ZW5zaW9uIHVybD0iaHR0cDovL2hsNy5vcmcvZmhpci9TdHJ1Y3R1cmVEZWZpbml0aW9uL2lzbzIxMDkwLUFEWFAtc3RyZWV0TmFtZSI+DQogICAgICAgICAgICAgIDx2YWx1ZVN0cmluZyB2YWx1ZT0iSGF1cHRzdHJhw59lIiAvPg0KICAgICAgICAgICAgPC9leHRlbnNpb24+DQogICAgICAgICAgPC9saW5lPg0KICAgICAgICAgIDxjaXR5IHZhbHVlPSJCZXJsaW4iIC8+DQogICAgICAgICAgPHBvc3RhbENvZGUgdmFsdWU9IjEwODI3IiAvPg0KICAgICAgICAgIDxjb3VudHJ5IHZhbHVlPSJEIiAvPg0KICAgICAgICA8L2FkZHJlc3M+DQogICAgICA8L09yZ2FuaXphdGlvbj4NCiAgICA8L3Jlc291cmNlPg0KICA8L2VudHJ5Pg0KICA8ZW50cnk+DQogICAgPGZ1bGxVcmwgdmFsdWU9Imh0dHA6Ly9sb2NhbGhvc3Q6NDc5OS9maGlyL0NvdmVyYWdlLzJjNzRjZDVkLWU2MDgtNGIzOC1hZGJhLWNlMWE5YzUzN2Q1YSIgLz4NCiAgICA8cmVzb3VyY2U+DQogICAgICA8Q292ZXJhZ2U+DQogICAgICAgIDxpZCB2YWx1ZT0iMmM3NGNkNWQtZTYwOC00YjM4LWFkYmEtY2UxYTljNTM3ZDVhIiAvPg0KICAgICAgICA8bWV0YT4NCiAgICAgICAgICA8cHJvZmlsZSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9TdHJ1Y3R1cmVEZWZpbml0aW9uL0tCVl9QUl9GT1JfQ292ZXJhZ2V8MS4wLjMiIC8+DQogICAgICAgIDwvbWV0YT4NCiAgICAgICAgPGV4dGVuc2lvbiB1cmw9Imh0dHA6Ly9maGlyLmRlL1N0cnVjdHVyZURlZmluaXRpb24vZ2t2L2Jlc29uZGVyZS1wZXJzb25lbmdydXBwZSI+DQogICAgICAgICAgPHZhbHVlQ29kaW5nPg0KICAgICAgICAgICAgPHN5c3RlbSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9Db2RlU3lzdGVtL0tCVl9DU19TRkhJUl9LQlZfUEVSU09ORU5HUlVQUEUiIC8+DQogICAgICAgICAgICA8Y29kZSB2YWx1ZT0iMDAiIC8+DQogICAgICAgICAgPC92YWx1ZUNvZGluZz4NCiAgICAgICAgPC9leHRlbnNpb24+DQogICAgICAgIDxleHRlbnNpb24gdXJsPSJodHRwOi8vZmhpci5kZS9TdHJ1Y3R1cmVEZWZpbml0aW9uL2drdi9kbXAta2VubnplaWNoZW4iPg0KICAgICAgICAgIDx2YWx1ZUNvZGluZz4NCiAgICAgICAgICAgIDxzeXN0ZW0gdmFsdWU9Imh0dHBzOi8vZmhpci5rYnYuZGUvQ29kZVN5c3RlbS9LQlZfQ1NfU0ZISVJfS0JWX0RNUCIgLz4NCiAgICAgICAgICAgIDxjb2RlIHZhbHVlPSIwMCIgLz4NCiAgICAgICAgICA8L3ZhbHVlQ29kaW5nPg0KICAgICAgICA8L2V4dGVuc2lvbj4NCiAgICAgICAgPGV4dGVuc2lvbiB1cmw9Imh0dHA6Ly9maGlyLmRlL1N0cnVjdHVyZURlZmluaXRpb24vZ2t2L3ZlcnNpY2hlcnRlbmFydCI+DQogICAgICAgICAgPHZhbHVlQ29kaW5nPg0KICAgICAgICAgICAgPHN5c3RlbSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9WYWx1ZVNldC9LQlZfVlNfU0ZISVJfS0JWX1ZFUlNJQ0hFUlRFTlNUQVRVUyIgLz4NCiAgICAgICAgICAgIDxjb2RlIHZhbHVlPSIxIiAvPg0KICAgICAgICAgIDwvdmFsdWVDb2Rpbmc+DQogICAgICAgIDwvZXh0ZW5zaW9uPg0KICAgICAgICA8ZXh0ZW5zaW9uIHVybD0iaHR0cDovL2ZoaXIuZGUvU3RydWN0dXJlRGVmaW5pdGlvbi9na3Yvd29wIj4NCiAgICAgICAgICA8dmFsdWVDb2Rpbmc+DQogICAgICAgICAgICA8c3lzdGVtIHZhbHVlPSJodHRwczovL2ZoaXIua2J2LmRlL1ZhbHVlU2V0L0tCVl9WU19TRkhJUl9JVEFfV09QIiAvPg0KICAgICAgICAgICAgPGNvZGUgdmFsdWU9IjAwIiAvPg0KICAgICAgICAgIDwvdmFsdWVDb2Rpbmc+DQogICAgICAgIDwvZXh0ZW5zaW9uPg0KICAgICAgICA8c3RhdHVzIHZhbHVlPSJhY3RpdmUiIC8+DQogICAgICAgIDx0eXBlPg0KICAgICAgICAgIDxjb2Rpbmc+DQogICAgICAgICAgICA8c3lzdGVtIHZhbHVlPSJodHRwOi8vZmhpci5kZS9Db2RlU3lzdGVtL3ZlcnNpY2hlcnVuZ3NhcnQtZGUtYmFzaXMiIC8+DQogICAgICAgICAgICA8Y29kZSB2YWx1ZT0iR0tWIiAvPg0KICAgICAgICAgIDwvY29kaW5nPg0KICAgICAgICA8L3R5cGU+DQogICAgICAgIDxiZW5lZmljaWFyeT4NCiAgICAgICAgICA8cmVmZXJlbmNlIHZhbHVlPSJQYXRpZW50LzNiZTk4Njg1LThhYmEtNGE3Ni1hOTMwLWYxMTlhZDMwYWU2ZiIgLz4NCiAgICAgICAgPC9iZW5lZmljaWFyeT4NCiAgICAgICAgPHBheW9yPg0KICAgICAgICAgIDxpZGVudGlmaWVyPg0KICAgICAgICAgICAgPHN5c3RlbSB2YWx1ZT0iaHR0cDovL2ZoaXIuZGUv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";

    ASSERT_NO_THROW(CadesBesSignature(signedText, *manager));
}


TEST_F(CadesBesSignatureTest, validateRequiredSignedFields)//NOLINT(readability-function-cognitive-complexity)
{
    std::string_view myText = "The text to be signed";
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.prv.pem")});

    auto cert = Certificate::fromPem(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem"));

    std::string signedText;
    {
        CadesBesSignature cadesBesSignature{ cert, privKey, std::string{myText}};
        ASSERT_NO_THROW(signedText = Base64::encode(cadesBesSignature.get()));
    }

    const auto plain = Base64::decode(signedText);
    auto bioIndata = shared_BIO::make();
    ASSERT_TRUE(plain.size() <= static_cast<size_t>(std::numeric_limits<int>::max()));
    ASSERT_TRUE(BIO_write(bioIndata.get(), plain.data(), static_cast<int>(plain.size())) ==
                  static_cast<int>(plain.size()));
    shared_CMS_ContentInfo contentInfo = shared_CMS_ContentInfo::make(d2i_CMS_bio(bioIndata.get(), nullptr));
    ASSERT_NE(nullptr, contentInfo);

    auto* signerInfos = CMS_get0_SignerInfos(contentInfo);
    ASSERT_NE(nullptr, signerInfos);
    const int signerInfosCount = sk_CMS_SignerInfo_num(signerInfos);
    ASSERT_TRUE(signerInfosCount > 0);

    for (int ind = 0; ind < signerInfosCount; ind++)
    {
        auto* signerInfo = sk_CMS_SignerInfo_value(signerInfos, ind);
        ASSERT_NE(nullptr, signerInfo);

        ASSERT_TRUE(CMS_signed_get_attr_by_NID(signerInfo, NID_pkcs9_contentType, -1) >= 0);
        ASSERT_TRUE(CMS_signed_get_attr_by_NID(signerInfo, NID_pkcs9_signingTime, -1) >= 0);
        ASSERT_TRUE(CMS_signed_get_attr_by_NID(signerInfo, NID_pkcs9_messageDigest, -1) >= 0);

        ASSERT_TRUE(CMS_signed_get_attr_by_NID(signerInfo, NID_id_smime_aa_signingCertificate, -1) >= 0
            || CMS_signed_get_attr_by_NID(signerInfo, NID_id_smime_aa_signingCertificateV2, -1) >= 0);
    }
}


TEST_F(CadesBesSignatureTest, missingCertificateAttributesDetected)
{
    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>();

    const std::string cmsWithoutCertificateAttributes = "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";
    try
    {
        CadesBesSignature sig(cmsWithoutCertificateAttributes, *manager);
        FAIL() << "unexpected success";
    }
    catch(const CadesBesSignature::VerificationException& e)
    {
        ASSERT_EQ(e.what(), std::string("No certificate in signed info."));
    }
    catch(...)
    {
        FAIL() << "unexpected exception";
    }
}


TEST_F(CadesBesSignatureTest, validateQesG0NoOcspProxy)
{
    std::string_view myText = "The text to be signed";
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/QES-G0-Certificate.prv.pem")});

    auto cert = Certificate::fromPem(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/QES-G0-Certificate.pem"));

    auto certCA = Certificate::fromPem(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/QES-G0-CertificateCA.pem"));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ocsp.test.ibm.de/",
             {{cert, certCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    manager->addPostUpdateHook([=]{
        TslTestHelper::addCaCertificateToTrustStore(certCA, *manager, TslMode::BNA);
    });

    std::string signedText;
    {
        CadesBesSignature cadesBesSignature{ cert, privKey, std::string{myText}};
        ASSERT_NO_THROW(signedText = Base64::encode(cadesBesSignature.get()));
    }
    {
        CadesBesSignature cadesBesSignature{signedText, *manager};
        EXPECT_EQ(cadesBesSignature.payload(), myText);
    }
}


TEST_F(CadesBesSignatureTest, validateQesG0WithOcspProxy)
{
    std::string_view myText = "The text to be signed";
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/QES-G0-Certificate.prv.pem")});

    auto cert = Certificate::fromPem(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/QES-G0-Certificate.pem"));

    auto certCA = Certificate::fromPem(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/QES-G0-CertificateCA.pem"));

    // if proxy URL is set, it must be used for the certificate OCSP request,
    // because OCSP URL from TEST Certificate AIA Extension is not mapped in TSL
    const std::string ocspProxyUrl = "http://ocsp.proxy.ibm.de/";
    const std::string ocspUrlFromCert = "http://ocsp.test.ibm.de/";
    // The Gematik TI proxy works in the way that the original ocsp url is appended to the
    // the proxy url, e.g. "http://ocsp.proxy.ibm.de/http://ocsp.test.ibm.de/"
    EnvironmentVariableGuard guard("ERP_TSL_TI_OCSP_PROXY_URL", ocspProxyUrl);
    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {ocspProxyUrl + ocspUrlFromCert,
                {{cert, certCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    manager->addPostUpdateHook([=]{
        TslTestHelper::addCaCertificateToTrustStore(certCA, *manager, TslMode::BNA);
    });

    std::string signedText;
    {
        CadesBesSignature cadesBesSignature{ cert, privKey, std::string{myText}};
        ASSERT_NO_THROW(signedText = Base64::encode(cadesBesSignature.get()));
    }
    {
        CadesBesSignature cadesBesSignature{signedText, *manager};
        EXPECT_EQ(cadesBesSignature.payload(), myText);
    }
}


TEST_F(CadesBesSignatureTest, emptyCMSBundle)
{
    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>();
    ASSERT_THROW(CadesBesSignature("", *manager),
                 CadesBesSignature::VerificationException);
}


#ifndef _WIN32 // MSVC complains with error C2026: String too long
TEST_F(CadesBesSignatureTest, invalidSignatureInCMSBundle)
{
    std::string signedText = "MII2hwYJKoZIhvcNAQcCoII2eDCCNnQCAQExDzANBglghkgBZQMEAgEFADCCL9sGCSqGSIb3DQEHAaCCL8wEgi/IPEJ1bmRsZSB4bWxucz0iaHR0cDovL2hsNy5vcmcvZmhpciI+PGlkIHZhbHVlPSI0NTJhZmIxYi01Y2RlLTQ5Y2UtODBmMS0xMGFkOTA5NzQ0YzUiPjwvaWQ+PG1ldGE+PHByb2ZpbGUgdmFsdWU9Imh0dHBzOi8vZmhpci5rYnYuZGUvU3RydWN0dXJlRGVmaW5pdGlvbi9LQlZfUFJfRVJQX0J1bmRsZXwxLjAuMSI+PC9wcm9maWxlPjwvbWV0YT48aWRlbnRpZmllcj48c3lzdGVtIHZhbHVlPSJodHRwczovL2dlbWF0aWsuZGUvZmhpci9OYW1pbmdTeXN0ZW0vUHJlc2NyaXB0aW9uSUQiPjwvc3lzdGVtPjx2YWx1ZSB2YWx1ZT0iMTYwLjAwMC4wMDAuMDAxLjkzMi44MSI+PC92YWx1ZT48L2lkZW50aWZpZXI+PHR5cGUgdmFsdWU9ImRvY3VtZW50Ij48L3R5cGU+PHRpbWVzdGFtcCB2YWx1ZT0iMjAyMS0wNi0wN1QxNzozODowMS42NDUrMDI6MDAiPjwvdGltZXN0YW1wPjxlbnRyeT48ZnVsbFVybCB2YWx1ZT0iaHR0cHM6Ly9lLXJlemVwdC5kZS9Db21wb3NpdGlvbi9jY2EwZDlkYS00ZDMzLTQ3N2YtOTdkMS1jOTIzNGFkZWVmMzEiPjwvZnVsbFVybD48cmVzb3VyY2U+PENvbXBvc2l0aW9uIHhtbG5zPSJodHRwOi8vaGw3Lm9yZy9maGlyIj48aWQgdmFsdWU9ImNjYTBkOWRhLTRkMzMtNDc3Zi05N2QxLWM5MjM0YWRlZWYzMSI+PC9pZD48bWV0YT48cHJvZmlsZSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9TdHJ1Y3R1cmVEZWZpbml0aW9uL0tCVl9QUl9FUlBfQ29tcG9zaXRpb258MS4wLjEiPjwvcHJvZmlsZT48L21ldGE+PGV4dGVuc2lvbiB1cmw9Imh0dHBzOi8vZmhpci5rYnYuZGUvU3RydWN0dXJlRGVmaW5pdGlvbi9LQlZfRVhfRk9SX0xlZ2FsX2Jhc2lzIj48dmFsdWVDb2Rpbmc+PHN5c3RlbSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9Db2RlU3lzdGVtL0tCVl9DU19TRkhJUl9LQlZfU1RBVFVTS0VOTlpFSUNIRU4iPjwvc3lzdGVtPjxjb2RlIHZhbHVlPSIwMCI+PC9jb2RlPjwvdmFsdWVDb2Rpbmc+PC9leHRlbnNpb24+PHN0YXR1cyB2YWx1ZT0iZmluYWwiPjwvc3RhdHVzPjx0eXBlPjxjb2Rpbmc+PHN5c3RlbSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9Db2RlU3lzdGVtL0tCVl9DU19TRkhJUl9LQlZfRk9STVVMQVJfQVJUIj48L3N5c3RlbT48Y29kZSB2YWx1ZT0iZTE2QSI+PC9jb2RlPjwvY29kaW5nPjwvdHlwZT48c3ViamVjdD48cmVmZXJlbmNlIHZhbHVlPSJQYXRpZW50L2M3YjA0OTZkLTk0NzQtNDA4ZC05Nzc5LTM0ODBmMjRmNGIyMSI+PC9yZWZlcmVuY2U+PC9zdWJqZWN0PjxkYXRlIHZhbHVlPSIyMDIxLTA2LTA3VDE3OjM4OjAxKzAyOjAwIj48L2RhdGU+PGF1dGhvcj48cmVmZXJlbmNlIHZhbHVlPSJQcmFjdGl0aW9uZXIvOTQzZGEzZTYtYzQ4Yi00ODk5LWE0ZmItNzc5ZTZiZTY1NTQzIj48L3JlZmVyZW5jZT48dHlwZSB2YWx1ZT0iUHJhY3RpdGlvbmVyIj48L3R5cGU+PC9hdXRob3I+PGF1dGhvcj48dHlwZSB2YWx1ZT0iRGV2aWNlIj48L3R5cGU+PGlkZW50aWZpZXI+PHN5c3RlbSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9OYW1pbmdTeXN0ZW0vS0JWX05TX0ZPUl9QcnVlZm51bW1lciI+PC9zeXN0ZW0+PHZhbHVlIHZhbHVlPSJZLzQwMC8xOTEwLzM2LzM0NiI+PC92YWx1ZT48L2lkZW50aWZpZXI+PC9hdXRob3I+PHRpdGxlIHZhbHVlPSJlbGVrdHJvbmlzY2hlIEFyem5laW1pdHRlbHZlcm9yZG51bmciPjwvdGl0bGU+PGN1c3RvZGlhbj48cmVmZXJlbmNlIHZhbHVlPSJPcmdhbml6YXRpb24vNjAwMDQ5OGItOTdhNy00M2I5LWJlN2UtOTc3NTBiMTc5YWU5Ij48L3JlZmVyZW5jZT48L2N1c3RvZGlhbj48c2VjdGlvbj48Y29kZT48Y29kaW5nPjxzeXN0ZW0gdmFsdWU9Imh0dHBzOi8vZmhpci5rYnYuZGUvQ29kZVN5c3RlbS9LQlZfQ1NfRVJQX1NlY3Rpb25fVHlwZSI+PC9zeXN0ZW0+PGNvZGUgdmFsdWU9IlByZXNjcmlwdGlvbiI+PC9jb2RlPjwvY29kaW5nPjwvY29kZT48ZW50cnk+PHJlZmVyZW5jZSB2YWx1ZT0iTWVkaWNhdGlvblJlcXVlc3QvZTE1NWUxN2YtOTM5Zi00ZTliLWEwYTYtMzljMzVlZmUzZDg3Ij48L3JlZmVyZW5jZT48L2VudHJ5Pjwvc2VjdGlvbj48c2VjdGlvbj48Y29kZT48Y29kaW5nPjxzeXN0ZW0gdmFsdWU9Imh0dHBzOi8vZmhpci5rYnYuZGUvQ29kZVN5c3RlbS9LQlZfQ1NfRVJQX1NlY3Rpb25fVHlwZSI+PC9zeXN0ZW0+PGNvZGUgdmFsdWU9IkNvdmVyYWdlIj48L2NvZGU+PC9jb2Rpbmc+PC9jb2RlPjxlbnRyeT48cmVmZXJlbmNlIHZhbHVlPSJDb3ZlcmFnZS9kMzdkMjZiNy0wMTZjLTQ0ZWYtOTNiNS1hZDhiZjM4MWExNTgiPjwvcmVmZXJlbmNlPjwvZW50cnk+PC9zZWN0aW9uPjxzZWN0aW9uPjxjb2RlPjxjb2Rpbmc+PHN5c3RlbSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9Db2RlU3lzdGVtL0tCVl9DU19FUlBfU2VjdGlvbl9UeXBlIj48L3N5c3RlbT48Y29kZSB2YWx1ZT0iRk9SX1ByYWN0aXRpb25lclJvbGUiPjwvY29kZT48L2NvZGluZz48L2NvZGU+PGVudHJ5PjxyZWZlcmVuY2UgdmFsdWU9IlByYWN0aXRpb25lclJvbGUvZjdhODdmMWQtMzg4MS00NTI0LWFkMTItMjQzNjYxYmM2M2ZhIj48L3JlZmVyZW5jZT48L2VudHJ5Pjwvc2VjdGlvbj48L0NvbXBvc2l0aW9uPjwvcmVzb3VyY2U+PC9lbnRyeT48ZW50cnk+PGZ1bGxVcmwgdmFsdWU9Imh0dHBzOi8vZS1yZXplcHQuZGUvTWVkaWNhdGlvblJlcXVlc3QvZTE1NWUxN2YtOTM5Zi00ZTliLWEwYTYtMzljMzVlZmUzZDg3Ij48L2Z1bGxVcmw+PHJlc291cmNlPjxNZWRpY2F0aW9uUmVxdWVzdCB4bWxucz0iaHR0cDovL2hsNy5vcmcvZmhpciI+PGlkIHZhbHVlPSJlMTU1ZTE3Zi05MzlmLTRlOWItYTBhNi0zOWMzNWVmZTNkODciPjwvaWQ+PG1ldGE+PHByb2ZpbGUgdmFsdWU9Imh0dHBzOi8vZmhpci5rYnYuZGUvU3RydWN0dXJlRGVmaW5pdGlvbi9LQlZfUFJfRVJQX1ByZXNjcmlwdGlvbnwxLjAuMSI+PC9wcm9maWxlPjwvbWV0YT48ZXh0ZW5zaW9uIHVybD0iaHR0cHM6Ly9maGlyLmtidi5kZS9TdHJ1Y3R1cmVEZWZpbml0aW9uL0tCVl9FWF9FUlBfU3RhdHVzQ29QYXltZW50Ij48dmFsdWVDb2Rpbmc+PHN5c3RlbSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9Db2RlU3lzdGVtL0tCVl9DU19FUlBfU3RhdHVzQ29QYXltZW50Ij48L3N5c3RlbT48Y29kZSB2YWx1ZT0iMCI+PC9jb2RlPjwvdmFsdWVDb2Rpbmc+PC9leHRlbnNpb24+PGV4dGVuc2lvbiB1cmw9Imh0dHBzOi8vZmhpci5rYnYuZGUvU3RydWN0dXJlRGVmaW5pdGlvbi9LQlZfRVhfRVJQX0VtZXJnZW5jeVNlcnZpY2VzRmVlIj48dmFsdWVCb29sZWFuIHZhbHVlPSJmYWxzZSI+PC92YWx1ZUJvb2xlYW4+PC9leHRlbnNpb24+PGV4dGVuc2lvbiB1cmw9Imh0dHBzOi8vZmhpci5rYnYuZGUvU3RydWN0dXJlRGVmaW5pdGlvbi9LQlZfRVhfRVJQX0JWRyI+PHZhbHVlQm9vbGVhbiB2YWx1ZT0iZmFsc2UiPjwvdmFsdWVCb29sZWFuPjwvZXh0ZW5zaW9uPjxleHRlbnNpb24gdXJsPSJodHRwczovL2ZoaXIua2J2LmRlL1N0cnVjdHVyZURlZmluaXRpb24vS0JWX0VYX0VSUF9BY2NpZGVudCI+PGV4dGVuc2lvbiB1cmw9InVuZmFsbGtlbm56ZWljaGVuIj48dmFsdWVDb2Rpbmc+PHN5c3RlbSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9Db2RlU3lzdGVtL0tCVl9DU19GT1JfVXJzYWNoZV9UeXBlIj48L3N5c3RlbT48Y29kZSB2YWx1ZT0iMiI+PC9jb2RlPjwvdmFsdWVDb2Rpbmc+PC9leHRlbnNpb24+PGV4dGVuc2lvbiB1cmw9InVuZmFsbGJldHJpZWIiPjx2YWx1ZVN0cmluZyB2YWx1ZT0iRHVtbXktQmV0cmllYiI+PC92YWx1ZVN0cmluZz48L2V4dGVuc2lvbj48ZXh0ZW5zaW9uIHVybD0idW5mYWxsdGFnIj48dmFsdWVEYXRlIHZhbHVlPSIyMDIxLTA2LTA3Ij48L3ZhbHVlRGF0ZT48L2V4dGVuc2lvbj48L2V4dGVuc2lvbj48ZXh0ZW5zaW9uIHVybD0iaHR0cHM6Ly9maGlyLmtidi5kZS9TdHJ1Y3R1cmVEZWZpbml0aW9uL0tCVl9FWF9FUlBfTXVsdGlwbGVfUHJlc2NyaXB0aW9uIj48ZXh0ZW5zaW9uIHVybD0iS2VubnplaWNoZW4iPjx2YWx1ZUJvb2xlYW4gdmFsdWU9ImZhbHNlIj48L3ZhbHVlQm9vbGVhbj48L2V4dGVuc2lvbj48ZXh0ZW5zaW9uIHVybD0iTnVtbWVyaWVydW5nIj48dmFsdWVSYXRpbz48bnVtZXJhdG9yPjx2YWx1ZSB2YWx1ZT0iMCI+PC92YWx1ZT48L251bWVyYXRvcj48ZGVub21pbmF0b3I+PHZhbHVlIHZhbHVlPSIwIj48L3ZhbHVlPjwvZGVub21pbmF0b3I+PC92YWx1ZVJhdGlvPjwvZXh0ZW5zaW9uPjxleHRlbnNpb24gdXJsPSJaZWl0cmF1bSI+PHZhbHVlUGVyaW9kPjxzdGFydCB2YWx1ZT0iMjAyMS0wNi0wN1QxNzozODowMSswMjowMCI+PC9zdGFydD48ZW5kIHZhbHVlPSIyMDIxLTA2LTA3VDE3OjM4OjAxKzAyOjAwIj48L2VuZD48L3ZhbHVlUGVyaW9kPjwvZXh0ZW5zaW9uPjwvZXh0ZW5zaW9uPjxzdGF0dXMgdmFsdWU9ImFjdGl2ZSI+PC9zdGF0dXM+PGludGVudCB2YWx1ZT0ib3JkZXIiPjwvaW50ZW50PjxtZWRpY2F0aW9uUmVmZXJlbmNlPjxyZWZlcmVuY2UgdmFsdWU9Ik1lZGljYXRpb24vMDdiMmZiNDctMmVhOC00ZGI4LThjYjgtOWZjNDVhODQ3OWQ3Ij48L3JlZmVyZW5jZT48L21lZGljYXRpb25SZWZlcmVuY2U+PHN1YmplY3Q+PHJlZmVyZW5jZSB2YWx1ZT0iUGF0aWVudC9jN2IwNDk2ZC05NDc0LTQwOGQtOTc3OS0zNDgwZjI0ZjRiMjEiPjwvcmVmZXJlbmNlPjwvc3ViamVjdD48YXV0aG9yZWRPbiB2YWx1ZT0iMjAwMC0wMS0wMSI+PC9hdXRob3JlZE9uPjxyZXF1ZXN0ZXI+PHJlZmVyZW5jZSB2YWx1ZT0iUHJhY3RpdGlvbmVyLzk0M2RhM2U2LWM0OGItNDg5OS1hNGZiLTc3OWU2YmU2NTU0MyI+PC9yZWZlcmVuY2U+PC9yZXF1ZXN0ZXI+PGluc3VyYW5jZT48cmVmZXJlbmNlIHZhbHVlPSJDb3ZlcmFnZS9kMzdkMjZiNy0wMTZjLTQ0ZWYtOTNiNS1hZDhiZjM4MWExNTgiPjwvcmVmZXJlbmNlPjwvaW5zdXJhbmNlPjxkb3NhZ2VJbnN0cnVjdGlvbj48ZXh0ZW5zaW9uIHVybD0iaHR0cHM6Ly9maGlyLmtidi5kZS9TdHJ1Y3R1cmVEZWZpbml0aW9uL0tCVl9FWF9FUlBfRG9zYWdlRmxhZyI+PHZhbHVlQm9vbGVhbiB2YWx1ZT0idHJ1ZSI+PC92YWx1ZUJvb2xlYW4+PC9leHRlbnNpb24+PHRleHQgdmFsdWU9IjEtMC0xLTAiPjwvdGV4dD48L2Rvc2FnZUluc3RydWN0aW9uPjxkaXNwZW5zZVJlcXVlc3Q+PHZhbGlkaXR5UGVyaW9kPjxzdGFydCB2YWx1ZT0iMjAyMS0wNi0wN1QxNzozODowMSswMjowMCI+PC9zdGFydD48ZW5kIHZhbHVlPSIyMDIxLTA2LTA3VDE3OjM4OjAxKzAyOjAwIj48L2VuZD48L3ZhbGlkaXR5UGVyaW9kPjxxdWFudGl0eT48dmFsdWUgdmFsdWU9IjEiPjwvdmFsdWU+PHN5c3RlbSB2YWx1ZT0iaHR0cDovL3VuaXRzb2ZtZWFzdXJlLm9yZyI+PC9zeXN0ZW0+PGNvZGUgdmFsdWU9IntQYWNrYWdlfSI+PC9jb2RlPjwvcXVhbnRpdHk+PC9kaXNwZW5zZVJlcXVlc3Q+PHN1YnN0aXR1dGlvbj48YWxsb3dlZEJvb2xlYW4gdmFsdWU9InRydWUiPjwvYWxsb3dlZEJvb2xlYW4+PC9zdWJzdGl0dXRpb24+PC9NZWRpY2F0aW9uUmVxdWVzdD48L3Jlc291cmNlPjwvZW50cnk+PGVudHJ5PjxmdWxsVXJsIHZhbHVlPSJodHRwczovL2UtcmV6ZXB0LmRlL01lZGljYXRpb24vMDdiMmZiNDctMmVhOC00ZGI4LThjYjgtOWZjNDVhODQ3OWQ3Ij48L2Z1bGxVcmw+PHJlc291cmNlPjxNZWRpY2F0aW9uIHhtbG5zPSJodHRwOi8vaGw3Lm9yZy9maGlyIj48aWQgdmFsdWU9IjA3YjJmYjQ3LTJlYTgtNGRiOC04Y2I4LTlmYzQ1YTg0NzlkNyI+PC9pZD48bWV0YT48cHJvZmlsZSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9TdHJ1Y3R1cmVEZWZpbml0aW9uL0tCVl9QUl9FUlBfTWVkaWNhdGlvbl9QWk58MS4wLjEiPjwvcHJvZmlsZT48L21ldGE+PGV4dGVuc2lvbiB1cmw9Imh0dHBzOi8vZmhpci5rYnYuZGUvU3RydWN0dXJlRGVmaW5pdGlvbi9LQlZfRVhfRVJQX01lZGljYXRpb25fQ2F0ZWdvcnkiPjx2YWx1ZUNvZGluZz48c3lzdGVtIHZhbHVlPSJodHRwczovL2ZoaXIua2J2LmRlL0NvZGVTeXN0ZW0vS0JWX0NTX0VSUF9NZWRpY2F0aW9uX0NhdGVnb3J5Ij48L3N5c3RlbT48Y29kZSB2YWx1ZT0iMDAiPjwvY29kZT48L3ZhbHVlQ29kaW5nPjwvZXh0ZW5zaW9uPjxleHRlbnNpb24gdXJsPSJodHRwczovL2ZoaXIua2J2LmRlL1N0cnVjdHVyZURlZmluaXRpb24vS0JWX0VYX0VSUF9NZWRpY2F0aW9uX1ZhY2NpbmUiPjx2YWx1ZUJvb2xlYW4gdmFsdWU9ImZhbHNlIj48L3ZhbHVlQm9vbGVhbj48L2V4dGVuc2lvbj48Y29kZT48Y29kaW5nPjxzeXN0ZW0gdmFsdWU9Imh0dHA6Ly9maGlyLmRlL0NvZGVTeXN0ZW0vaWZhL3B6biI+PC9zeXN0ZW0+PGNvZGUgdmFsdWU9IjA2MzEzNzI4Ij48L2NvZGU+PC9jb2Rpbmc+PHRleHQgdmFsdWU9Imdlc3VuZCI+PC90ZXh0PjwvY29kZT48Zm9ybT48Y29kaW5nPjxzeXN0ZW0gdmFsdWU9Imh0dHBzOi8vZmhpci5rYnYuZGUvQ29kZVN5c3RlbS9LQlZfQ1NfU0ZISVJfS0JWX0RBUlJFSUNIVU5HU0ZPUk0iPjwvc3lzdGVtPjxjb2RlIHZhbHVlPSJUQUIiPjwvY29kZT48L2NvZGluZz48L2Zvcm0+PGFtb3VudD48bnVtZXJhdG9yPjx2YWx1ZSB2YWx1ZT0iMTIiPjwvdmFsdWU+PHVuaXQgdmFsdWU9IlRBQiI+PC91bml0PjxzeXN0ZW0gdmFsdWU9Imh0dHA6Ly91bml0c29mbWVhc3VyZS5vcmciPjwvc3lzdGVtPjxjb2RlIHZhbHVlPSJ7dGJsfSI+PC9jb2RlPjwvbnVtZXJhdG9yPjxkZW5vbWluYXRvcj48dmFsdWUgdmFsdWU9IjEiPjwvdmFsdWU+PC9kZW5vbWluYXRvcj48L2Ftb3VudD48L01lZGljYXRpb24+PC9yZXNvdXJjZT48L2VudHJ5PjxlbnRyeT48ZnVsbFVybCB2YWx1ZT0iaHR0cHM6Ly9lLXJlemVwdC5kZS9Db3ZlcmFnZS9kMzdkMjZiNy0wMTZjLTQ0ZWYtOTNiNS1hZDhiZjM4MWExNTgiPjwvZnVsbFVybD48cmVzb3VyY2U+PENvdmVyYWdlIHhtbG5zPSJodHRwOi8vaGw3Lm9yZy9maGlyIj48aWQgdmFsdWU9ImQzN2QyNmI3LTAxNmMtNDRlZi05M2I1LWFkOGJmMzgxYTE1OCI+PC9pZD48bWV0YT48cHJvZmlsZSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9TdHJ1Y3R1cmVEZWZpbml0aW9uL0tCVl9QUl9GT1JfQ292ZXJhZ2V8MS4wLjMiPjwvcHJvZmlsZT48L21ldGE+PGV4dGVuc2lvbiB1cmw9Imh0dHA6Ly9maGlyLmRlL1N0cnVjdHVyZURlZmluaXRpb24vZ2t2L2Jlc29uZGVyZS1wZXJzb25lbmdydXBwZSI+PHZhbHVlQ29kaW5nPjxzeXN0ZW0gdmFsdWU9Imh0dHBzOi8vZmhpci5rYnYuZGUvQ29kZVN5c3RlbS9LQlZfQ1NfU0ZISVJfS0JWX1BFUlNPTkVOR1JVUFBFIj48L3N5c3RlbT48Y29kZSB2YWx1ZT0iMDAiPjwvY29kZT48L3ZhbHVlQ29kaW5nPjwvZXh0ZW5zaW9uPjxleHRlbnNpb24gdXJsPSJodHRwOi8vZmhpci5kZS9TdHJ1Y3R1cmVEZWZpbml0aW9uL2drdi9kbXAta2VubnplaWNoZW4iPjx2YWx1ZUNvZGluZz48c3lzdGVtIHZhbHVlPSJodHRwczovL2ZoaXIua2J2LmRlL0NvZGVTeXN0ZW0vS0JWX0NTX1NGSElSX0tCVl9ETVAiPjwvc3lzdGVtPjxjb2RlIHZhbHVlPSIwMCI+PC9jb2RlPjwvdmFsdWVDb2Rpbmc+PC9leHRlbnNpb24+PGV4dGVuc2lvbiB1cmw9Imh0dHA6Ly9maGlyLmRlL1N0cnVjdHVyZURlZmluaXRpb24vZ2t2L3dvcCI+PHZhbHVlQ29kaW5nPjxzeXN0ZW0gdmFsdWU9Imh0dHBzOi8vZmhpci5rYnYuZGUvQ29kZVN5c3RlbS9LQlZfQ1NfU0ZISVJfSVRBX1dPUCI+PC9zeXN0ZW0+PGNvZGUgdmFsdWU9IjAzIj48L2NvZGU+PC92YWx1ZUNvZGluZz48L2V4dGVuc2lvbj48ZXh0ZW5zaW9uIHVybD0iaHR0cDovL2ZoaXIuZGUvU3RydWN0dXJlRGVmaW5pdGlvbi9na3YvdmVyc2ljaGVydGVuYXJ0Ij48dmFsdWVDb2Rpbmc+PHN5c3RlbSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9Db2RlU3lzdGVtL0tCVl9DU19TRkhJUl9LQlZfVkVSU0lDSEVSVEVOU1RBVFVTIj48L3N5c3RlbT48Y29kZSB2YWx1ZT0iMSI+PC9jb2RlPjwvdmFsdWVDb2Rpbmc+PC9leHRlbnNpb24+PHN0YXR1cyB2YWx1ZT0iYWN0aXZlIj48L3N0YXR1cz48dHlwZT48Y29kaW5nPjxzeXN0ZW0gdmFsdWU9Imh0dHBzOi8vZmhpci5rYnYuZGUvQ29kZVN5c3RlbS9LQlZfQ1NfU0ZISVJfS0JWX1ZFUlNJQ0hFUlRFTlNUQVRVUyI+PC9zeXN0ZW0+PGNvZGUgdmFsdWU9IkdLViI+PC9jb2RlPjwvY29kaW5nPjwvdHlwZT48YmVuZWZpY2lhcnk+PHJlZmVyZW5jZSB2YWx1ZT0iUGF0aWVudC9jN2IwNDk2ZC05NDc0LTQwOGQtOTc3OS0zNDgwZjI0ZjRiMjEiPjwvcmVmZXJlbmNlPjwvYmVuZWZpY2lhcnk+PHBlcmlvZD48ZW5kIHZhbHVlPSIyMDIxLTA2LTA3Ij48L2VuZD48L3BlcmlvZD48cGF5b3I+PGlkZW50aWZpZXI+PHN5c3RlbSB2YWx1ZT0iaHR0cDovL2ZoaXIuZGUvTmFtaW5nU3lzdGVtL2FyZ2UtaWsvaWtuciI+PC9zeXN0ZW0+PHZhbHVlIHZhbHVlPSIwIj48L3ZhbHVlPjwvaWRlbnRpZmllcj48ZGlzcGxheSB2YWx1ZT0idGVzdCI+PC9kaXNwbGF5PjwvcGF5b3I+PC9Db3ZlcmFnZT48L3Jlc291cmNlPjwvZW50cnk+PGVudHJ5PjxmdWxsVXJsIHZhbHVlPSJodHRwczovL2UtcmV6ZXB0LmRlL09yZ2FuaXphdGlvbi82MDAwNDk4Yi05N2E3LTQzYjktYmU3ZS05Nzc1MGIxNzlhZTkiPjwvZnVsbFVybD48cmVzb3VyY2U+PE9yZ2FuaXphdGlvbiB4bWxucz0iaHR0cDovL2hsNy5vcmcvZmhpciI+PGlkIHZhbHVlPSI2MDAwNDk4Yi05N2E3LTQzYjktYmU3ZS05Nzc1MGIxNzlhZTkiPjwvaWQ+PG1ldGE+PHByb2ZpbGUgdmFsdWU9Imh0dHBzOi8vZmhpci5rYnYuZGUvU3RydWN0dXJlRGVmaW5pdGlvbi9LQlZfUFJfRk9SX09yZ2FuaXphdGlvbnwxLjAuMyI+PC9wcm9maWxlPjwvbWV0YT48aWRlbnRpZmllcj48dHlwZT48Y29kaW5nPjxzeXN0ZW0gdmFsdWU9Imh0dHA6Ly90ZXJtaW5vbG9neS5obDcub3JnL0NvZGVTeXN0ZW0vdjItMDIwMyI+PC9zeXN0ZW0+PGNvZGUgdmFsdWU9IlhYIj48L2NvZGU+PC9jb2Rpbmc+PC90eXBlPjxzeXN0ZW0gdmFsdWU9Imh0dHA6Ly9maGlyLmRlL05hbWluZ1N5c3RlbS9hcmdlLWlrL2lrbnIiPjwvc3lzdGVtPjx2YWx1ZSB2YWx1ZT0iMTIzNDU2Ij48L3ZhbHVlPjwvaWRlbnRpZmllcj48dGVsZWNvbT48c3lzdGVtIHZhbHVlPSJwaG9uZSI+PC9zeXN0ZW0+PHZhbHVlIHZhbHVlPSIwMzAxMjM0NTY3Ij48L3ZhbHVlPjwvdGVsZWNvbT48L09yZ2FuaXphdGlvbj48L3Jlc291cmNlPjwvZW50cnk+PGVudHJ5PjxmdWxsVXJsIHZhbHVlPSJodHRwczovL2UtcmV6ZXB0LmRlL1BhdGllbnQvYzdiMDQ5NmQtOTQ3NC00MDhkLTk3NzktMzQ4MGYyNGY0YjIxIj48L2Z1bGxVcmw+PHJlc291cmNlPjxQYXRpZW50IHhtbG5zPSJodHRwOi8vaGw3Lm9yZy9maGlyIj48aWQgdmFsdWU9ImM3YjA0OTZkLTk0NzQtNDA4ZC05Nzc5LTM0ODBmMjRmNGIyMSI+PC9pZD48bWV0YT48cHJvZmlsZSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9TdHJ1Y3R1cmVEZWZpbml0aW9uL0tCVl9QUl9GT1JfUGF0aWVudHwxLjAuMyI+PC9wcm9maWxlPjwvbWV0YT48aWRlbnRpZmllcj48dXNlIHZhbHVlPSJvZmZpY2lhbCI+PC91c2U+PHR5cGU+PGNvZGluZz48c3lzdGVtIHZhbHVlPSJodHRwOi8vZmhpci5kZS9Db2RlU3lzdGVtL2lkZW50aWZpZXItdHlwZS1kZS1iYXNpcyI+PC9zeXN0ZW0+PGNvZGUgdmFsdWU9IkdLViI+PC9jb2RlPjwvY29kaW5nPjwvdHlwZT48c3lzdGVtIHZhbHVlPSJodHRwOi8vZmhpci5kZS9OYW1pbmdTeXN0ZW0vZ2t2L2t2aWQtMTAiPjwvc3lzdGVtPjx2YWx1ZSB2YWx1ZT0iWDExMDUwMjQxNCI+PC92YWx1ZT48L2lkZW50aWZpZXI+PG5hbWU+PHVzZSB2YWx1ZT0ib2ZmaWNpYWwiPjwvdXNlPjxmYW1pbHkgdmFsdWU9Ik1laWVyIj48ZXh0ZW5zaW9uIHVybD0iaHR0cDovL2hsNy5vcmcvZmhpci9TdHJ1Y3R1cmVEZWZpbml0aW9uL2h1bWFubmFtZS1vd24tbmFtZSI+PHZhbHVlU3RyaW5nIHZhbHVlPSJNZWllciI+PC92YWx1ZVN0cmluZz48L2V4dGVuc2lvbj48L2ZhbWlseT48Z2l2ZW4gdmFsdWU9Ik1heCI+PC9naXZlbj48L25hbWU+PGJpcnRoRGF0ZSB2YWx1ZT0iMjAyMS0wNi0wNyI+PC9iaXJ0aERhdGU+PGFkZHJlc3M+PHR5cGUgdmFsdWU9ImJvdGgiPjwvdHlwZT48bGluZSB2YWx1ZT0iTXVzdGVyc3RyLiAxIj48ZXh0ZW5zaW9uIHVybD0iaHR0cDovL2hsNy5vcmcvZmhpci9TdHJ1Y3R1cmVEZWZpbml0aW9uL2lzbzIxMDkwLUFEWFAtc3RyZWV0TmFtZSI+PHZhbHVlU3RyaW5nIHZhbHVlPSJNdXN0ZXJzdHIuIDEiPjwvdmFsdWVTdHJpbmc+PC9leHRlbnNpb24+PC9saW5lPjxjaXR5IHZhbHVlPSJCZXJsaW4iPjwvY2l0eT48cG9zdGFsQ29kZSB2YWx1ZT0iMTA2MjMiPjwvcG9zdGFsQ29kZT48L2FkZHJlc3M+PC9QYXRpZW50PjwvcmVzb3VyY2U+PC9lbnRyeT48ZW50cnk+PGZ1bGxVcmwgdmFsdWU9Imh0dHBzOi8vZS1yZXplcHQuZGUvUHJhY3RpdGlvbmVyLzk0M2RhM2U2LWM0OGItNDg5OS1hNGZiLTc3OWU2YmU2NTU0MyI+PC9mdWxsVXJsPjxyZXNvdXJjZT48UHJhY3RpdGlvbmVyIHhtbG5zPSJodHRwOi8vaGw3Lm9yZy9maGlyIj48aWQgdmFsdWU9Ijk0M2RhM2U2LWM0OGItNDg5OS1hNGZiLTc3OWU2YmU2NTU0MyI+PC9pZD48bWV0YT48cHJvZmlsZSB2YWx1ZT0iaHR0cHM6Ly9maGlyLmtidi5kZS9TdHJ1Y3R1cmVEZWZpbml0aW9uL0tCVl9QUl9GT1JfUHJhY3RpdGlvbmVyfDEuMC4zIj48L3Byb2ZpbGU+PC9tZXRhPjxpZGVudGlmaWVyPjx0eXBlPjxjb2Rpbmc+PHN5c3RlbSB2YWx1ZT0iaHR0cDovL3Rlcm1pbm9sb2d5LmhsNy5vcmcvQ29kZVN5c3RlbS92Mi0wMjAzIj48L3N5c3RlbT48Y29kZSB2YWx1ZT0iTEFOUiI+PC9jb2RlPjwvY29kaW5nPjwvdHlwZT48c3lzdGVtIHZhbHVlPSJodHRwczovL2ZoaXIua2J2LmRlL05hbWluZ1N5c3RlbS9LQlZfTlNfQmFzZV9BTlIiPjwvc3lzdGVtPjx2YWx1ZSB2YWx1ZT0iODM4MzgyMjAyIj48L3ZhbHVlPjwvaWRlbnRpZmllcj48bmFtZT48dXNlIHZhbHVlPSJvZmZpY2lhbCI+PC91c2U+PGZhbWlseSB2YWx1ZT0iVG9wcC1HbMO8Y2tsaWNoIj48ZXh0ZW5zaW9uIHVybD0iaHR0cDovL2hsNy5vcmcvZmhpci9TdHJ1Y3R1cmVEZWZpbml0aW9uL2h1bWFubmFtZS1vd24tbmFtZSI+PHZhbHVlU3RyaW5nIHZhbHVlPSJUb3BwLUdsw7xja2xpY2giPjwvdmFsdWVTdHJpbmc+PC9leHRlbnNpb24+PC9mYW1pbHk+PGdpdmVuIHZhbHVlPSJIYW5zIj48L2dpdmVuPjxwcmVmaXggdmFsdWU9IkRyLiBtZWQuIj48ZXh0ZW5zaW9uIHVybD0iaHR0cDovL2hsNy5vcmcvZmhpci9TdHJ1Y3R1cmVEZWZpbml0aW9uL2lzbzIxMDkwLUVOLXF1YWxpZmllciI+PHZhbHVlQ29kZSB2YWx1ZT0iQUMiPjwvdmFsdWVDb2RlPjwvZXh0ZW5zaW9uPjwvcHJlZml4PjwvbmFtZT48cXVhbGlmaWNhdGlvbj48Y29kZT48Y29kaW5nPjxzeXN0ZW0gdmFsdWU9Imh0dHBzOi8vZmhpci5rYnYuZGUvQ29kZVN5c3RlbS9LQlZfQ1NfRk9SX1F1YWxpZmljYXRpb25fVHlwZSI+PC9zeXN0ZW0+PGNvZGUgdmFsdWU9IjAwIj48L2NvZGU+PC9jb2Rpbmc+PC9jb2RlPjwvcXVhbGlmaWNhdGlvbj48cXVhbGlmaWNhdGlvbj48Y29kZT48dGV4dCB2YWx1ZT0iSGF1c2FyenQiPjwvdGV4dD48L2NvZGU+PC9xdWFsaWZpY2F0aW9uPjwvUHJhY3RpdGlvbmVyPjwvcmVzb3VyY2U+PC9lbnRyeT48ZW50cnk+PGZ1bGxVcmwgdmFsdWU9Imh0dHBzOi8vZS1yZXplcHQuZGUvUHJhY3RpdGlvbmVyUm9sZS9mN2E4N2YxZC0zODgxLTQ1MjQtYWQxMi0yNDM2NjFiYzYzZmEiPjwvZnVsbFVybD48cmVzb3VyY2U+PFByYWN0aXRpb25lclJvbGUgeG1sbnM9Imh0dHA6Ly9obDcub3JnL2ZoaXIiPjxpZCB2YWx1ZT0iZjdhODdmMWQtMzg4MS00NTI0LWFkMTItMjQzNjYxYmM2M2ZhIj48L2lkPjxtZXRhPjxwcm9maWxlIHZhbHVlPSJodHRwczovL2ZoaXIua2J2LmRlL1N0cnVjdHVyZURlZmluaXRpb24vS0JWX1BSX0ZPUl9QcmFjdGl0aW9uZXJSb2xlfDEuMC4zIj48L3Byb2ZpbGU+PC9tZXRhPjxwcmFjdGl0aW9uZXI+PHJlZmVyZW5jZSB2YWx1ZT0iUHJhY3RpdGlvbmVyLzk0M2RhM2U2LWM0OGItNDg5OS1hNGZiLTc3OWU2YmU2NTU0MyI+PC9yZWZlcmVuY2U+PC9wcmFjdGl0aW9uZXI+PG9yZ2FuaXphdGlvbj48aWRlbnRpZmllcj48c3lzdGVtIHZhbHVlPSJodHRwOi8vZmhpci5kZS9OYW1pbmdTeXN0ZW0vYXN2L3RlYW1udW1tZXIiPjwvc3lzdGVtPjx2YWx1ZSB2YWx1ZT0iMDAzNDU2Nzg5Ij48L3ZhbHVlPjwvaWRlbnRpZmllcj48L29yZ2FuaXphdGlvbj48L1ByYWN0aXRpb25lclJvbGU+PC9yZXNvdXJjZT48L2VudHJ5PjwvQnVuZGxlPqCCA7swggO3MIIDXaADAgECAgcDK2rHMCv5MAoGCCqGSM49BAMCMIGpMQswCQYDVQQGEwJERTEfMB0GA1UECgwWZ2VtYXRpayBHbWJIIE5PVC1WQUxJRDEgMB4GA1UEYQwXVVN0LUlkTnIuIERFIDAwOTk3NDU5MzMxNjA0BgNVBAsMLVF1YWxpZml6aWVydGVyIFZEQSBkZXIgVGVsZW1hdGlraW5mcmFzdHJ1a3R1cjEfMB0GA1UEAwwWR0VNLkhCQS1xQ0E2IFRFU1QtT05MWTAeFw0yMDA2MTAwMDAwMDBaFw0yNTA2MDkyMzU5NTlaMHkxCzAJBgNVBAYTAkRFMWowDAYDVQQqDAVLZXZpbjAWBgNVBAQMD0TDpW1tZXItw5hyc3RlZDAbBgNVBAUTFDgwMjc2ODgzMTEwMDAwMTI5MDg3MCUGA1UEAwweS2V2aW4gRMOlbW1lci3DmHJzdGVkVEVTVC1PTkxZMFowFAYHKoZIzj0CAQYJKyQDAwIIAQEHA0IABF43BxVtmg1kdNKofu0JtfmEH9q4zKIVJTNQEtXElBoRjk4k1mEXgu8ykdXOkW2aXrcRjNHqB0HzvUkOywCxqYOjggGcMIIBmDAbBgkrBgEEAcBtAwUEDjAMBgorBgEEAcBtAwUBMCIGCCsGAQUFBwEDBBYwFDAIBgYEAI5GAQEwCAYGBACORgEEMAwGA1UdEwEB/wQCMAAwHwYDVR0jBBgwFoAUJNkKEVsma9+SnhikifS4Sh0vY2wwHQYDVR0OBBYEFHnXHr1JIxrRKHtAY2GzGH6aFWDsMA4GA1UdDwEB/wQEAwIGQDA5BgNVHSAEMjAwMAkGByqCFABMBEgwCQYHBACL7EABAjAKBggqghQATASBETAMBgorBgEEAYLNMwEBMDgGCCsGAQUFBwEBBCwwKjAoBggrBgEFBQcwAYYcaHR0cDovL2VoY2EuZ2VtYXRpay5kZS9vY3NwLzCBgQYFKyQIAwMEeDB2pCgwJjELMAkGA1UEBhMCREUxFzAVBgNVBAoMDmdlbWF0aWsgQmVybGluMEowSDBGMEQwFgwUWmFobsOkcnp0aW4vWmFobmFyenQwCQYHKoIUAEwEHxMfMi1IQkEtVGVzdGthcnRlLTg4MzExMDAwMDEyOTA4NzAKBggqhkjOPQQDAgNIADBFAiAVQ0dvh6/WW2dXyI6WRHUd6AQxvmD/mILCnVnENOOhTgIhAKOFg5z3EWOXWW7LakYJmUDHh18/GmYCB08ORoowBdtCMYICvjCCAroCAQEwgbUwgakxCzAJBgNVBAYTAkRFMR8wHQYDVQQKDBZnZW1hdGlrIEdtYkggTk9ULVZBTElEMSAwHgYDVQRhDBdVU3QtSWROci4gREUgMDA5OTc0NTkzMzE2MDQGA1UECwwtUXVhbGlmaXppZXJ0ZXIgVkRBIGRlciBUZWxlbWF0aWtpbmZyYXN0cnVrdHVyMR8wHQYDVQQDDBZHRU0uSEJBLXFDQTYgVEVTVC1PTkxZAgcDK2rHMCv5MA0GCWCGSAFlAwQCAQUAoIIBlDAYBgkqhkiG9w0BCQMxCwYJKoZIhvcNAQcBMBwGCSqGSIb3DQEJBTEPFw0yMTA2MDcxNTM4MDFaMC0GCSqGSIb3DQEJNDEgMB4wDQYJYIZIAWUDBAIBBQChDQYJKoZIhvcNAQELBQAwLwYJKoZIhvcNAQkEMSIEII3Vbvl4GTpibh+uKht4m5oL9/C19XqdwuPt/R/JfZGqMIH5BgsqhkiG9w0BCRACLzGB6TCB5jCB4zCB4AQgPGmtAcowDcnp2jQnDfQm8X1gK++qelITiLDyr7feJxswgbswga+kgawwgakxCzAJBgNVBAYTAkRFMR8wHQYDVQQKDBZnZW1hdGlrIEdtYkggTk9ULVZBTElEMSAwHgYDVQRhDBdVU3QtSWROci4gREUgMDA5OTc0NTkzMzE2MDQGA1UECwwtUXVhbGlmaXppZXJ0ZXIgVkRBIGRlciBUZWxlbWF0aWtpbmZyYXN0cnVrdHVyMR8wHQYDVQQDDBZHRU0uSEJBLXFDQTYgVEVTVC1PTkxZAgcDK2rHMCv5MA0GCSqGSIb3DQEBCwUABEcwRQIhAJHccw0FrQMiMaVjeTG1N5VblOx3kxo7iDPcc3XQ679VAiBhbc+Wz9VzWrqGbwqbLO4QK6OASjOdzJ+6FUph2n5i+Q==";
    ASSERT_THROW(CadesBesSignature{signedText},
                 CadesBesSignature::VerificationException);
}
#endif


TEST_F(CadesBesSignatureTest, validateOcspResponseInGeneratedCMS)//NOLINT(readability-function-cognitive-complexity)
{
    std::string_view myText = "The text to be signed";
    auto cert = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErp());
    X509Certificate certX509 = X509Certificate::createFromBase64(cert.toBase64Der());
    auto certCA = Certificate::fromPem(CFdSigErpTestHelper::cFdSigErpSigner());
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{CFdSigErpTestHelper::cFdSigErpPrivateKey()});
    const std::string ocspUrl(CFdSigErpTestHelper::cFsSigErpOcspUrl());
    std::shared_ptr<TslManager> tslManager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {ocspUrl, {{cert, certCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    auto ocspResponseData =
        tslManager->getCertificateOcspResponse(TslMode::TSL, certX509, {CertificateType::C_FD_SIG}, false);

    std::string signedText;
    {
        CadesBesSignature cadesBesSignature{
            cert,
            privKey,
            std::string{myText},
            std::nullopt,
            OcspHelper::stringToOcspResponse(ocspResponseData.response)};
        ASSERT_NO_THROW(signedText = Base64::encode(cadesBesSignature.get()));
    }

    const auto plain = Base64::decode(signedText);
    auto bioIndata = shared_BIO::make();
    ASSERT_TRUE(plain.size() <= static_cast<size_t>(std::numeric_limits<int>::max()));
    ASSERT_TRUE(BIO_write(bioIndata.get(), plain.data(), static_cast<int>(plain.size())) ==
                  static_cast<int>(plain.size()));
    shared_CMS_ContentInfo contentInfo = shared_CMS_ContentInfo::make(d2i_CMS_bio(bioIndata.get(), nullptr));
    ASSERT_NE(nullptr, contentInfo);

    const int nidOcspResponseRevocationContainer = OBJ_txt2nid("1.3.6.1.5.5.7.16.2");
    ASSERT_NE(nidOcspResponseRevocationContainer, NID_undef);

    ASN1_OBJECT* formatObject = OBJ_nid2obj(nidOcspResponseRevocationContainer);
    ASSERT_NE(formatObject, nullptr);
    int index = CMS_get_anotherRevocationInfo_by_format(contentInfo, formatObject, -1);
    ASSERT_TRUE(index >= 0);
    ASN1_TYPE* ocspResponseAny = CMS_get0_anotherRevocationInfo(contentInfo, index);
    ASSERT_NE(ocspResponseAny, nullptr);
    int objectType = ASN1_TYPE_get(ocspResponseAny);
    ASSERT_EQ(objectType, V_ASN1_SEQUENCE);
    OcspResponsePtr ocspResponseFromCms(
        reinterpret_cast<OCSP_RESPONSE*>(
            ASN1_TYPE_unpack_sequence(ASN1_ITEM_rptr(OCSP_RESPONSE), ocspResponseAny)));
    ASSERT_NE(ocspResponseFromCms, nullptr);

    const std::string bioOcspResponseFromCms = OcspHelper::ocspResponseToString(*ocspResponseFromCms);
    ASSERT_EQ(ocspResponseData.response, bioOcspResponseFromCms);
}
