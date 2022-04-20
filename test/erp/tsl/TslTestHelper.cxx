/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include <gtest/gtest.h>// should be first or FRIEND_TEST would not work

#include "test/erp/tsl/TslTestHelper.hxx"

#include "erp/crypto/Certificate.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/tsl/TslService.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Environment.hxx"
#include "erp/util/String.hxx"
#include "test/erp/service/HealthHandlerTestTslManager.hxx"
#include "test/erp/tsl/RefreshJobTestTslManager.hxx"
#include "test/mock/MockOcsp.hxx"

#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#pragma warning(disable: 4996)
namespace {
    int mkstemp(char* filename)
    {
        errno_t result = _mktemp_s(filename, strlen(filename)+1);
        if (result != 0) return -1;
        result = open(filename, _O_RDWR | _O_CREAT | _O_EXCL, _S_IREAD | _S_IWRITE);
        return result;
    }
}
#else
#include <unistd.h>
#endif


namespace
{
    shared_EVP_PKEY getDefaultOcspPrivateKey()
    {
        static const SafeString ocspPrivateKeyPem {FileHelper::readFileAsString(
                std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/DefaultOcsp.prv.pem")};
        return EllipticCurveUtils::pemToPrivatePublicKeyPair(ocspPrivateKeyPem);
    }
}


std::string TslTestHelper::createValidButManipulatedTslContentsFromTemplate(const std::string& tslTemplate)
{
    // Read tst/TSL.xml, update its next update time to now plus 1 hour and write the modified TSL to a temporary file.
    const std::string content = FileHelper::readFileAsString(tslTemplate);
    const auto nextUpdateTime = std::chrono::system_clock::now() + std::chrono::hours(1);

    return String::replaceAll(
        content, "##NEXT-UPDATE-TIME##", model::Timestamp(nextUpdateTime).toXsDateTime());
}


Certificate TslTestHelper::getDefaultOcspCertificate()
{
    static const std::string ocspCertificatePem =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/DefaultOcsp.pem");
    return Certificate::fromPem(ocspCertificatePem);
}


Certificate TslTestHelper::getDefaultOcspCertificateCa()
{
    static const std::string ocspCertificateBase64Der =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/tsl/X509Certificate/DefaultOcspIssuer.base64.der");
    return Certificate::fromBase64Der(ocspCertificateBase64Der);
}


Certificate TslTestHelper::getTslSignerCertificate()
{
    static const std::string tslSignerCertificateString =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/generated_pki/sub_ca1_ec/certificates/tsl_signer_ec/tsl_signer_ec.der");
    return Certificate::fromBinaryDer(tslSignerCertificateString);
}


Certificate TslTestHelper::getTslSignerCACertificate()
{
    static const std::string tslSignerCertificateIssuerString =
        FileHelper::readFileAsString(
            std::string{TEST_DATA_DIR} + "/generated_pki/sub_ca1_ec/ca.der");
    return Certificate::fromBinaryDer(tslSignerCertificateIssuerString);
}


void TslTestHelper::setOcspUrlRequestHandler(
    UrlRequestSenderMock& requestSender,
    const std::string& url,
    const std::vector<MockOcsp::CertificatePair>& ocspResponderKnownCertificateCaPairs)
{
    requestSender.setUrlHandler(
        url,
        [
            ocspResponderKnownCertificateCaPairs,
            ocspCertificate = getDefaultOcspCertificate(),
            ocspPrivateKey = getDefaultOcspPrivateKey()]
        (const std::string& request) mutable -> ClientResponse
        {
            const std::string responseBody =
                MockOcsp::create(
                    request,
                    ocspResponderKnownCertificateCaPairs,
                    ocspCertificate,
                    ocspPrivateKey).toDer();

            Header header;
            header.setStatus(HttpStatus::OK);
            header.setContentLength(responseBody.size());
            return ClientResponse(header, responseBody);
        });
}


void TslTestHelper::setOcspUslRequestHandlerTslSigner(UrlRequestSenderMock& requestSender)
{
    setOcspUrlRequestHandler(
        requestSender,
        "http://ocsp-testref.tsl.telematik-test/ocsp",
        {{getTslSignerCertificate(), getTslSignerCACertificate(), MockOcsp::CertificateOcspTestMode::SUCCESS}});
}


void TslTestHelper::addOcspCertificateToTrustStore(const Certificate& certificate, TrustStore& trustStore)
{
    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());
    trustStore.mServiceInformationMap.emplace(
        CertificateId{x509Certificate.getSubject(), x509Certificate.getSubjectKeyIdentifier()},
        TslParser::ServiceInformation{
            x509Certificate,
            "http://uri.etsi.org/TrstSvc/Svctype/Certstatus/OCSP",
            {"http://ocsp00.gematik.invalid/not-used"},
            TslParser::AcceptanceHistoryMap{ {
                 std::chrono::time_point<std::chrono::system_clock>(std::chrono::milliseconds(1569415137000)),
                 true}},
            {TslService::oid_fd_sig}});
}


void TslTestHelper::addCaCertificateToTrustStore(
    const Certificate& certificate,
    TslManager& tslManager,
    const TslMode mode,
    const std::optional<std::string>& customSubjectKeyIdentifier,
    const std::optional<std::string>& customCertificateTypeId)
{
    TrustStore* trustStore = nullptr;
    if (mode == TslMode::TSL)
    {
        trustStore = tslManager.mTslTrustStore.get();
    }
    else // TslMode::BNA
    {
        trustStore = tslManager.mBnaTrustStore.get();
    }

    Expect(trustStore != nullptr, "TrustStore must be set to use the method.");

    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());
    TslParser::ExtensionOidList extensionOidList =
        (mode == TslMode::TSL
             ? TslParser::ExtensionOidList{
                   customCertificateTypeId.has_value()
                       ? *customCertificateTypeId
                       : TslService::oid_fd_sig}
             : TslParser::ExtensionOidList{});

