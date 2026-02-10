/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
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
    Certificate getCertificate() const;

    void setCertificate (Certificate&& idpCertificate);

    void resetCertificate (void);

    void setSecondaryCertificate(Certificate secondaryCertificate);
    std::optional<Certificate> getSecondaryCertificate() const;
    void resetSecondaryCertificate();

    bool isHealthy() const;
    void healthCheck() const;

private:
    mutable std::mutex mMutex;
    std::optional<Certificate> mSignerCertificate;
    std::optional<Certificate> mSecondaryCertificate;
    std::chrono::system_clock::time_point mLastUpdate;
};


#endif
