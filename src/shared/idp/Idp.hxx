/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_IDP_HXX
#define ERP_PROCESSING_CONTEXT_IDP_HXX

#include "shared/crypto/Certificate.hxx"

#include <mutex>
#include <optional>


/**
 * Thread safe holder of the current valid IDP signer certificate.
 */
class Idp
{
public:
    const Certificate& getCertificate (void) const;

    void setCertificate (Certificate&& idpCertificate);

    void resetCertificate (void);

    bool isHealthy() const;
    void healthCheck() const;

private:
    mutable std::mutex mMutex;
    std::optional<Certificate> mSignerCertificate;
    std::chrono::system_clock::time_point mLastUpdate;
};


#endif
