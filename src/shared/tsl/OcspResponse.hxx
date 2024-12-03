#ifndef ERP_PROCESSING_CONTEXT_OCSP_RESPONSE_TSL_HXX
#define ERP_PROCESSING_CONTEXT_OCSP_RESPONSE_TSL_HXX

#include <chrono>
#include <optional>
#include <string>

class TrustStore;

enum class CertificateStatus
{
    good, revoked, unknown
};

struct OcspStatus
{
    CertificateStatus certificateStatus = CertificateStatus::unknown;

    // > epoch, if certificateStatus == revoked
    std::chrono::system_clock::time_point revocationTime;

    std::string to_string() const;
};


std::string toString(CertificateStatus certificateStatus);
CertificateStatus toCertificateStatus(const std::string& string);

class OcspResponse
{
public:
    OcspStatus status;
    std::chrono::system_clock::duration gracePeriod{0};
    std::chrono::system_clock::time_point producedAt;
    std::chrono::system_clock::time_point receivedAt;
    bool fromCache;
    std::string response;

    void checkStatus(const TrustStore& trustStore,
                     std::optional<std::chrono::system_clock::time_point> referenceTimePoint = std::nullopt);
};

#endif// ERP_PROCESSING_CONTEXT_OCSP_RESPONSE_TSL_HXX
