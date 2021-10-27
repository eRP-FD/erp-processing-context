/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_HSMMOCKFACTORY_HXX
#define ERP_PROCESSING_CONTEXT_HSMMOCKFACTORY_HXX

#include "erp/hsm/HsmFactory.hxx"

#include "mock/hsm/HsmMockClient.hxx"

#if WITH_HSM_MOCK  != 1
#error HsmMockFactory.hxx included but WITH_HSM_MOCK not enabled
#endif


class HsmMockFactory : public HsmFactory
{
public:
    HsmMockFactory (void);
    explicit HsmMockFactory (
        std::unique_ptr<HsmClient>&& hsmClient,
        std::shared_ptr<BlobCache> blobCache);

    virtual std::shared_ptr<HsmRawSession> rawConnect (void) override;
};


#endif
