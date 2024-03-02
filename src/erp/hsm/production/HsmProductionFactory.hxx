/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_HSM_HSMPRODUCTIONFACTORY_HXX
#define ERP_PROCESSING_CONTEXT_HSM_HSMPRODUCTIONFACTORY_HXX

#include "erp/hsm/HsmFactory.hxx"
#include "erp/hsm/HsmSession.hxx"

#include "erp/util/SafeString.hxx"

#include <string>
#include <chrono>


/**
 * Factory for HSM session objects.
 *
 * Do not use this factory directly. Rather use HsmPool to acquire one.
 *
 * For more information, see
 *     doc/HsmAndTpm.md
 *     https://dth01.ibmgcloud.net/confluence/display/ERP/HSM+Firmware+Interface
 *     https://dth01.ibmgcloud.net/confluence/display/ERP/Start+Here
 *
 * The relationship between HsmProductionFactory and HsmSession is so that HsmProductionFactory will log in to the HSM
 * when the application starts. Then it obtains and keeps the TEE token.
 */
class HsmProductionFactory : public HsmFactory
{
public:
    /**
     *  Hsm is a factory for HsmSession objects.
     */
    explicit HsmProductionFactory (
        std::unique_ptr<HsmClient>&& hsmClient,
        std::shared_ptr<BlobCache> blobCache);
    ~HsmProductionFactory (void) override = default;

    /**
     *  Connect as a user with a password.
     *
     *  This is subject to change. The final design an implementation depend on the details of the enrollment process.
     *  At the moment device, username and password are taken from the configuration.
     *
     *  Either returns a valid HamSession object or throws an exception.
     */
    std::shared_ptr<HsmRawSession> rawConnect (void) override;
};


#endif
