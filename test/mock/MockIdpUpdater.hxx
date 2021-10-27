/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MOCKIDPUPDATER_HXX
#define ERP_PROCESSING_CONTEXT_MOCKIDPUPDATER_HXX

#include "erp/idp/IdpUpdater.hxx"


class MockIdpUpdater : public IdpUpdater
{
public:
    using IdpUpdater::IdpUpdater;

protected:
    virtual std::string doDownloadWellknown (void);
    virtual std::string doDownloadDiscovery (const UrlHelper::UrlParts& url);
};


#endif
