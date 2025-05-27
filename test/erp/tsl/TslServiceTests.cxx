/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include <gtest/gtest.h>// should be first or FRIEND_TEST would not work

#include "shared/crypto/Certificate.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/tsl/OcspService.hxx"
#include "shared/tsl/TrustStore.hxx"
#include "shared/tsl/TslService.hxx"
#include "shared/tsl/error/TslError.hxx"
#include "shared/util/FileHelper.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "mock/tsl/MockOcsp.hxx"
#include "mock/tsl/UrlRequestSenderMock.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/ResourceManager.hxx"

#include <date/date.h>
#include <test_config.h>
#include <chrono>
#include <unordered_map>


class TslServiceTest : public testing::Test
{
public:
    std::unique_ptr<TrustStore> mTrustStore;
    std::unique_ptr<EnvironmentVariableGuard> mCaDerPathGuard;
    std::string userCertificate;

    void SetUp() override
    {
        userCertificate =
            FileHelper::readFileAsString(
	       	ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/certificates/smc_b_osig_ec/smc_b_osig_ec.der"));
        mCaDerPathGuard = std::make_unique<EnvironmentVariableGuard>(
            "ERP_TSL_INITIAL_CA_DER_PATH",
            ResourceManager::getAbsoluteFilename("test/generated_pki/sub_ca1_ec/ca.der"));

        mTrustStore = TslTestHelper::createTslTrustStore();
    }

    void TearDown() override
    {
        mCaDerPathGuard.reset();
        mTrustStore.reset();
    }
};


TEST_F(TslServiceTest, providedTsl)
{
    const std::unordered_map<std::string, std::string> expectedBnaOcspMapping {
        {"http://ehca.gematik.de/ocsp/", "http://ehca-testref.komp-ca.telematik-test:8080/status/qocsp"},
        {"http://qocsp-ePTA.testumgebung.dgnservice.de:8080/ocsp", "http://titu-qocsp-hba.medisign.tsp-hba.telematik-test:8080/ocsp"},
        {"http://qocsp-zod2.testumgebung.dgnservice.de:8080/ocsp", "http://titu-qocsp-hba.medisign.tsp-hba.telematik-test:8080/ocsp"},
        {"http://ocsp.pki.telematik-test:8080/CMOCSP/OCSP", "http://ehca-testref.komp-ca.telematik-test:8080/status/qocsp"},
        {"http://qocsp-eA.testumgebung.dgnservice.de:8080/ocsp", "http://titu-qocsp-hba.medisign.tsp-hba.telematik-test:8080/ocsp"},
        {"http://ocsp-qes.egk-test-tsp.de", "http://ocsp-qes-testref.atos.tsp-hba.telematik-test"},
        {"http://qca10-qocsp-hba.testumgebung.dgnservice.de:8080/ocsp", "http://titu-qocsp-hba.medisign.tsp-hba.telematik-test:8080/ocsp"},
        {"http://ehca.gematik.de/ecc-qocsp", "http://ehca-testref.komp-ca.telematik-test:8080/status/ecc-qocsp"},
        {"http://qocsp-eZAA.testumgebung.dgnservice.de:8080/ocsp", "http://titu-qocsp-hba.medisign.tsp-hba.telematik-test:8080/ocsp"},
        {"http://qocsp.hba.test.telesec.de/ocspr", "http://qocsp-hba-testref.tsi.tsp-hba.telematik-test/ocspr"},
        {"http://qocsp-hba.testumgebung.dgnservice.de:8080/ocsp", "http://titu-qocsp-hba.medisign.tsp-hba.telematik-test:8080/ocsp"},
        {"http://staging.ocsp.d-trust.net", "http://ocsp.bdr.tsp-hba.telematik-test"}
    };

    EXPECT_FALSE(mTrustStore->mServiceInformationMap.empty());
    EXPECT_EQ(expectedBnaOcspMapping, mTrustStore->getBnaOcspMapping());
}


TEST_F(TslServiceTest, verifyCertificateRevokedCAFailing)//NOLINT(readability-function-cognitive-complexity)
{
    UrlRequestSenderMock requestSender({});
    X509Certificate certificate = X509Certificate::createFromAsnBytes({ reinterpret_cast<const unsigned char*>(userCertificate.data()), userCertificate.size() });

    auto iterator = mTrustStore->mServiceInformationMap.find(
        {certificate.getIssuer(), certificate.getAuthorityKeyIdentifier()});
    ASSERT_NE(mTrustStore->mServiceInformationMap.end(), iterator);
    iterator->second.serviceAcceptanceHistory = {
        {date::sys_days{date::June/1/2020}, true},
        {date::sys_days{date::June/9/2020}, false}
    };

    TslTestHelper::setOcspUrlRequestHandler(
        requestSender,
        "http://ocsp-testref.tsl.telematik-test/ocsp",
        {{Certificate::fromBinaryDer(userCertificate),
          Certificate::fromBase64Der(iterator->second.certificate.toBase64()),
          MockOcsp::CertificateOcspTestMode::SUCCESS}});

    EXPECT_TSL_ERROR_THROW(
        TslService::checkCertificate(certificate, {}, requestSender, *mTrustStore, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
        {TslErrorCode::CA_CERTIFICATE_REVOKED_IN_TSL},
        HttpStatus::BadRequest);
}


TEST_F(TslServiceTest, verifyCertificateValidThenRevokedCASuccess)
{
    UrlRequestSenderMock requestSender({});
    X509Certificate certificate = X509Certificate::createFromAsnBytes({ reinterpret_cast<const unsigned char*>(userCertificate.data()), userCertificate.size() });

    auto iterator = mTrustStore->mServiceInformationMap.find(
        {certificate.getIssuer(), certificate.getAuthorityKeyIdentifier()});
    ASSERT_NE(mTrustStore->mServiceInformationMap.end(), iterator);
    iterator->second.serviceAcceptanceHistory = {
        {date::sys_days{date::June/10/2020}, true},
        {date::sys_days{date::June/10/2032}, false}
    };

    TslTestHelper::setOcspUrlRequestHandler(
        requestSender,
        "http://ocsp-testref.tsl.telematik-test/ocsp",
        {{Certificate::fromBinaryDer(userCertificate),
          Certificate::fromBase64Der(iterator->second.certificate.toBase64()),
          MockOcsp::CertificateOcspTestMode::SUCCESS}});

    EXPECT_NO_THROW(
        TslService::checkCertificate(certificate, {}, requestSender, *mTrustStore, TslTestHelper::getDefaultTestOcspCheckDescriptor()));
}


TEST_F(TslServiceTest, verifyCertificatePolicyNoRestrictionsSuccessful)
{
    UrlRequestSenderMock requestSender({});
    X509Certificate certificate = X509Certificate::createFromAsnBytes({ reinterpret_cast<const unsigned char*>(userCertificate.data()), userCertificate.size() });

    auto iterator = mTrustStore->mServiceInformationMap.find(
        {certificate.getIssuer(), certificate.getAuthorityKeyIdentifier()});
    ASSERT_NE(mTrustStore->mServiceInformationMap.end(), iterator);

    TslTestHelper::setOcspUrlRequestHandler(
        requestSender,
        "http://ocsp-testref.tsl.telematik-test/ocsp",
        {{Certificate::fromBinaryDer(userCertificate),
          Certificate::fromBase64Der(iterator->second.certificate.toBase64()),
          MockOcsp::CertificateOcspTestMode::SUCCESS}});

    EXPECT_NO_THROW(
        TslService::checkCertificate(certificate, {}, requestSender, *mTrustStore, TslTestHelper::getDefaultTestOcspCheckDescriptor()));
}


TEST_F(TslServiceTest, verifyCertificatePolicySuccessful)
{
    UrlRequestSenderMock requestSender({});
    X509Certificate certificate = X509Certificate::createFromAsnBytes({ reinterpret_cast<const unsigned char*>(userCertificate.data()), userCertificate.size() });

    auto iterator = mTrustStore->mServiceInformationMap.find(
        {certificate.getIssuer(), certificate.getAuthorityKeyIdentifier()});
    ASSERT_NE(mTrustStore->mServiceInformationMap.end(), iterator);

    TslTestHelper::setOcspUrlRequestHandler(
        requestSender,
        "http://ocsp-testref.tsl.telematik-test/ocsp",
        {{Certificate::fromBinaryDer(userCertificate),
          Certificate::fromBase64Der(iterator->second.certificate.toBase64()),
          MockOcsp::CertificateOcspTestMode::SUCCESS}});

    EXPECT_NO_THROW(
        TslService::checkCertificate(certificate, {CertificateType::C_HCI_OSIG}, requestSender, *mTrustStore, TslTestHelper::getDefaultTestOcspCheckDescriptor()));
}


TEST_F(TslServiceTest, verifyCertificatePolicyFailing)//NOLINT(readability-function-cognitive-complexity)
{
    UrlRequestSenderMock requestSender({});
    X509Certificate certificate = X509Certificate::createFromBase64("MIIC4TCCAoegAwIBAgIRAK+JCZiHcke3vWXmfPAbuf0wCgYIKoZIzj0EAwIwgYQxIDAeBgNVBAMMF0FDTE9TLktPTVAtQ0EgVEVTVC1PTkxZMTIwMAYDVQQLDClLb21wb25lbnRlbi1DQSBkZXIgVGVsZW1hdGlraW5mcmFzdHJ1a3R1cjEfMB0GA1UECgwWYWNoZWxvcyBHbWJIIE5PVC1WQUxJRDELMAkGA1UEBhMCREUwHhcNMjEwMzExMjMwMDAwWhcNMjMwMzEwMjMwMDAwWjBJMRIwEAYDVQQDDAlJRFAgU2lnIDMxJjAkBgNVBAoMHWdlbWF0aWsgVEVTVC1PTkxZIC0gTk9ULVZBTElEMQswCQYDVQQGEwJERTBaMBQGByqGSM49AgEGCSskAwMCCAEBBwNCAASGGZ8IhgJ+UGDseN2fDMGmSw96/28+gU3EDuaW3rpuqkM2A+XQwnK4sP1TTz2QkM10D4OErVssuTjqvU6IARJjo4IBETCCAQ0wHQYDVR0OBBYEFBZf3Tqupyv/MA5EAvsybQktzVo2MA4GA1UdDwEB/wQEAwIHgDAMBgNVHRMBAf8EAjAAMFIGA1UdIARLMEkwOwYIKoIUAEwEgSMwLzAtBggrBgEFBQcCARYhaHR0cDovL3d3dy5nZW1hdGlrLmRlL2dvL3BvbGljaWVzMAoGCCqCFABMBIFLMEYGCCsGAQUFBwEBBDowODA2BggrBgEFBQcwAYYqaHR0cDovL29jc3AtdGVzdC5vY3NwLnRlbGVtYXRpay10ZXN0OjgwODAvMB8GA1UdIwQYMBaAFITGAi50Bw/bE2uIb0TMdzHGff70MBEGBSskCAMDBAgwBjAEMAIwADAKBggqhkjOPQQDAgNIADBFAiEAilBbiEWbA8n6l3V5iIV/DgJjRVusNpyKCAyKA7Q1kEMCIDwVNy+ORXpKrLNB+YyQQHHAf+UCf4wp71GJp1KFskwn");
    EXPECT_TSL_ERROR_THROW(
        TslService::checkCertificate(certificate, {CertificateType::C_FD_SIG}, requestSender, *mTrustStore, TslTestHelper::getDefaultTestOcspCheckDescriptor()),
        {TslErrorCode::CERT_TYPE_MISMATCH},
        HttpStatus::BadRequest);
}
