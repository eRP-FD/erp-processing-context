/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include <gtest/gtest.h>// should be first or FRIEND_TEST would not work

#include "erp/crypto/Certificate.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/tsl/OcspService.hxx"
#include "erp/tsl/TrustStore.hxx"
#include "erp/tsl/TslService.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/util/FileHelper.hxx"
#include "test/erp/tsl/TslTestHelper.hxx"
#include "mock/tsl/MockOcsp.hxx"
#include "mock/tsl/UrlRequestSenderMock.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/ResourceManager.hxx"

#include <date/date.h>
#include <test_config.h>
#include <chrono>
#include <unordered_map>


namespace
{
    constexpr const char* userCertificate =
        "MIIFcTCCBFmgAwIBAgIHAXumDkbX3zANBgkqhkiG9w0BAQsFADCBmjELMAkGA1UE"
        "BhMCREUxHzAdBgNVBAoMFmdlbWF0aWsgR21iSCBOT1QtVkFMSUQxSDBGBgNVBAsM"
        "P0luc3RpdHV0aW9uIGRlcyBHZXN1bmRoZWl0c3dlc2Vucy1DQSBkZXIgVGVsZW1h"
        "dGlraW5mcmFzdHJ1a3R1cjEgMB4GA1UEAwwXR0VNLlNNQ0ItQ0EyNCBURVNULU9O"
        "TFkwHhcNMjAwNjEwMDAwMDAwWhcNMjUwNjA5MjM1OTU5WjCB+DELMAkGA1UEBhMC"
        "REUxFDASBgNVBAcMC03DvGhsaGF1c2VuMQ4wDAYDVQQRDAU5OTk3NDEeMBwGA1UE"
        "CQwVTGFuZ2Vuc2FsemFlciBTdHIuIDI1MSowKAYDVQQKDCEzLVNNQy1CLVRlc3Rr"
        "YXJ0ZS04ODMxMTAwMDAxMjkwNjgxHTAbBgNVBAUTFDgwMjc2ODgzMTEwMDAwMTI5"
        "MDY4MRQwEgYDVQQEDAtCbGFua2VuYmVyZzEWMBQGA1UEKgwNRG9taW5pay1QZXRl"
        "cjEqMCgGA1UEAwwhQXBvdGhla2UgYW0gU3BvcnR6ZW50cnVtVEVTVC1PTkxZMIIB"
        "IjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAjVMEf2TTXlfkuCDyiMpo96jA"
        "5XRvkaHTy+4qTcDR1awUP4yemfKsB1BTWMMSDrA1/2YdnlZJeynEnQi0K4LWMvTc"
        "q+CRGi4ghcIokb2TURZXZ1K6FTJHqITojp9ZRaNTap+kIpOZCmSRa7ftRzEgooPj"
        "G6C+7XxUViczlVE17UJMPavWQfY2+A1M/0vx9Jbi7wPmXCMuEvj7yEAVRCGQExVx"
        "zbLZPE7FS/vlXcwkFtmnMUVWiQFFXlVLG7uUc9CQFvTXPT5ppDhxAmVeUBLNXKru"
        "DkpSeuq3sCi93ln9hXXw/xPeNAAehtvxFp6eMGf5LEVGvZj8v51qu4eDPaKtJwID"
        "AQABo4IBWjCCAVYwEwYDVR0lBAwwCgYIKwYBBQUHAwIwDgYDVR0PAQH/BAQDAgWg"
        "MB0GA1UdDgQWBBSEkJ1lgmhiHfVZyKKyVw2Qd86PPDA4BggrBgEFBQcBAQQsMCow"
        "KAYIKwYBBQUHMAGGHGh0dHA6Ly9laGNhLmdlbWF0aWsuZGUvb2NzcC8wDAYDVR0T"
        "AQH/BAIwADAgBgNVHSAEGTAXMAoGCCqCFABMBIEjMAkGByqCFABMBE0wHwYDVR0j"
        "BBgwFoAUeunhb+oUWRYF7gPp0/0hq97p2Z4wgYQGBSskCAMDBHsweaQoMCYxCzAJ"
        "BgNVBAYTAkRFMRcwFQYDVQQKDA5nZW1hdGlrIEJlcmxpbjBNMEswSTBHMBcMFcOW"
        "ZmZlbnRsaWNoZSBBcG90aGVrZTAJBgcqghQATAQ2EyEzLVNNQy1CLVRlc3RrYXJ0"
        "ZS04ODMxMTAwMDAxMjkwNjgwDQYJKoZIhvcNAQELBQADggEBAGwmbkXMdRrIZwTz"
        "UVsdH6RUB7cc3+CcDN0NqLSOM7sdCQrr5NfzcK2dzhc77KVzviZbvz6MxfEq47Y/"
        "dPMmtVlU0Amw5bbnYT4WnadjrLOHnKCxLFssrfo0izB7IJvBswMQl/KnUXbk/X57"
        "KcNKTYOfuCVVVt+yET63N4qp9YOPiMdCHxu+BUvgwmOgr/enRnh+HgCYVQtzLmDX"
        "imBcneRoZg3XgukoMQPd5TlVlZAF1JZ6W8uGN+LEiddnHdzYFVInest3xMzwHj4T"
        "3lXLCkr6oc9jvwKe2A2qsBvcbEFDR0mi0CW9NjfJ05v/52GKZZZyjEnFjnHJ1J5r"
        "1DlD5S8=";
}