    trustStore->mServiceInformationMap.emplace(
        CertificateId{x509Certificate.getSubject(),
                      customSubjectKeyIdentifier.has_value()
                          ? *customSubjectKeyIdentifier
                          : x509Certificate.getSubjectKeyIdentifier()},
        TslParser::ServiceInformation{
            x509Certificate,
            "http://uri.etsi.org/TrstSvc/Svctype/CA/QC",
            {"http://ocsp-testref.tsl.telematik-test/ocsp"},
            TslParser::AcceptanceHistoryMap{{
                std::chrono::time_point<std::chrono::system_clock>(std::chrono::milliseconds(1569415137000)),
                true}},
            extensionOidList});
}


void TslTestHelper::setCaCertificateTimestamp(
    const std::chrono::time_point<std::chrono::system_clock> timestamp,
    const Certificate& certificate,
    TslManager& tslManager,
    const TslMode mode)
{
    TrustStore* trustStore = nullptr;
    if (mode == TslMode::TSL)
    {
        trustStore = tslManager.mTslTrustStore.get();
    }
    else // TslMode::BNA
    {
        trustStore = tslManager.mBnaTrustStore.get();
    }

    Expect(trustStore != nullptr, "TrustStore must be set to use the method.");

    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());
    auto iterator = trustStore->mServiceInformationMap.find(
        {x509Certificate.getSubject(), x509Certificate.getSubjectKeyIdentifier()});
    Expect(iterator != trustStore->mServiceInformationMap.end(), "Unknown Certificate is provided.");

    iterator->second.serviceAcceptanceHistory.clear();
    iterator->second.serviceAcceptanceHistory =
        TslParser::AcceptanceHistoryMap{{timestamp, true}};
}


void TslTestHelper::removeCertificateFromTrustStore(
    const Certificate& certificate,
    TslManager& tslManager,
    const TslMode mode)
{
    TrustStore* trustStore = nullptr;
    if (mode == TslMode::TSL)
    {
        trustStore = tslManager.mTslTrustStore.get();
    }
    else // TslMode::BNA
    {
        trustStore = tslManager.mBnaTrustStore.get();
    }

    Expect(trustStore != nullptr, "TrustStore must be set to use the method.");

    X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());
    trustStore->mServiceInformationMap.erase({x509Certificate.getSubject(), x509Certificate.getSubjectKeyIdentifier()});
}


