/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/tsl/OcspService.hxx"

#include "erp/client/HttpClient.hxx"
#include "erp/client/UrlRequestSender.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/tsl/OcspHelper.hxx"
#include "erp/tsl/TrustStore.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Buffer.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/GLog.hxx"
#include "erp/util/String.hxx"
#include "erp/util/UrlHelper.hxx"

#include <cctype>
#include <ctime>
#include <utility>


#undef Expect
#define Expect(expression, message) Expect3(expression, message, OcspService::OcspError)

namespace
{
    static std::mutex requestMutex;

    constexpr int defaultOcspGracePeriodSeconds = 600;

    constexpr const char* statusGood{"good"};
    constexpr const char* statusRevoked{"revoked"};
    constexpr const char* statusUnknown{"unknown"};

    // value "Toleranz t" from GS-A_5215
    constexpr const int tolerance = 37;

    using OcspRequestPtr = std::unique_ptr<OCSP_REQUEST, decltype(&OCSP_REQUEST_free)>;
    using OcspResponsePtr = std::unique_ptr<OCSP_RESPONSE, decltype(&OCSP_RESPONSE_free)>;
    using Asn1ObjectPtr = std::unique_ptr<ASN1_OBJECT, decltype(&ASN1_OBJECT_free)>;

    std::pair<OcspRequestPtr, OCSP_CERTID*>
    createOcspRequest (const X509Certificate& certificate, const X509Certificate& issuer)
    {
        Expect(certificate.hasX509Ptr(),
               "certificate must provide X509 representation");
        Expect(issuer.hasX509Ptr(),
               "issuer certificate must provide X509 representation");

        OcspRequestPtr request{OCSP_REQUEST_new(), OCSP_REQUEST_free};
        Expect(request.operator bool(), "Could not create OCSP request!");

        OCSP_CERTID* id{OCSP_cert_to_id(nullptr, certificate.getX509ConstPtr(), issuer.getX509ConstPtr())};
        Expect(id, "Could not create OCSP request!");

        if (!OCSP_request_add0_id(request.get(), id))
        {
            OCSP_CERTID_free(id);
            throw OcspService::OcspError("Could not create OCSP request!");
        }

        // after this, id will be part of request, so it does no longer have to be freed
        return std::make_pair(std::move(request), id);
    }


    util::Buffer toBuffer (const OcspRequestPtr& ocspRequest)
    {
        unsigned char* buffer = nullptr;
        const int bufferLength = i2d_OCSP_REQUEST(ocspRequest.get(), &buffer);
        Expect(bufferLength > 0, "Could not create OCSP request!");

        util::Buffer requestBuffer{util::rawToBuffer(buffer, bufferLength)};

        OPENSSL_free(buffer);

        return requestBuffer;
    }


    std::chrono::seconds getGracePeriod(const TslMode mode)
    {
        if (mode == TslMode::TSL)
        {
            return std::chrono::seconds(
                Configuration::instance().getOptionalIntValue(ConfigurationKey::OCSP_NON_QES_GRACE_PERIOD,
                                                              defaultOcspGracePeriodSeconds));
        }
        else
        {
            return std::chrono::seconds(
                Configuration::instance().getOptionalIntValue(ConfigurationKey::OCSP_QES_GRACE_PERIOD,
                                                              defaultOcspGracePeriodSeconds));
        }
    }


