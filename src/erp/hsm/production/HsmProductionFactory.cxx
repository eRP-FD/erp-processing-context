/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/hsm/production/HsmProductionFactory.hxx"

#include "erp/hsm/HsmIdentity.hxx"
#include "erp/hsm/production/HsmProductionClient.hxx"
#include "erp/hsm/production/HsmRawSession.hxx"


HsmProductionFactory::HsmProductionFactory (
    std::unique_ptr<HsmClient>&& hsmClient,
    std::shared_ptr<BlobCache> blobCache)
    : HsmFactory(std::move(hsmClient), std::move(blobCache))
{
}


std::shared_ptr<HsmRawSession> HsmProductionFactory::rawConnect (void)
{
    return std::make_shared<HsmRawSession>(
        HsmProductionClient::connect(HsmIdentity::getWorkIdentity()));
}