std::unique_ptr<TrustStore> TslTestHelper::createTslTrustStore(const std::optional<std::string>& tslContent)
{
    std::unique_ptr<TrustStore> trustStore =
        std::make_unique<TrustStore>(TslMode::TSL, std::vector<std::string>{tslDownloadUrl});

    const std::string tslContentToUse =
        (tslContent.has_value()
             ? *tslContent
             : FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/generated_pki/tsl/TSL_valid.xml"));

    Certificate ocspCertificate = getDefaultOcspCertificate();
    shared_EVP_PKEY ocspPrivateKey = getDefaultOcspPrivateKey();

    UrlRequestSenderMock requestSender(
        std::unordered_map<std::string, std::string>{
            {shaDownloadUrl, Hash::sha256(tslContentToUse)},
            {tslDownloadUrl, tslContentToUse}});

    setOcspUslRequestHandlerTslSigner(requestSender);

    addOcspCertificateToTrustStore(ocspCertificate, *trustStore);

    TslService::triggerTslUpdateIfNecessary(
        requestSender,
        *StaticData::getXmlValidator(),
        *trustStore,
        true);

    addOcspCertificateToTrustStore(ocspCertificate, *trustStore);

    return trustStore;
}


std::unique_ptr<TrustStore> TslTestHelper::createOutdatedTslTrustStore(
    const std::optional<std::string>& customTslDownloadUrl)
{
    const std::string url = customTslDownloadUrl.has_value() ? *customTslDownloadUrl : tslDownloadUrl;
    std::unique_ptr<TrustStore> trustStore =
        std::make_unique<TrustStore>(
            TslMode::TSL,
            std::vector<std::string>{url});

    const std::string tslContentToUse =
         FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/generated_pki/tsl/TSL_outdated.xml");

    Certificate ocspCertificate = getDefaultOcspCertificate();
    shared_EVP_PKEY ocspPrivateKey = getDefaultOcspPrivateKey();

    UrlRequestSenderMock requestSender(
        std::unordered_map<std::string, std::string>{
            {shaDownloadUrl, Hash::sha256(tslContentToUse)},
            {url, tslContentToUse}});

    setOcspUslRequestHandlerTslSigner(requestSender);

    addOcspCertificateToTrustStore(ocspCertificate, *trustStore);

    try
    {
        TslService::triggerTslUpdateIfNecessary(
            requestSender,
            *StaticData::getXmlValidator(),
            *trustStore,
            true);
        Fail("Trust store is outdated, update must fail.");
    }
    catch(const TslError& e)
    {
        Expect(e.getErrorData().size() == 1, "Only one error is expected");
        Expect(e.getErrorData()[0].errorCode == TslErrorCode::VALIDITY_WARNING_2,
               "Only outdated-error is expected");
    }

    addOcspCertificateToTrustStore(ocspCertificate, *trustStore);

    return trustStore;
}


template<class Manager>
std::shared_ptr<Manager> TslTestHelper::createTslManager(
    const std::shared_ptr<UrlRequestSenderMock>& customRequestSender,
    const std::vector<TslManager::PostUpdateHook>& initialPostUpdateHooks,
    const std::map<std::string, const std::vector<MockOcsp::CertificatePair>>& ocspResponderKnownCertificateCaPairs,
    const std::optional<Certificate>& ocspCertificate,
    std::unique_ptr<TrustStore> tslTrustStore)
{
    const std::string tslContent = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/generated_pki/tsl/TSL_valid.xml");
    const std::string bnaContent = FileHelper::readFileAsString(std::string{TEST_DATA_DIR} + "/tsl/BNA_valid.xml");
    std::shared_ptr<UrlRequestSenderMock> requestSender = customRequestSender;
    if (requestSender == nullptr)
    {
        requestSender = std::make_shared<UrlRequestSenderMock>(
            std::unordered_map<std::string, std::string>{
                {shaDownloadUrl, Hash::sha256(tslContent)},
                {tslDownloadUrl, tslContent},
                {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.xml", bnaContent},
                {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.sha2", Hash::sha256(bnaContent)}});
    }

    setOcspUslRequestHandlerTslSigner(*requestSender);

    for (const auto& [url, knownCertificatesPairs] : ocspResponderKnownCertificateCaPairs)
    {
        setOcspUrlRequestHandler(*requestSender, url, knownCertificatesPairs);
    }

    std::unique_ptr<TrustStore> trustStore(std::move(tslTrustStore));
    if (trustStore == nullptr)
    {
        trustStore = std::make_unique<TrustStore>(TslMode::TSL, std::vector<std::string>{tslDownloadUrl});
    }
    addOcspCertificateToTrustStore(getDefaultOcspCertificate(), *trustStore);

    std::shared_ptr<Manager> result(new Manager(
        requestSender,
        StaticData::getXmlValidator(),
        initialPostUpdateHooks,
        std::move(trustStore),
        std::unique_ptr<TrustStore>{}));
    Expect(result != nullptr, "can not create TSL manager");

    Certificate ocspCertificateToUse = ocspCertificate.has_value() ? *ocspCertificate : getDefaultOcspCertificate();
    addOcspCertificateToTrustStore(ocspCertificateToUse, *result->mTslTrustStore);
    addOcspCertificateToTrustStore(ocspCertificateToUse, *result->mBnaTrustStore);

    return result;
}


template std::shared_ptr<TslManager> TslTestHelper::createTslManager(
    const std::shared_ptr<UrlRequestSenderMock>& customRequestSender,
    const std::vector<TslManager::PostUpdateHook>& initialPostUpdateHooks,
    const std::map<std::string, const std::vector<MockOcsp::CertificatePair>>& ocspResponderKnownCertificateCaPairs,
    const std::optional<Certificate>& ocspCertificate,
    std::unique_ptr<TrustStore> tslTrustStore);

template std::shared_ptr<RefreshJobTestTslManager> TslTestHelper::createTslManager(
    const std::shared_ptr<UrlRequestSenderMock>& customRequestSender,
    const std::vector<TslManager::PostUpdateHook>& initialPostUpdateHooks,
    const std::map<std::string, const std::vector<MockOcsp::CertificatePair>>& ocspResponderKnownCertificateCaPairs,
    const std::optional<Certificate>& ocspCertificate,
    std::unique_ptr<TrustStore> tslTrustStore);

template std::shared_ptr<HealthHandlerTestTslManager> TslTestHelper::createTslManager(
    const std::shared_ptr<UrlRequestSenderMock>& customRequestSender,
    const std::vector<TslManager::PostUpdateHook>& initialPostUpdateHooks,
    const std::map<std::string, const std::vector<MockOcsp::CertificatePair>>& ocspResponderKnownCertificateCaPairs,
    const std::optional<Certificate>& ocspCertificate,
    std::unique_ptr<TrustStore> tslTrustStore);
