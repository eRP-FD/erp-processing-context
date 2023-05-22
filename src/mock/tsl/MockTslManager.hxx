/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_MOCK_TSL_MOCKTSLMANAGER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_MOCK_TSL_MOCKTSLMANAGER_HXX


#include "erp/tsl/TslManager.hxx"
#include "erp/crypto/Certificate.hxx"

#if WITH_HSM_MOCK  != 1
#error MockTslManager.hxx included but WITH_HSM_MOCK not enabled
#endif

class MockTslManager
{
public:
    static std::shared_ptr<TslManager> createMockTslManager(std::shared_ptr<XmlValidator> xmlValidator);

    static void addOcspCertificateToTrustStore(const Certificate& certificate, TrustStore& trustStore);
    static void
    addCaCertificateToTrustStore(const Certificate& certificate, TslManager& tslManager, const TslMode mode,
                                 const std::optional<std::string>& customSubjectKeyIdentifier = std::nullopt,
                                 const std::optional<std::string>& customCertificateTypeId = std::nullopt);
};


#endif//ERP_PROCESSING_CONTEXT_SRC_MOCK_TSL_MOCKTSLMANAGER_HXX
