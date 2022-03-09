/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/enrolment/EnrolmentServiceContext.hxx"

#include "erp/util/Expect.hxx"
#include "erp/tpm/Tpm.hxx"


EnrolmentServiceContext::EnrolmentServiceContext (
    Tpm::Factory&& _tpmFactory,
    const std::shared_ptr<BlobCache>& _blobCache,
    std::shared_ptr<TslManager> initTslManager,
    std::shared_ptr<HsmPool> initHsmPool)
    : enrolmentData(),
      tpmFactory(std::move(_tpmFactory)),
      blobCache(_blobCache),
      tslManager{std::move(initTslManager)},
      hsmPool{initHsmPool}
{
    Expect3(tpmFactory!=nullptr, "TPM factory has been passed as nullptr to EnrolmentServiceContext", std::logic_error);
    Expect3(blobCache!=nullptr, "blob cache has been passed as nullptr to EnrolmentServiceContext", std::logic_error);
    Expect3(hsmPool!=nullptr, "hsmPool has been passed as nullptr to EnrolmentServiceContext", std::logic_error);
}


EnrolmentServiceContext::~EnrolmentServiceContext (void) = default;
