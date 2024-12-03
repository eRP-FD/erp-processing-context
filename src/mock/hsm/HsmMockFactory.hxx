/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_HSMMOCKFACTORY_HXX
#define ERP_PROCESSING_CONTEXT_HSMMOCKFACTORY_HXX

#include "shared/hsm/HsmFactory.hxx"

#include "mock/hsm/HsmMockClient.hxx"

#if WITH_HSM_MOCK  != 1
#error HsmMockFactory.hxx included but WITH_HSM_MOCK not enabled
#endif


class HsmMockFactory : public HsmFactory
{
public:
    explicit HsmMockFactory (
        std::unique_ptr<HsmClient>&& hsmClient,
        std::shared_ptr<BlobCache> blobCache);

    std::shared_ptr<HsmRawSession> rawConnect (void) override;
};


#endif
