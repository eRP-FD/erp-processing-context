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
class Tpm;

class EnrolmentServiceContext
{
public:
    explicit EnrolmentServiceContext (
        Tpm::Factory&& tpmFactory,
        const std::shared_ptr<BlobCache>& blobCache);
    ~EnrolmentServiceContext (void);

    EnrolmentData enrolmentData;
    Tpm::Factory tpmFactory;
    /// The blob cache is shared between the service contexts of enrolment service and processing context.
    std::shared_ptr<BlobCache> blobCache;
};


#endif
