/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/hsm/production/TeeTokenProductionUpdater.hxx"

#if ! defined(__APPLE__)
#include "erp/enrolment/EnrolmentHelper.hxx"
#include "erp/hsm/HsmFactory.hxx"
#include "erp/hsm/HsmIdentity.hxx"
#else
#include "erp/hsm/ErpTypes.hxx"
#include "mock/tpm/TpmTestData.hxx"
#endif


ErpBlob TeeTokenProductionUpdater::provideTeeToken (HsmFactory& hsmFactory)
{
#if ! defined(__APPLE__)
    auto& blobCache = hsmFactory.getBlobCache();
    auto teeToken = EnrolmentHelper(HsmIdentity::getWorkIdentity())
        .createTeeToken(blobCache);
#else
    (void)hsmFactory;
    auto teeToken = ErpBlob::fromCDump(tpm::teeToken_blob_base64);
#endif

    TVLOG(1) << "TeeTokenProductionUpdater::doUpdate, finished getting TEE token";
    TVLOG(1) << "got new tee token of size " << teeToken.data.size() << " with generation " << teeToken.generation;
    return teeToken;
}
