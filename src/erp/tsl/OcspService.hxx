/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_CLIENT_OCSP_OCSP_HXX
#define EPA_CLIENT_OCSP_OCSP_HXX

#include "erp/client/ClientBase.hxx"
#include "erp/crypto/OpenSslHelper.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/tsl/OcspCheckDescriptor.hxx"
#include "erp/tsl/OcspResponse.hxx"
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

    /**
     * Gets the OCSP response for certificate.
     *
     * @param certificate   the certificate to check
     * @param requestSender the OCSP request will be sent via this sender
     * @param ocspUrl       URL to use for OCSP-request
     * @param trustStore    contains the certificate of the issuer and cached
     *                      OCSP responses
     * @param validateHashExtension whether the hash extension should be validated
     * @param ocspCheckDescriptor       describes the OCSP check approach to use
     *
     * @return OCSP response
     * @throws std::runtime_error on error
     */
    static OcspResponse
      getCurrentResponse (
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
    static OcspStatus
    getCurrentStatusOfTslSigner (const X509Certificate& certificate,
                                 const X509Certificate& issuerCertificate,
                                 const UrlRequestSender& requestSender,
                                 const OcspUrl& ocspUrl,
                                 TrustStore& oldTrustStore,
                                 const std::optional<std::vector<X509Certificate>>& ocspSignerCertificates,
                                 const bool validateHashExtension);

};

#endif
