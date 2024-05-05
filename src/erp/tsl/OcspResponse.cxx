#include "OcspResponse.hxx"

#include "erp/tsl/error/TslError.hxx"
#include "erp/tsl/error/TslErrorCode.hxx"
#include "erp/tsl/TrustStore.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/String.hxx"

constexpr const char* statusGood{"good"};
constexpr const char* statusRevoked{"revoked"};
constexpr const char* statusUnknown{"unknown"};

std::string toString (CertificateStatus certificateStatus)
{
    switch (certificateStatus)
    {
        case CertificateStatus::good:
            return statusGood;
        case CertificateStatus::revoked:
            return statusRevoked;
        case CertificateStatus::unknown:
            return statusUnknown;
    }
    Fail2("Certificate Status is invalid", std::logic_error);
}

CertificateStatus toCertificateStatus (const std::string& string)
{
    const std::string lowercaseString{String::toLower(string)};
    if (lowercaseString == statusGood)
    {
        return CertificateStatus::good;
    }
    if (lowercaseString == statusRevoked)
    {
        return CertificateStatus::revoked;
    }
    return CertificateStatus::unknown;
}

std::string OcspStatus::to_string() const
{
    std::stringstream status;
    switch (certificateStatus)
    {
        case CertificateStatus::good:
            status << "Status: Good";
            break;
        case CertificateStatus::revoked:
            status << "Status: Revoked";
            break;
        case CertificateStatus::unknown:
            status << "Status: Unknown";
            break;
        default:
            Fail2("Unhandled CertificateStatus enum value", std::logic_error);
    }

    status << ", revocation time: " << std::chrono::system_clock::to_time_t(revocationTime);

    return status.str();
}



void OcspResponse::checkStatus(const TrustStore& trustStore, std::optional<std::chrono::system_clock::time_point> referenceTimePoint)
{
    if (!referenceTimePoint)
    {
        referenceTimePoint = std::chrono::system_clock::now();
    }
    TslExpect6(
        status.certificateStatus != CertificateStatus::unknown,
        "OCSP Check failed, certificate is unknown.",
        TslErrorCode::CERT_UNKNOWN,
        trustStore.getTslMode(),
               trustStore.getIdOfTslInUse(),
               trustStore.getSequenceNumberOfTslInUse());

    TslExpect6(
        status.certificateStatus != CertificateStatus::revoked
        || referenceTimePoint < status.revocationTime,
               "OCSP Check failed, certificate is revoked.",
               TslErrorCode::CERT_REVOKED,
               trustStore.getTslMode(),
               trustStore.getIdOfTslInUse(),
               trustStore.getSequenceNumberOfTslInUse());
}
