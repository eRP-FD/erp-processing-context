/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/hsm/HsmFactory.hxx"

#include "shared/hsm/HsmClient.hxx"
#include "shared/hsm/HsmSession.hxx"
#include "shared/util/Expect.hxx"


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
    TLOG(INFO) << "Initializing new HSM-Session.";
    return std::make_unique<HsmSession>(
        *mHsmClient,
        *mBlobCache,
        rawConnect());
}


BlobCache& HsmFactory::getBlobCache (void)
{
    return *mBlobCache;
}
