/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "mock/hsm/HsmMockFactory.hxx"

#include "erp/hsm/BlobCache.hxx"

#include "mock/hsm/MockBlobCache.hxx"


HsmMockFactory::HsmMockFactory (void)
    : HsmMockFactory(std::make_unique<HsmMockClient>(), MockBlobCache::createBlobCache(MockBlobCache::MockTarget::MockedHsm))
{
}


HsmMockFactory::HsmMockFactory (
    std::unique_ptr<HsmClient>&& hsmClient,
    std::shared_ptr<BlobCache> blobCache)
    : HsmFactory(std::move(hsmClient), std::move(blobCache))
{
}


std::shared_ptr<HsmRawSession> HsmMockFactory::rawConnect (void)
{
    return std::shared_ptr<HsmRawSession>();
}