    std::string sendOcspRequest (const UrlRequestSender& requestSender,
                                 const OcspUrl& url,
                                 const util::Buffer& request)
    {
        auto timer = DurationConsumer::getCurrent().getTimer("OCSP response from " + url.url);

        try
        {
            std::lock_guard guard (requestMutex);

            const std::string contentType = "application/ocsp-request";

            VLOG(2) << "OCSP request (base64-encoded) on Url: " << url.url << "\n"
                    << Base64::encode(request) << "\n\n";

            const auto timeStart = std::chrono::steady_clock::now();

            const auto response = requestSender.send(
                url.url,
                HttpMethod::POST,
                std::string(reinterpret_cast<const char*>(request.data()), request.size()),
                contentType);

            const auto timeStop = std::chrono::steady_clock::now();
            std::chrono::duration<double> elapsedTime =
                std::chrono::duration_cast<std::chrono::duration<double>>(timeStop - timeStart);
            VLOG(1) << "Elapsed time for operation OCSP request: " << elapsedTime.count() << " seconds.";

            VLOG(2) << "OCSP response, status=" << response.getHeader().status()
                    << " (base-encoded):\n" << Base64::encode(response.getBody()) << "\n\n";

            Expect(response.getHeader().status() == HttpStatus::OK,
                   std::string("OCSP call failed, status=") + toString(response.getHeader().status()));

            return response.getBody();
        }
        catch (const std::exception& e)
        {
            timer.notifyFailure(e.what());
            TslFailWithStatus(e.what() + std::string(", ocsp url: ") + url.url,
                              TslErrorCode::OCSP_NOT_AVAILABLE,
                              url.isRequestData ? HttpStatus::BadRequest : HttpStatus::InternalServerError,
                              trustStore.getTslMode());
        }
    }


    OcspResponsePtr toOcspResponse (const std::string& response)
    {
        if (response.empty())
        {
            return OcspResponsePtr{nullptr, OCSP_RESPONSE_free};
        }
        const unsigned char* buffer = reinterpret_cast<const unsigned char*>(response.data());
        return OcspResponsePtr{ d2i_OCSP_RESPONSE(nullptr, &buffer, gsl::narrow_cast<int>(response.size())),
                                OCSP_RESPONSE_free };
    }


    X509* checkSignatureAndGetSigner (OCSP_BASICRESP& basicResponse)
    {
        X509* signer{nullptr};
        if (OCSP_resp_get0_signer(&basicResponse, &signer, nullptr) != 1)
        {
            return nullptr;
        }

        if (OCSP_basic_verify(&basicResponse, nullptr, nullptr, OCSP_NOVERIFY) != 1)
        {
            return nullptr;
        }
        return signer;
    }


    bool isSignerInTsl (
            X509* signer,
            const TrustStore& trustStore,
            const std::optional<std::vector<X509Certificate>>& ocspSignerCertificates)
    {
        if (ocspSignerCertificates.has_value())
        {
            for (const auto& certificate : ocspSignerCertificates.value())
            {
                if (X509_cmp(signer, certificate.getX509ConstPtr()) == 0)
                {
                    return true;
                }
            }
        }

        if (trustStore.isOcspResponderInTsl(signer))
        {
            return true;
        }

        if (trustStore.getTslMode() == TslMode::BNA)
        {
            // in case of QES Certificate the OCSP Response can be signed by other CAs
            std::optional<TrustStore::CaInfo> caInfo =
                trustStore.lookupCaCertificate(X509Certificate::createFromX509Pointer(signer));
            if (caInfo.has_value() && caInfo->certificate.hasX509Ptr())
            {
                TslExpect6(caInfo->accepted,
                           "OCSP-Issuer CA is revoked in BNetzA-VL.",
                           CA_CERTIFICATE_REVOKED_IN_BNETZA_VL,
                           TslMode::BNA,
                           trustStore.getIdOfTslInUse(),
                           trustStore.getSequenceNumberOfTslInUse());

                return true;
            }
        }

        return false;
    }


    /**
     * Checks the validity and plausibility of the 'producedAt' info from the OCSP
     * response.
     *
     * @param producedAt is the time point, when the OCSP response was signed.
     * @param now        is the current time point
     * @paramm gracePeriod gracePeriod for certificate
     * @see gemSpec_PKI, § 9.1.2.2 OCSP-Response - Zeiten, GS-A_5215, (a) and (b)
     */
    bool checkProducedAt (const ASN1_GENERALIZEDTIME* producedAt, std::time_t now, const std::chrono::seconds& gracePeriod)
    {
        Expect(producedAt != nullptr, "producedAt must not be null");

        if (ASN1_GENERALIZEDTIME_check(producedAt) != 1)
        {
            return false;
        }

        // the OCSP forwarder may cache
        std::time_t timePoint = now - tolerance - std::chrono::duration_cast<std::chrono::seconds>(gracePeriod).count();
        // (a) ensure, that the OCSP response is not older than (tolerance + gracePeriod)
        if (X509_cmp_time(producedAt, &timePoint) < 0)
        {
            return false;   // producedAt + gracePeriod <= now - tolerance
        }
        timePoint = now + tolerance;

        // (b) ensure, that the OCSP response is not signed in the future
        return X509_cmp_time(producedAt, &timePoint) <= 0;  // i.e.: producedAt <= now + tolerance
    }


