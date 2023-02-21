/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_ERP_SERVICE_HEALTHHANDLERTESTTSLMANAGER_HXX
#define ERP_PROCESSING_CONTEXT_TEST_ERP_SERVICE_HEALTHHANDLERTESTTSLMANAGER_HXX

#include <memory>
#include <vector>

class UrlRequestSender;
class XmlValidator;
class TrustStore;

class HealthHandlerTestTslManager : public TslManager
{
public:
    explicit HealthHandlerTestTslManager(
        std::shared_ptr<UrlRequestSender> requestSender,
        std::shared_ptr<XmlValidator> xmlValidator,
        const std::vector<std::function<void(void)>>& initialPostUpdateHooks,
        std::unique_ptr<TrustStore> tslTrustStore,
        std::unique_ptr<TrustStore> bnaTrustStore)
        : TslManager(std::move(requestSender), std::move(xmlValidator), initialPostUpdateHooks,
                     std::move(tslTrustStore), std::move(bnaTrustStore))
    {
    }

    TrustStore::OcspResponseData
    getCertificateOcspResponse(
        const TslMode tslMode,
        X509Certificate &certificate,
        const std::unordered_set<CertificateType> &typeRestrictions,
        const OcspCheckDescriptor& ocspCheckDescriptor) override
    {
        if (failOcspRetrieval)
            throw std::runtime_error("OCSP RETRIEVAL FAILURE");
        return TslManager::getCertificateOcspResponse(
            tslMode,
            certificate,
            typeRestrictions,
            ocspCheckDescriptor);
    }

    void healthCheckTsl() const override
    {
        if (failTsl)
            throw std::runtime_error("TSL FAILURE");
        TslManager::healthCheckTsl();
    }
    void healthCheckBna() const override
    {
        if (failBna)
            throw std::runtime_error("BNA FAILURE");
        TslManager::healthCheckBna();
    }

    static bool failTsl;
    static bool failBna;
    static bool failOcspRetrieval;
};
#endif
