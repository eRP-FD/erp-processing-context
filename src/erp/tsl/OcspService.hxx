/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_CLIENT_OCSP_OCSP_HXX
#define EPA_CLIENT_OCSP_OCSP_HXX

#include "erp/client/ClientBase.hxx"
#include "erp/crypto/OpenSslHelper.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/tsl/OcspCheckDescriptor.hxx"
#include "erp/tsl/OcspUrl.hxx"
#include "erp/tsl/X509Certificate.hxx"
#include "fhirtools/util/Gsl.hxx"

#include <chrono>
#include <optional>
#include <sstream>
#include <string>


class TrustStore;
class UrlRequestSender;

/**
 * Class for querying the OCSP status of certificates.
 *
 * @see [RFC690] https://tools.ietf.org/html/rfc6960
 */
class OcspService
{
public:

    class OcspError : public std::runtime_error
    {
    public:
        explicit OcspError (const std::string& string)
            : std::runtime_error(string)
        {}
    };

    enum class CertificateStatus
    {
        good, revoked, unknown
    };

    static std::string toString (CertificateStatus certificateStatus);

    static CertificateStatus toCertificateStatus (const std::string& string);

    struct Status
    {
        CertificateStatus certificateStatus = CertificateStatus::unknown;

        // > epoch, if certificateStatus == revoked
        std::chrono::system_clock::time_point revocationTime;

        std::string to_string() const
        {
            std::stringstream status;
            switch(certificateStatus)
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
    };

    /**
     * Gets the OCSP status of a certificate.
     *
     * @param certificate   the certificate to check
     * @param requestSender the OCSP request will be sent via this sender
     * @param ocspUrl       URL to use for OCSP-request
     * @param trustStore    contains the certificate of the issuer and cached
     *                      OCSP responses
     * @param validateHashExtension whether the hash extension should be validated
     * @param ocspCheckDescriptor       describes the OCSP check approach to use
     *
     * @return OCSP status (good / revoked / unknown) plus revocation time
     * @throws std::runtime_error on error
     */
    static Status
    getCurrentStatus (
        const X509Certificate& certificate,
        const UrlRequestSender& requestSender,
        const OcspUrl& ocspUrl,
        TrustStore& trustStore,
        const bool validateHashExtension,
        const OcspCheckDescriptor& ocspCheckDescriptor);

    /**
     * Gets the OCSP status for the TSL signer certificate.
     *
     * @param certificate   the certificate of the TSL signer
     * @param issuerCertificate the issuer certificate
     * @param requestSender the OCSP request will be sent via this sender
     * @param ocspUrl       URL to use for OCSP-request
     * @param oldTrustStore contains cached OCSP responses
     * @param ocspSignerCertificates  (optional) the certificates of the OCSP
     *                      responders as taken from the TSL
     * @param validateHashExtension whether the hash extension should be validated
     *
     * @return OCSP status (good / revoked / unknown) plus revocation time
     * @throws std::runtime_error on error
     */
    static Status
    getCurrentStatusOfTslSigner (const X509Certificate& certificate,
                                 const X509Certificate& issuerCertificate,
                                 const UrlRequestSender& requestSender,
                                 const OcspUrl& ocspUrl,
                                 TrustStore& oldTrustStore,
                                 const std::optional<std::vector<X509Certificate>>& ocspSignerCertificates,
                                 const bool validateHashExtension);


    /**
     * Checks OCSP status and throws related TslError in case it is not CertificateStatus::good
     */
    static void checkOcspStatus (
        const OcspService::Status& status,
        const std::optional<std::chrono::system_clock::time_point>& referenceTimePoint,
        const TrustStore& trustStore);
};

#endif