    /**
     * Checks the validity and plausibility of the two ASN1_GENERALIZEDTIME values
     * 'thisUpdate' and 'nextUpdate' that were contained in an OCSP response.
     *
     * @param thisUpdate is the time point, when the status information is correct
     * @param nextUpdate is the time point, when new status information will be
     *                   available
     * @param now        is the current time point
     * @paramm gracePeriod gracePeriod for certificate
     *
     * @see gemSpec_PKI, § 9.1.2.2 OCSP-Response - Zeiten, GS-A_5215, (c) and (d)
     */
    bool
    checkValidity (const ASN1_GENERALIZEDTIME* thisUpdate, const ASN1_GENERALIZEDTIME* nextUpdate,
                   std::time_t now, const std::chrono::seconds& gracePeriod)
    {
        Expect(thisUpdate != nullptr, "thisUpdate must not be null");

        if (ASN1_GENERALIZEDTIME_check(thisUpdate) != 1)
        {
            return false;
        }

        std::time_t timePoint = now + tolerance;
        // (c) ensure, that the status information is already valid
        if (X509_cmp_time(thisUpdate, &timePoint) > 0)
        {
            return false;       // thisUpdate > now + tolerance
        }

        // nextUpdate may be omitted
        if (!nextUpdate)
        {
            return true;
        }

        if (ASN1_GENERALIZEDTIME_check(nextUpdate) != 1)
        {
            return false;
        }

        // the OCSP forwarder may cache
        timePoint = now - tolerance - std::chrono::duration_cast<std::chrono::seconds>(gracePeriod).count();
        // (d) ensure, that the status information was Called getOcspResponseString
        // still valid (tolerance + gracePeriod) ticks
        // ago, i.e.: nextUpdate + gracePeriod > now - tolerance
        return X509_cmp_time(nextUpdate, &timePoint) >= 0;
    }


    /// Checks, if the cert hash extension is present in singleResponse and if the value
    /// is identical to the calculated hash of the certificate
    void
    checkCertHashExtension (OCSP_SINGLERESP* singleResponse, const X509Certificate& certificate, const TslMode mode)
    {
        Expect(singleResponse != nullptr, "singleResponse must not be null");

        // search for cert hash single extension
        const Asn1ObjectPtr obj{OBJ_txt2obj("1.3.36.8.3.13", 1), ASN1_OBJECT_free};
        Expect(obj != nullptr, "can not create txt object");
        const auto extensionLocation{OCSP_SINGLERESP_get_ext_by_OBJ(singleResponse, obj.get(), -1)};
        TslExpect4(extensionLocation >= 0,
                   "OCSP response - cert hash extension is missing",
                   TslErrorCode::CERTHASH_EXTENSION_MISSING,
                   mode);

        X509_EXTENSION* extension{OCSP_SINGLERESP_get_ext(singleResponse, extensionLocation)};
        Expect(extension, "OCSP response - cert hash extension could not be read");

        const ASN1_OCTET_STRING* extensionData{X509_EXTENSION_get_data(extension)};
        Expect(extensionData, "OCSP response - cert hash extension data could not be read");
        Expect(extensionData->data != nullptr,
               "OCSP response - cert hash extension data should not be null");

        const unsigned char* buffer = extensionData->data;
        OcspHelper::Asn1SequencePtr extensionDataPtr{
                d2i_ASN1_SEQUENCE_ANY(nullptr, &buffer, extensionData->length)};

        Expect(extensionDataPtr && sk_ASN1_TYPE_num(extensionDataPtr.get()) > 1,
            "OCSP response - cert hash extension data ptr could not be read");

        // parse cert hash single extension
        const EVP_MD* digest = OcspHelper::getDigestFromExtension(extensionDataPtr.get());
        Expect(digest, "OCSP response - cert hash extension - digest could not be read");

        const ASN1_OCTET_STRING* certHashValueOctet{
                OcspHelper::getCertHashValueFromExtension(extensionDataPtr.get())};
        Expect(certHashValueOctet, "OCSP response - cert hash extension - hash value could not be read");

        // calculate hash of certificate
        unsigned char messageDigest[EVP_MAX_MD_SIZE];
        unsigned int digestLength;
        Expect(X509_digest(certificate.getX509ConstPtr(), digest, messageDigest, &digestLength) == 1
                    && gsl::narrow<int>(digestLength) == EVP_MD_size(digest),
               "OCSP response - cert hash extension - length of hash values not matching");

        // compare calculated hash to value from cert hash extension
        TslExpect4(gsl::narrow<int>(digestLength) == certHashValueOctet->length
                     && std::equal(certHashValueOctet->data,
                         certHashValueOctet->data + digestLength, messageDigest),
                   "OCSP response - cert hash extension - hash values not matching",
                   TslErrorCode::CERTHASH_MISMATCH,
                   mode);
    }


