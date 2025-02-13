/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include <gtest/gtest.h>// should be first or FRIEND_TEST would not work

#include "shared/crypto/CadesBesSignature.hxx"
#include "shared/crypto/Certificate.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/tsl/OcspHelper.hxx"
#include "shared/tsl/OcspService.hxx"
#include "shared/tsl/TslManager.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/util/Hash.hxx"
#include "shared/util/SafeString.hxx"
#include "mock/tsl/UrlRequestSenderMock.hxx"
#include "test/erp/pc/CFdSigErpTestHelper.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "test/util/CertificateDirLoader.h"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestConfiguration.hxx"


#include "test_config.h"

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

    OcspResponsePtr getEmbeddedOcspResponse(const std::string& cmsBase64Text)
    {
        const auto plain = Base64::decode(cmsBase64Text);
        auto bioIndata = shared_BIO::make();
        Expect(plain.size() <= static_cast<size_t>(std::numeric_limits<int>::max()), "unexpected size");
        Expect(BIO_write(bioIndata.get(), plain.data(), static_cast<int>(plain.size())) ==
                      static_cast<int>(plain.size()), "can not write bio");
        shared_CMS_ContentInfo contentInfo = shared_CMS_ContentInfo::make(d2i_CMS_bio(bioIndata.get(), nullptr));
        Expect(contentInfo != nullptr, "can not create CMS_ContentInfo");

        const int nidOcspResponseRevocationContainer = OBJ_txt2nid("1.3.6.1.5.5.7.16.2");
        if (nidOcspResponseRevocationContainer != NID_undef)
        {
            ASN1_OBJECT* formatObject = OBJ_nid2obj(nidOcspResponseRevocationContainer);
            Expect(formatObject != nullptr, "no format object for OCSP response revocation container");
            int index = CMS_get_anotherRevocationInfo_by_format(contentInfo, formatObject, -1);
            if (index >= 0)
            {
                ASN1_TYPE* ocspResponseAny = CMS_get0_anotherRevocationInfo(contentInfo, index);
                Expect(ocspResponseAny != nullptr, "no ASN1_TYPE object for embedded OCSP response");
                int objectType = ASN1_TYPE_get(ocspResponseAny);
                Expect(objectType == V_ASN1_SEQUENCE, "unexpected object type");
                return OcspResponsePtr(
                    reinterpret_cast<OCSP_RESPONSE*>(
                        ASN1_TYPE_unpack_sequence(ASN1_ITEM_rptr(OCSP_RESPONSE), ocspResponseAny)));
            }
            else
            {
                TLOG(INFO) << "no valid index for revocation info";
            }
        }
        else
        {
            TLOG(INFO) << "no object for OCSP response revocation container";
        }

        return OcspResponsePtr();
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
        ASSERT_NO_THROW(signedText = cms.getBase64());
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
        ASSERT_NO_THROW(signedText = cadesBesSignature.getBase64());
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
    EXPECT_EQ(Base64::encode(messageDigest), referenceDigest);
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
        ASSERT_NO_THROW(signedText = cadesBesSignature.getBase64());
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
            {"http://ehca-testref.komp-ca.telematik-test:8080/status/qocsp",
             {{cert, certCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    std::string signedText;
    {
        CadesBesSignature cadesBesSignature{ cert, privKey, std::string{myText}};
        ASSERT_NO_THROW(signedText = cadesBesSignature.getBase64());
    }
    {
        CadesBesSignature cadesBesSignature{signedText, *manager};
        EXPECT_EQ(cadesBesSignature.payload(), myText);
    }
}


TEST_F(CadesBesSignatureTest, GematikExampleWithOutdatedOcsp)//NOLINT(readability-function-cognitive-complexity)
{
    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>();

    // The contained in CAdES-BES OCSP-Response is outdated, but it is valid for the
    // time of signature creation ( reference time point ), thus it must be accepted
    const auto raw =
        FileHelper::readFileAsString(
            std::string(TEST_DATA_DIR)
            + "/cadesBesSignature/4fe2013d-ae94-441a-a1b1-78236ae65680_S_SECUN_secu_kon_4.8.2_4.1.3.p7s");
    CadesBesSignature cadesBesSignature(raw, *manager);

    ASSERT_FALSE(cadesBesSignature.payload().empty());
}


TEST_F(CadesBesSignatureTest, GematikExampleCounterSignature)//NOLINT(readability-function-cognitive-complexity)
{
    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>();

    // The contained in CAdES-BES OCSP-Response is outdated, but it is valid for the
    // time of signature creation ( reference time point ), thus it must be accepted
    const auto raw =
        FileHelper::readFileAsString(
            std::string(TEST_DATA_DIR)
            + "/cadesBesSignature/4fe2013d-ae94-441a-a1b1-78236ae65680_S_SECUN_secu_kon_4.8.2_4.1.3.p7s");
    CadesBesSignature cadesBesSignature(raw);

    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{privateKey});
    auto cert = Certificate::fromPem(certificate);
    cadesBesSignature.addCounterSignature(cert, privKey);

    const std::string signedText = cadesBesSignature.getBase64();

    // load the serialized CAdES-BES
    {
        const CadesBesSignature resultCadesBesSignature{signedText};
        ASSERT_FALSE(resultCadesBesSignature.payload().empty());
        ASSERT_NO_THROW(resultCadesBesSignature.validateCounterSignature(cert));
    }
}


TEST_F(CadesBesSignatureTest, validateGematik)
{
    // test certificates for "Vorläuferkarten", which is handled as CertificateType::C_HP_ENC internally
    // and has certificate policy oid_vk_eaa_enc (oid 1.3.6.1.4.1.24796.1.10)
    // cf ERP-6078
    std::string_view myText = "The text to be signed";
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(
        SafeString{FileHelper::readFileAsString(ResourceManager::getAbsoluteFilename(
            "test/generated_pki/sub_ca1_ec/certificates/qes_cert_ec_hp_enc/qes_cert_ec_hp_enc_key.pem"))});

    auto cert = Certificate::fromBinaryDer(FileHelper::readFileAsString(ResourceManager::getAbsoluteFilename(
        "test/generated_pki/sub_ca1_ec/certificates/qes_cert_ec_hp_enc/qes_cert_ec_hp_enc.der")));

    auto certCA = Certificate::fromBinaryDer(
        FileHelper::readFileAsString(ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/ca.der")));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {}, {}, {{"http://ocsp.test.ibm.de/", {{cert, certCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    std::weak_ptr<TslManager> mgrWeakPtr{manager};
    manager->addPostUpdateHook([mgrWeakPtr, certCA] {
        auto tslManager = mgrWeakPtr.lock();
        if (! tslManager)
            return;
        TslTestHelper::addCaCertificateToTrustStore(certCA, *tslManager, TslMode::BNA);
    });
    std::string signedText;
    {
        CadesBesSignature cadesBesSignature{cert, privKey, std::string{myText}};
        ASSERT_NO_THROW(signedText = cadesBesSignature.getBase64());
    }
    {
        CadesBesSignature cadesBesSignature{signedText, *manager, true};
        EXPECT_EQ(cadesBesSignature.payload(), myText);
    }
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
        ASSERT_NO_THROW(signedText = cadesBesSignature.getBase64());
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

    const std::string cmsWithoutCertificateAttributes = "MIIGAgYJKoZIhvcNAQcCoIIF8zCCBe8CAQExDTALBglghkgBZQMEAgEwJAYJKoZIhvcNAQcBoBcEFVRoZSB0ZXh0IHRvIGJlIHNpZ25lZKCCA6gwggOkMIIDS6ADAgECAgcAh2dSTrvGMAoGCCqGSM49BAMCMIGpMQswCQYDVQQGEwJERTEfMB0GA1UECgwWZ2VtYXRpayBHbWJIIE5PVC1WQUxJRDEgMB4GA1UEYQwXVVN0LUlkTnIuIERFIDAwOTk3NDU5MzMxNjA0BgNVBAsMLVF1YWxpZml6aWVydGVyIFZEQSBkZXIgVGVsZW1hdGlraW5mcmFzdHJ1a3R1cjEfMB0GA1UEAwwWR0VNLkhCQS1xQ0E2IFRFU1QtT05MWTAeFw0yMDA2MTAwMDAwMDBaFw0yNTA2MDkyMzU5NTlaMHAxCzAJBgNVBAYTAkRFMWEwDAYDVQQEDAVPdMOtczAUBgNVBCoMDUfDvG50aGVyIEdyYWYwGwYDVQQFExQ4MDI3Njg4MzExMDAwMDEyOTA4NDAeBgNVBAMMF0fDvG50aGVyIE90w61zVEVTVC1PTkxZMFowFAYHKoZIzj0CAQYJKyQDAwIIAQEHA0IABFaUpx9bSbrz9MRPwR5FMgMrPqODxbmA7ncuBwZW4fY5SCRIbf4ry4+cqcEIHW4mHtlHgKcZUoGWykM0I63uqxijggGTMIIBjzAbBgkrBgEEAcBtAwUEDjAMBgorBgEEAcBtAwUBMCIGCCsGAQUFBwEDBBYwFDAIBgYEAI5GAQEwCAYGBACORgEEMAwGA1UdEwEB/wQCMAAwHwYDVR0jBBgwFoAUJNkKEVsma9+SnhikifS4Sh0vY2wwHQYDVR0OBBYEFFr40X2KgAyz9cMYwfTO7Om8BipjMA4GA1UdDwEB/wQEAwIGQDA5BgNVHSAEMjAwMAkGByqCFABMBEgwCQYHBACL7EABAjAKBggqghQATASBETAMBgorBgEEAYLNMwEBMDgGCCsGAQUFBwEBBCwwKjAoBggrBgEFBQcwAYYcaHR0cDovL2VoY2EuZ2VtYXRpay5kZS9vY3NwLzB5BgUrJAgDAwRwMG6kKDAmMQswCQYDVQQGEwJERTEXMBUGA1UECgwOZ2VtYXRpayBCZXJsaW4wQjBAMD4wPDAODAzDhHJ6dGluL0FyenQwCQYHKoIUAEwEHhMfMS1IQkEtVGVzdGthcnRlLTg4MzExMDAwMDEyOTA4NDAKBggqhkjOPQQDAgNHADBEAiA6c9EYWI5DBbvKXStG93jxm58GECVMJQ1Du83LmNu36wIgFz+S1r9AdB2Ce5OQA2M+B1YA5f2svqvG5qf6z46lfBQxggIHMIICAwIBATCBtTCBqTELMAkGA1UEBhMCREUxHzAdBgNVBAoMFmdlbWF0aWsgR21iSCBOT1QtVkFMSUQxIDAeBgNVBGEMF1VTdC1JZE5yLiBERSAwMDk5NzQ1OTMzMTYwNAYDVQQLDC1RdWFsaWZpemllcnRlciBWREEgZGVyIFRlbGVtYXRpa2luZnJhc3RydWt0dXIxHzAdBgNVBAMMFkdFTS5IQkEtcUNBNiBURVNULU9OTFkCBwCHZ1JOu8YwCwYJYIZIAWUDBAIBoIHkMBgGCSqGSIb3DQEJAzELBgkqhkiG9w0BBwEwHAYJKoZIhvcNAQkFMQ8XDTIxMDUwNjEyNTc0M1owLwYJKoZIhvcNAQkEMSIEIH4kaHm4Mu/DR6mT4ccZ3BQ7CrNB6SqY4NpgXBYKM+1XMHkGCSqGSIb3DQEJDzFsMGowCwYJYIZIAWUDBAEqMAsGCWCGSAFlAwQBFjALBglghkgBZQMEAQIwCgYIKoZIhvcNAwcwDgYIKoZIhvcNAwICAgCAMA0GCCqGSIb3DQMCAgFAMAcGBSsOAwIHMA0GCCqGSIb3DQMCAgEoMAoGCCqGSM49BAMCBEYwRAIgU3KrAzs+V7k3F/7vot/7yV/pygylrkKNspjzeydu9E0CIAalh9vLHfO3XJDamyJALp8eNK4Dw9Wyn+ZDIF/AbAM9";
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
    std::weak_ptr<TslManager> mgrWeakPtr{manager};
    manager->addPostUpdateHook([mgrWeakPtr, certCA] {
        auto tslManager = mgrWeakPtr.lock();
        if (! tslManager)
            return;
        TslTestHelper::addCaCertificateToTrustStore(certCA, *tslManager, TslMode::BNA);
    });

    std::string signedText;
    {
        CadesBesSignature cadesBesSignature{ cert, privKey, std::string{myText}};
        ASSERT_NO_THROW(signedText = cadesBesSignature.getBase64());
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
    std::weak_ptr<TslManager> mgrWeakPtr{manager};
    manager->addPostUpdateHook([mgrWeakPtr, certCA] {
        auto tslManager = mgrWeakPtr.lock();
        if (! tslManager)
            return;
        TslTestHelper::addCaCertificateToTrustStore(certCA, *tslManager, TslMode::BNA);
    });

    std::string signedText;
    {
        CadesBesSignature cadesBesSignature{ cert, privKey, std::string{myText}};
        ASSERT_NO_THROW(signedText = cadesBesSignature.getBase64());
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
        tslManager->getCertificateOcspResponse(
            TslMode::TSL,
            certX509,
            {CertificateType::C_FD_OSIG},
            TslTestHelper::getDefaultTestOcspCheckDescriptor());

    std::string signedText;
    {
        CadesBesSignature cadesBesSignature{
            cert,
            privKey,
            std::string{myText},
            std::nullopt,
            OcspHelper::stringToOcspResponse(ocspResponseData.response)};
        ASSERT_NO_THROW(signedText = cadesBesSignature.getBase64());
    }

    OcspResponsePtr ocspResponseFromCms;
    ASSERT_NO_THROW(ocspResponseFromCms = getEmbeddedOcspResponse(signedText));
    ASSERT_NE(ocspResponseFromCms, nullptr);

    const std::string bioOcspResponseFromCms = OcspHelper::ocspResponseToString(*ocspResponseFromCms);
    ASSERT_EQ(ocspResponseData.response, bioOcspResponseFromCms);
}


TEST_F(CadesBesSignatureTest, noProvidedOcspResponseInCms)
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
            {"http://ehca-testref.komp-ca.telematik-test:8080/status/qocsp",
             {{cert, certCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    std::string signedText;
    {
        CadesBesSignature cadesBesSignature{ cert, privKey, std::string{myText}};
        ASSERT_NO_THROW(signedText = cadesBesSignature.getBase64());
    }

    {
        // created cadesBesSignature does not contain embedded OCSP-Response
        OcspResponsePtr ocspResponseFromCms;
        ASSERT_NO_THROW(ocspResponseFromCms = getEmbeddedOcspResponse(signedText));
        ASSERT_EQ(ocspResponseFromCms, nullptr);

        // the reloading should embed OCSP-Response to the package
        CadesBesSignature cadesBesSignature{signedText, *manager};
        ASSERT_NO_THROW(ocspResponseFromCms = getEmbeddedOcspResponse(cadesBesSignature.getBase64()));
        ASSERT_NE(ocspResponseFromCms, nullptr);

        // validate embedded OCSP response
        auto x509Certificate = X509Certificate::createFromBase64(cert.toBase64Der());
        auto ocspResponseData =
            manager->getCertificateOcspResponse(
                TslMode::BNA,
                x509Certificate,
                {CertificateType::C_HP_QES},
                TslTestHelper::getDefaultTestOcspCheckDescriptor());
        const std::string bioOcspResponseFromCms = OcspHelper::ocspResponseToString(*ocspResponseFromCms);
        ASSERT_EQ(ocspResponseData.response, bioOcspResponseFromCms);
    }
}


TEST_F(CadesBesSignatureTest, validateSmcBOsig)
{
    std::string_view myText = "The text to be signed";
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{
        FileHelper::readFileAsString(
            ResourceManager::getAbsoluteFilename(
                "test/generated_pki/sub_ca1_ec/certificates/smc_b_osig_ec/smc_b_osig_ec_key.pem"))});

    auto cert = Certificate::fromBinaryDer(FileHelper::readFileAsString(
        ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/certificates/smc_b_osig_ec/smc_b_osig_ec.der")));

    auto certCA = Certificate::fromBinaryDer(FileHelper::readFileAsString(
        ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/ca.der")));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ocsp-testref.tsl.telematik-test/ocsp",
             {{cert, certCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    std::string signedText;
    {
        CadesBesSignature cadesBesSignature{ cert, privKey, std::string{myText}};
        ASSERT_NO_THROW(signedText = cadesBesSignature.getBase64());
    }
    {
        CadesBesSignature cadesBesSignature{signedText, *manager, true};
        EXPECT_EQ(cadesBesSignature.payload(), myText);
    }
}


TEST_F(CadesBesSignatureTest, noProvidedOcspResponseInCmsWithSmcBOsig)
{
    std::string_view myText = "The text to be signed";
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{
        FileHelper::readFileAsString(
            ResourceManager::getAbsoluteFilename(
                "test/generated_pki/sub_ca1_ec/certificates/smc_b_osig_ec/smc_b_osig_ec_key.pem"))});

    auto cert = Certificate::fromBinaryDer(FileHelper::readFileAsString(
        ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/certificates/smc_b_osig_ec/smc_b_osig_ec.der")));

    auto certCA = Certificate::fromBinaryDer(FileHelper::readFileAsString(
        ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/ca.der")));

    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(
        {},
        {},
        {
            {"http://ocsp-testref.tsl.telematik-test/ocsp",
             {{cert, certCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}}});

    std::string signedText;
    {
        CadesBesSignature cadesBesSignature{ cert, privKey, std::string{myText}};
        ASSERT_NO_THROW(signedText = cadesBesSignature.getBase64());
    }

    {
        // created cadesBesSignature does not contain embedded OCSP-Response
        OcspResponsePtr ocspResponseFromCms;
        ASSERT_NO_THROW(ocspResponseFromCms = getEmbeddedOcspResponse(signedText));
        ASSERT_EQ(ocspResponseFromCms, nullptr);

        // the reloading should embed OCSP-Response to the package
        CadesBesSignature cadesBesSignature{signedText, *manager, true};
        ASSERT_NO_THROW(ocspResponseFromCms = getEmbeddedOcspResponse(cadesBesSignature.getBase64()));
        ASSERT_NE(ocspResponseFromCms, nullptr);

        // validate embedded OCSP response
        auto x509Certificate = X509Certificate::createFromBase64(cert.toBase64Der());
        auto ocspResponseData =
            manager->getCertificateOcspResponse(
                TslMode::TSL,
                x509Certificate,
                {CertificateType::C_HCI_OSIG},
                TslTestHelper::getDefaultTestOcspCheckDescriptor());
        const std::string bioOcspResponseFromCms = OcspHelper::ocspResponseToString(*ocspResponseFromCms);
        ASSERT_EQ(ocspResponseData.response, bioOcspResponseFromCms);
    }
}


TEST_F(CadesBesSignatureTest, ocspRequestFailure)
{
    // in case of an ocsp request failure we expect the passed
    std::string_view myText = "The text to be signed";
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.prv.pem")});

    auto cert = Certificate::fromPem(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem"));

    auto certCA = Certificate::fromBase64Der(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-Issuer.base64.der"));

    const std::string ocspUrl{"http://ehca-testref.komp-ca.telematik-test:8080/status/qocsp"};
    std::shared_ptr<UrlRequestSenderMock> requestSender =
        CFdSigErpTestHelper::createRequestSender<UrlRequestSenderMock>();


    std::shared_ptr<TslManager> manager = TslTestHelper::createTslManager<TslManager>(requestSender, {}, {});

    std::string signedText;
    {
        // create the cadesbes signature without the ocsp response
        CadesBesSignature cadesBesSignature{cert, privKey, std::string{myText}};
        ASSERT_NO_THROW(signedText = cadesBesSignature.getBase64());
    }

    std::vector<std::function<ClientResponse(const std::string&)>> ocspHandlers = {
        [](const std::string&) -> ClientResponse {
            Header header;
            header.setStatus(HttpStatus::NetworkConnectTimeoutError);
            header.setContentLength(0);
            return {header, ""};
        },
        [](const std::string&) -> ClientResponse {
            throw std::runtime_error{"test error"};
        }};
    for (const auto& handler :ocspHandlers)
    {
        requestSender->setUrlHandler(ocspUrl, handler);
        try
        {
            CadesBesSignature cadesBesSignature{signedText, *manager};
            ADD_FAILURE() << "should throw an exception";
        }
        catch (const TslError& e)
        {
            EXPECT_EQ(e.getHttpStatus(), HttpStatus::BackendCallFailed);
            EXPECT_EQ(e.getErrorData().at(0).errorCode, TslErrorCode::OCSP_NOT_AVAILABLE);
        }
        catch (...)
        {
            ADD_FAILURE() << "unexpected exception";
        }
    }
}

TEST_F(CadesBesSignatureTest, getTelematikId)
{
    model::TelematikId referenceTelematikId{"2-HBA-Testkarte-883110000129087"};
    CadesBesSignature cadesBesSignature(
        Base64::encode(FileHelper::readFileAsString(::std::string(TEST_DATA_DIR) + "/issues/ERP-5822/kbv_bundle.p7s")));
    std::optional<model::TelematikId> telematikId;
    ASSERT_NO_THROW(telematikId = cadesBesSignature.getTelematikId());
    EXPECT_EQ(referenceTelematikId.id(), telematikId->id());
}

TEST_F(CadesBesSignatureTest, getTelematikId2)
{
    model::TelematikId referenceTelematikId{"1-HBA-Testkarte-883110000129084"};
    std::string_view myText = "The text to be signed";
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.prv.pem")});
    auto cert = Certificate::fromPem(FileHelper::readFileAsString(
        std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/80276883110000129084-C_HP_QES_E256.pem"));
    std::string signedText;
    {
        CadesBesSignature cms{cert, privKey, std::string{myText}};
        ASSERT_NO_THROW(signedText = cms.getBase64());
    }
    {
        CadesBesSignature cms{ signedText};
        std::optional<model::TelematikId> telematikId;
        ASSERT_NO_THROW(telematikId = cms.getTelematikId());
        EXPECT_EQ(telematikId->id(), referenceTelematikId.id());
    }
}

TEST_F(CadesBesSignatureTest, getSignerSerialNumber)
{
    const auto content = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/qes.pem");
    auto cert = Certificate::fromPem(content);
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{content.c_str()});

    std::string_view myText = "The text to be signed";
    CadesBesSignature cadesBesSignature{cert, privKey, std::string{myText}};
    EXPECT_EQ(cadesBesSignature.getSignerSerialNumber(), "80276883110000129084");
}
