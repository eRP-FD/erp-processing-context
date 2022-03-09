/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_ENROLMENTSERVICECONTEXT_HXX
#define ERP_PROCESSING_CONTEXT_ENROLMENTSERVICECONTEXT_HXX


#include "erp/enrolment/EnrolmentData.hxx"
#include "erp/hsm/BlobCache.hxx"
#include "erp/tpm/Tpm.hxx"


class BlobDatabase;
class HsmPool;
class Tpm;
class TslManager;

class EnrolmentServiceContext
{
public:
    explicit EnrolmentServiceContext (
        Tpm::Factory&& tpmFactory,
        const std::shared_ptr<BlobCache>& blobCache,
        std::shared_ptr<TslManager> initTslManager,
        std::shared_ptr<HsmPool> initHsmPool);
    ~EnrolmentServiceContext (void);

    EnrolmentData enrolmentData;
    Tpm::Factory tpmFactory;
    /// The blob cache is shared between the service contexts of enrolment service and processing context.
    std::shared_ptr<BlobCache> blobCache;
    std::shared_ptr<TslManager> tslManager;
    std::shared_ptr<HsmPool> hsmPool;
};


#endif