    /// convert ASN1_GENERALIZEDTIME into time_point
    std::chrono::system_clock::time_point
    toTimePoint (const ASN1_GENERALIZEDTIME* asn1Generalizedtime)
    {
        Expect(asn1Generalizedtime != nullptr, "asn1Generalizedtime must not be null");
        struct tm tm{};
        ASN1_TIME_to_tm(asn1Generalizedtime, &tm);
        return model::Timestamp::fromTmInUtc(tm).toChronoTimePoint();
    }


    std::tuple<OcspService::Status, std::chrono::seconds, std::chrono::system_clock::time_point>
    parseResponse (
            const X509Certificate& certificate,
            const OcspResponsePtr& response,
            OCSP_CERTID* id,
            const TrustStore& trustStore,
            const std::optional<std::vector<X509Certificate>>& ocspSignerCertificates,
            const bool validateHashExtension)
    {
        try {
            TslExpect4(id != nullptr,
                       "id must not be null",
                       TslErrorCode::PROVIDED_OCSP_RESPONSE_NOT_VALID,
                       trustStore.getTslMode());

            TslExpect4(OCSP_response_status(response.get()) == OCSP_RESPONSE_STATUS_SUCCESSFUL,
                       "Unexpected OCSP status",
                       TslErrorCode::OCSP_STATUS_ERROR,
                       trustStore.getTslMode());

            const std::shared_ptr<OCSP_BASICRESP> basicResponse{
                OCSP_response_get1_basic(response.get()),
                OCSP_BASICRESP_free};
            Expect(basicResponse.operator bool(), "Can not get basic data from OCSP response.");

            // gemSpec_PKI § 9.1.2.2 OCSP-Response Zeiten
            // GEMREQ-start GS-A_5215
            const ASN1_GENERALIZEDTIME* producedAt{OCSP_resp_get0_produced_at(basicResponse.get())};
            Expect(producedAt != nullptr, "can not get producedAt");
            const std::time_t now{std::time(nullptr)};
            const std::chrono::seconds gracePeriod = getGracePeriod(trustStore.getTslMode());

            TslExpect4(checkProducedAt(producedAt, now, gracePeriod),
                       "OCSP producedAt is not valid",
                       TslErrorCode::PROVIDED_OCSP_RESPONSE_NOT_VALID,
                       trustStore.getTslMode());

            X509* signer{checkSignatureAndGetSigner(*basicResponse)};
            TslExpect4(signer,
                      "OCSP Signature check has failed",
                      TslErrorCode::OCSP_SIGNATURE_ERROR,
                      trustStore.getTslMode());

            TslExpect4(isSignerInTsl(signer, trustStore, ocspSignerCertificates),
                       "OCSP Signer cert not found in TrustServiceStatusList",
                       TslErrorCode::OCSP_CERT_MISSING,
                       trustStore.getTslMode());

            const int index{OCSP_resp_find(basicResponse.get(), id, -1)};
            TslExpect4(index >= 0,
                       "Certificate ID not found in OCSP response",
                       TslErrorCode::OCSP_CHECK_REVOCATION_ERROR,
                       trustStore.getTslMode());

            OCSP_SINGLERESP* singleResponse{OCSP_resp_get0(basicResponse.get(), index)};
            Expect(singleResponse != nullptr, "can not get response");

            // OCSP_single_get0_status also sets revocationTime, thisUpdate and nextUpdate
            ASN1_GENERALIZEDTIME* revocationTime{nullptr};
            ASN1_GENERALIZEDTIME* thisUpdate{nullptr};
            ASN1_GENERALIZEDTIME* nextUpdate{nullptr};
            const int status{
                OCSP_single_get0_status(singleResponse, nullptr, &revocationTime, &thisUpdate,
                                        &nextUpdate)};

            TslExpect4(checkValidity(thisUpdate, nextUpdate, now, gracePeriod),
                       "OCSP update time is not plausible.",
                       TslErrorCode::PROVIDED_OCSP_RESPONSE_NOT_VALID,
                       trustStore.getTslMode());
            // GEMREQ-end GS-A_5215

            if ((status == V_OCSP_CERTSTATUS_REVOKED || status == V_OCSP_CERTSTATUS_GOOD)
                && validateHashExtension)
            {
                checkCertHashExtension(singleResponse, certificate, trustStore.getTslMode());
            }

            if (status == V_OCSP_CERTSTATUS_REVOKED)
            {
                return std::make_tuple(OcspService::Status{OcspService::CertificateStatus::revoked, toTimePoint(revocationTime)},
                                       gracePeriod,
                                       toTimePoint(producedAt));
            }
            else if (status == V_OCSP_CERTSTATUS_GOOD)
            {
                return std::make_tuple(OcspService::Status{OcspService::CertificateStatus::good, {}},
                                       gracePeriod,
                                       toTimePoint(producedAt));
            }

            return std::make_tuple(OcspService::Status{OcspService::CertificateStatus::unknown, {}},
                                   gracePeriod,
                                   toTimePoint(producedAt));
        }
        catch (const TslError&)
        {
            throw;
        }
        catch (const std::runtime_error& e)
        {
            TslFail3(std::string("OCSP Response is invalid: ") + e.what(),
                     TslErrorCode::PROVIDED_OCSP_RESPONSE_NOT_VALID,
                     trustStore.getTslMode());
        }
    }