class TslServiceTest : public testing::Test
{
public:
    std::unique_ptr<TrustStore> mTrustStore;
    std::unique_ptr<EnvironmentVariableGuard> mCaDerPathGuard;

    void SetUp() override
    {
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
        {"http://qocsp-ePTA.testumgebung.dgnservice.de:8080/ocsp", "http://titu-qocsp-hba.medisign.tsp-hba.telematik-test:8080/ocsp"},
        {"http://qocsp-zod2.testumgebung.dgnservice.de:8080/ocsp", "http://titu-qocsp-hba.medisign.tsp-hba.telematik-test:8080/ocsp"},
        {"http://ocsp-qes.egk-test-tsp.de", "http://ocsp-qes-testref.atos.tsp-hba.telematik-test"},
        {"http://qocsp-eA.testumgebung.dgnservice.de:8080/ocsp", "http://titu-qocsp-hba.medisign.tsp-hba.telematik-test:8080/ocsp"},
        {"http://ocsp.pki.telematik-test:8080/CMOCSP/OCSP", "http://ehca-testref.sig-test.telematik-test:8080/status/qocsp"},
        {"http://ehca.gematik.de/ocsp/", "http://ehca-testref.sig-test.telematik-test:8080/status/qocsp"},
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
    X509Certificate certificate = X509Certificate::createFromBase64(userCertificate);

    auto iterator = mTrustStore->mServiceInformationMap.find(
        {certificate.getIssuer(), certificate.getAuthorityKeyIdentifier()});
    ASSERT_NE(mTrustStore->mServiceInformationMap.end(), iterator);
    iterator->second.serviceAcceptanceHistory = {
        {date::sys_days{date::June/1/2020}, true},
        {date::sys_days{date::June/9/2020}, false}
    };

    TslTestHelper::setOcspUrlRequestHandler(
        requestSender,
        "http://ehca-testref.sig-test.telematik-test:8080/status/ocsp",
        {{Certificate::fromBase64Der(userCertificate),
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
    X509Certificate certificate = X509Certificate::createFromBase64(userCertificate);

    auto iterator = mTrustStore->mServiceInformationMap.find(
        {certificate.getIssuer(), certificate.getAuthorityKeyIdentifier()});
    ASSERT_NE(mTrustStore->mServiceInformationMap.end(), iterator);
    iterator->second.serviceAcceptanceHistory = {
        {date::sys_days{date::June/9/2020}, true},
        {date::sys_days{date::June/11/2020}, false}
    };

    TslTestHelper::setOcspUrlRequestHandler(
        requestSender,
        "http://ehca-testref.sig-test.telematik-test:8080/status/ocsp",
        {{Certificate::fromBase64Der(userCertificate),
          Certificate::fromBase64Der(iterator->second.certificate.toBase64()),
          MockOcsp::CertificateOcspTestMode::SUCCESS}});

    EXPECT_NO_THROW(
        TslService::checkCertificate(certificate, {}, requestSender, *mTrustStore, TslTestHelper::getDefaultTestOcspCheckDescriptor()));
}


TEST_F(TslServiceTest, verifyCertificatePolicyNoRestrictionsSuccessful)
{
    UrlRequestSenderMock requestSender({});
    X509Certificate certificate = X509Certificate::createFromBase64(userCertificate);

    auto iterator = mTrustStore->mServiceInformationMap.find(
        {certificate.getIssuer(), certificate.getAuthorityKeyIdentifier()});
    ASSERT_NE(mTrustStore->mServiceInformationMap.end(), iterator);

    TslTestHelper::setOcspUrlRequestHandler(
        requestSender,
        "http://ehca-testref.sig-test.telematik-test:8080/status/ocsp",
        {{Certificate::fromBase64Der(userCertificate),
          Certificate::fromBase64Der(iterator->second.certificate.toBase64()),
          MockOcsp::CertificateOcspTestMode::SUCCESS}});

    EXPECT_NO_THROW(
        TslService::checkCertificate(certificate, {}, requestSender, *mTrustStore, TslTestHelper::getDefaultTestOcspCheckDescriptor()));
}


TEST_F(TslServiceTest, verifyCertificatePolicySuccessful)
{
    UrlRequestSenderMock requestSender({});
    X509Certificate certificate = X509Certificate::createFromBase64(userCertificate);

    auto iterator = mTrustStore->mServiceInformationMap.find(
        {certificate.getIssuer(), certificate.getAuthorityKeyIdentifier()});
    ASSERT_NE(mTrustStore->mServiceInformationMap.end(), iterator);

    TslTestHelper::setOcspUrlRequestHandler(
        requestSender,
        "http://ehca-testref.sig-test.telematik-test:8080/status/ocsp",
        {{Certificate::fromBase64Der(userCertificate),
          Certificate::fromBase64Der(iterator->second.certificate.toBase64()),
          MockOcsp::CertificateOcspTestMode::SUCCESS}});

    EXPECT_NO_THROW(
        TslService::checkCertificate(certificate, {CertificateType::C_HCI_AUT}, requestSender, *mTrustStore, TslTestHelper::getDefaultTestOcspCheckDescriptor()));
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
