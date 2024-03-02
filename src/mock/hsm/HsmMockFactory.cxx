/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "mock/hsm/HsmMockFactory.hxx"

#include "erp/hsm/BlobCache.hxx"


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