    /**
     * Gets the OCSP status of a certificate.
     *
     * @param certificate   the certificate to check
     * @param issuer        the issuer of the certificate
     * @param requestSender the OCSP request will be sent via this sender
     * @param ocspPath      HTTP path for the OCSP request
     * @param trustStore    contains cached OCSP responses
     * @param ocspSignerCertificates  the certificates of the OCSP
     *                      responders as taken from the TSL
     * @param isResponseRequired if false then response will remain empty if a cached status is found
     *                      if true a cached status is ignored and a lookup is always made to produce the requested OCSP response
     * @param validateHashExtension whether the response hash extension should be validated
     *
     * @return  pair of OCSP status (good / revoked / unknown) plus revocation time and the ocsp response
     * @throws OcspService::OcspError on error
     */
    std::pair<OcspService::Status, util::Buffer> getCurrentStatusAndResponse (
            const X509Certificate& certificate,
            const X509Certificate& issuer,
            const UrlRequestSender& requestSender,
            const OcspUrl& ocspUrl,
            TrustStore& trustStore,
            const std::optional<std::vector<X509Certificate>>& ocspSignerCertificates,
            const bool isResponseRequired,
            const bool validateHashExtension)
    {
        VLOG(2) << "OcspService: getCurrentStatusAndResponse";

        Expect( certificate.hasX509Ptr() && issuer.hasX509Ptr(),
                "Invalid certificate is provided for OCSP verification.");

        if ( ! isResponseRequired)
        {
            const auto cachedStatus = trustStore.getCachedOcspStatus(certificate.getSha256FingerprintHex());
            if (cachedStatus)
            {
                VLOG(2) << "Returning cached OCSP status: " << cachedStatus->to_string();
                return {cachedStatus.value(), {}};
            }
        }

        auto [request, id] = createOcspRequest(certificate, issuer);

        VLOG(2) << "Sending new OCSP request with URL: " << ocspUrl.url;

        const std::string response{sendOcspRequest(requestSender, ocspUrl, toBuffer(request))};
        auto ocspResponse{toOcspResponse(response)};

        TslExpect4(ocspResponse.operator bool(),
                   "Can not parse OCSP response",
                   TslErrorCode::PROVIDED_OCSP_RESPONSE_NOT_VALID,
                   trustStore.getTslMode());

        const auto [newStatus, gracePeriod, producedAt] =
            parseResponse(certificate, ocspResponse, id,
                          trustStore, ocspSignerCertificates, validateHashExtension);

        VLOG(2) << "Returning new OCSP status: " << newStatus.to_string();

        trustStore.setCacheOcspStatus(certificate.getSha256FingerprintHex(), newStatus, gracePeriod, producedAt);
        return {newStatus, util::stringToBuffer(response)};
    }
}   // anonymous namespace


