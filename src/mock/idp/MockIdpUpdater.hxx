/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MOCKIDPUPDATER_HXX
#define ERP_PROCESSING_CONTEXT_MOCKIDPUPDATER_HXX

#include "shared/idp/IdpUpdater.hxx"

#if WITH_HSM_MOCK  != 1
#error MockIdpUpdater.hxx included but WITH_HSM_MOCK not enabled
#endif

class MockIdpUpdater : public IdpUpdater
{
public:
    using IdpUpdater::IdpUpdater;

protected:
    std::string doDownloadWellknown (void) override;
    std::string doDownloadDiscovery (const UrlHelper::UrlParts& url) override;
};


#endif
