#ifndef ERP_PROCESSING_CONTEXT_IDP_HXX
#define ERP_PROCESSING_CONTEXT_IDP_HXX

#include "erp/crypto/Certificate.hxx"

#include <mutex>
#include <optional>


/**
 * Thread safe holder of the current valid IDP signer certificate.
 */
class Idp
{
public:
    const Certificate& getCertificate (void) const;

    void setCertificate (Certificate&& updatToDateCertificate);

    void resetCertificate (void);

    void healthCheck() const;

private:
    mutable std::mutex mMutex;
    std::optional<Certificate> mSignerCertificate;
    std::chrono::system_clock::time_point mLastUpdate;
};


#endif