OcspService::Status
OcspService::getCurrentStatus (const X509Certificate& certificate,
                               const UrlRequestSender& requestSender,
                               const OcspUrl& ocspUrl,
                               TrustStore& trustStore,
                               const bool validateHashExtension)
{
    const auto caInfo = trustStore.lookupCaCertificate(certificate);
    Expect(caInfo.has_value(), "CA of the certificate must be in trust store.");
    return ::getCurrentStatusAndResponse(certificate, caInfo->certificate,
                                         requestSender, ocspUrl, trustStore,
                                         std::nullopt,
                                         false,
                                         validateHashExtension).first;
}


OcspService::Status
OcspService::getCurrentStatusOfTslSigner (const X509Certificate& certificate,
                                          const X509Certificate& issuerCertificate,
                                          const UrlRequestSender& requestSender,
                                          const OcspUrl& ocspUrl,
                                          TrustStore& oldTrustStore,
                                          const std::optional<std::vector<X509Certificate>>& ocspSignerCertificates,
                                          const bool validateHashExtension)
{
    for (const X509Certificate& tslSignerCa : oldTrustStore.getTslSignerCas())
    {
        // Find the TSL signer CA certificate that this certificate was signed by...
        if (certificate.signatureIsValidAndWasSignedBy(tslSignerCa))
        {
            // ...got it - now we can use it as the issuer certificate in our OCSP request.
            return ::getCurrentStatusAndResponse(certificate,
                                                 issuerCertificate,
                                                 requestSender,
                                                 ocspUrl,
                                                 oldTrustStore,
                                                 ocspSignerCertificates,
                                                 false,
                                                 validateHashExtension).first;
        }
    }

    // If the certificate was not signed by any of the TSL signer certificates, then we don't even
    // send an OCSP request because we should never have gotten here.
    LogicErrorFail("getCurrentStatusOfTslSigner must only be called for certificates that "
                   "are signed by one of the TSL signer certificates");

}


std::string OcspService::toString (OcspService::CertificateStatus certificateStatus)
{
    switch (certificateStatus)
    {
        case CertificateStatus::good:
            return statusGood;
        case CertificateStatus::revoked:
            return statusRevoked;
        case CertificateStatus::unknown:
            return statusUnknown;
        default:
            throw std::logic_error{"Certificate Status is invalid"};
    }
}


OcspService::CertificateStatus OcspService::toCertificateStatus (const std::string& string)
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
