/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_SHARED_TSL_TSLMANAGER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_SHARED_TSL_TSLMANAGER_HXX


#include <boost/core/noncopyable.hpp>
#include <string>
#include <unordered_set>
#include <vector>

#include "shared/tsl/TslService.hxx"


class TslManager;

// GEMREQ-start A_16901
/**
 * This TslProvider is only intended to be used for PC.
 */
class TslProvider : private boost::noncopyable
{
public:
    static TslProvider& getInstance();

    void setTslManager(TslManager* tslManager);

    /**
     * Get OCSP response. A response is cached and subsequent calls with the same certificate as subject return
     * a cached response.
     */
    std::string getOcspResponse(const std::string& certAsDerBase64String);

    /**
     * Checks certificate and returns all role OIDs of the certificate.
     * Throws TslError in case of problems.
     */
    std::vector<std::string> checkCertificate(
        const std::string& certAsDerBase64String,
        const std::unordered_set<CertificateType>& typeRestrictions);

    /**
     * Checks certificate and returns all role OIDs of the certificate.
     * Throws TslError in case of problems.
     */
    std::vector<std::string> checkCertificate(
        const X509Certificate& certificate,
        const std::unordered_set<CertificateType>& typeRestrictions);


private:
    TslProvider();

    // only for tests
    friend class TslHelper;
    //

    mutable std::mutex mMutex;
    TslManager* mTslManager{nullptr};
};
// GEMREQ-end A_16901

#endif
