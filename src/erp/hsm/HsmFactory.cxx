/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/hsm/HsmFactory.hxx"

#include "erp/hsm/HsmClient.hxx"
#include "erp/hsm/HsmSession.hxx"
#include "erp/util/Expect.hxx"


HsmFactory::HsmFactory (
    std::unique_ptr<HsmClient>&& hsmClient,
    std::shared_ptr<BlobCache>&& blobCache)
    : mHsmClient(std::move(hsmClient)),
      mBlobCache(std::move(blobCache))
{
    Expect(mHsmClient!=nullptr, "hsm client is missing");
    Expect(mBlobCache!=nullptr, "blob cache is missing");
}


std::unique_ptr<HsmSession> HsmFactory::connect (void)
{
    TLOG(WARNING) << "Initializing new HSM-Session.";
    return std::make_unique<HsmSession>(
        *mHsmClient,
        *mBlobCache,
        rawConnect());
}


BlobCache& HsmFactory::getBlobCache (void)
{
    return *mBlobCache;
}
