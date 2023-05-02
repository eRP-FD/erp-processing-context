/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "mock/tsl/MockTslManager.hxx"
#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Hash.hxx"
#include "mock/util/MockConfiguration.hxx"

#include "UrlRequestSenderMock.hxx"

std::shared_ptr<TslManager> MockTslManager::createMockTslManager(std::shared_ptr<XmlValidator> xmlValidator)
{
    const auto pkiPath = MockConfiguration::instance().getPathValue(MockConfigurationKey::MOCK_GENERATED_PKI_PATH);
    const std::string tslContent = FileHelper::readFileAsString(pkiPath / "tsl/TSL_no_ocsp_mapping.xml");
    const std::string bnaContent = FileHelper::readFileAsString(pkiPath / "tsl/BNA_valid.xml");
    auto requestSender = std::make_shared<UrlRequestSenderMock>(std::unordered_map<std::string, std::string>{
        {"http://download-ref.tsl.telematik-test:80/ECC/ECC-RSA_TSL-ref.xml", tslContent},
        {"https://download-ref.tsl.telematik-test:443/ECC/ECC-RSA_TSL-ref.sha2", Hash::sha256(tslContent)},
        {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.xml", bnaContent},
        {"https://download-testref.bnetzavl.telematik-test:443/BNA-TSL.sha2", Hash::sha256(bnaContent)}});

    const std::string tslSignerCertificateString =
        FileHelper::readFileAsString(pkiPath / "sub_ca1_ec/certificates/tsl_signer_ec/tsl_signer_ec.der");

    const std::string tslSignerCertificateIssuerString = FileHelper::readFileAsString(pkiPath / "sub_ca1_ec/ca.der");

    const std::string ocspCertificatePem =
        FileHelper::readFileAsString(pkiPath / "../tsl/X509Certificate/DefaultOcsp.pem");

    const SafeString ocspPrivateKeyPem{
        FileHelper::readFileAsString(pkiPath / "../tsl/X509Certificate/DefaultOcsp.prv.pem")};
    const auto ocspPrivateKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(ocspPrivateKeyPem);
    const auto ocspCertificate = Certificate::fromPem(ocspCertificatePem);

    const auto idpCertificate =
        Certificate::fromPem(FileHelper::readFileAsString(pkiPath / "../tsl/X509Certificate/IDP-Wansim.pem"));
    const auto idpCertificateCa =
        Certificate::fromPem(FileHelper::readFileAsString(pkiPath / "../tsl/X509Certificate/IDP-Wansim-CA.pem"));
    requestSender->setOcspUrlRequestHandler(
        "http://ocsp-testref.tsl.telematik-test/ocsp",
        {{Certificate::fromBinaryDer(tslSignerCertificateString),
          Certificate::fromBinaryDer(tslSignerCertificateIssuerString), MockOcsp::CertificateOcspTestMode::SUCCESS},
         {idpCertificate, idpCertificateCa, MockOcsp::CertificateOcspTestMode::SUCCESS}},
        ocspCertificate, ocspPrivateKey);

    const auto cert = Certificate::fromPem(Configuration::instance().getStringValue(ConfigurationKey::C_FD_SIG_ERP));
    const auto certCA = Certificate::fromPem(
        MockConfiguration::instance().getStringValue(MockConfigurationKey::MOCK_ID_FD_SIG_SIGNER_CERT));
    requestSender->setOcspUrlRequestHandler(
        MockConfiguration::instance().getStringValue(MockConfigurationKey::MOCK_ID_FD_SIG_OCSP_URL),
        {{cert, certCA, MockOcsp::CertificateOcspTestMode::SUCCESS}}, ocspCertificate, ocspPrivateKey);


    const auto qesCert = Certificate::fromPem(FileHelper::readFileAsString(pkiPath / "../tsl/X509Certificate/qes.pem"));
    const auto gemHbaQCA6 = Certificate::fromBinaryDer(
        FileHelper::readFileAsString(pkiPath / "../tsl/X509Certificate/GEM.HBA-qCA6_TEST-ONLY.der"));
    requestSender->setOcspUrlRequestHandler("http://ehca.gematik.de/ocsp/",
                                            {{qesCert, gemHbaQCA6, MockOcsp::CertificateOcspTestMode::SUCCESS}},
                                            ocspCertificate, ocspPrivateKey);

    const auto nonQesSmcbCert = Certificate::fromPem(FileHelper::readFileAsString(pkiPath / "../tsl/X509Certificate/nonQesSmcb.pem"));
    const auto nonQesSmcbIssuer = Certificate::fromBinaryDer(
        FileHelper::readFileAsString(pkiPath / "../tsl/X509Certificate/nonQesSmcbIssuer.der"));
    requestSender->setOcspUrlRequestHandler("http://ocsp-test.ocsp.telematik-test:8080/",
                                            {{nonQesSmcbCert, nonQesSmcbIssuer, MockOcsp::CertificateOcspTestMode::SUCCESS}},
                                            ocspCertificate, ocspPrivateKey);

    auto trustStore = std::make_unique<TrustStore>(
        TslMode::TSL, std::vector<std::string>{"http://download-ref.tsl.telematik-test:80/ECC/ECC-RSA_TSL-ref.xml"});

    addOcspCertificateToTrustStore(ocspCertificate, *trustStore);

    std::shared_ptr<TslManager> mgr(new TslManager(requestSender, std::move(xmlValidator),
                                                   std::vector<TslManager::PostUpdateHook>{}, std::move(trustStore),
                                                   std::unique_ptr<TrustStore>{}));
    std::weak_ptr<TslManager> mgrWeakPtr{mgr};
    auto postUpdateHook = [mgrWeakPtr, ocspCertificate, idpCertificateCa]{
        auto tslManager = mgrWeakPtr.lock();
        if (!tslManager)
            return;
        addOcspCertificateToTrustStore(ocspCertificate, *tslManager->mTslTrustStore);
        addOcspCertificateToTrustStore(ocspCertificate, *tslManager->mBnaTrustStore);
        addCaCertificateToTrustStore(idpCertificateCa, *tslManager, TSL);
        addCaCertificateToTrustStore(idpCertificateCa, *tslManager, BNA);
    };
    mgr->addPostUpdateHook(postUpdateHook);

    return mgr;
}

void MockTslManager::addOcspCertificateToTrustStore(const Certificate& certificate, TrustStore& trustStore)
{
    const X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());
    trustStore.mServiceInformationMap.emplace(
        CertificateId{x509Certificate.getSubject(), x509Certificate.getSubjectKeyIdentifier()},
        TslParser::ServiceInformation{
            x509Certificate,
            "http://uri.etsi.org/TrstSvc/Svctype/Certstatus/OCSP",
            {"http://ocsp00.gematik.invalid/not-used"},
            TslParser::AcceptanceHistoryMap{
                {std::chrono::time_point<std::chrono::system_clock>(std::chrono::milliseconds(1569415137000)), true}},
            {TslService::oid_fd_sig}});
}

void MockTslManager::addCaCertificateToTrustStore(const Certificate& certificate, TslManager& tslManager,
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

    const X509Certificate x509Certificate = X509Certificate::createFromBase64(certificate.toBase64Der());
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
