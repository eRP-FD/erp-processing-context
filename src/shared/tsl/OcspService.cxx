/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/tsl/OcspService.hxx"
#include "shared/beast/BoostBeastStringWriter.hxx"
#include "shared/network/client/HttpClient.hxx"
#include "shared/network/client/UrlRequestSender.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/tsl/OcspCheckDescriptor.hxx"
#include "shared/tsl/OcspHelper.hxx"
#include "shared/tsl/TrustStore.hxx"
#include "shared/tsl/error/TslError.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Buffer.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/GLog.hxx"
#include "shared/util/JsonLog.hxx"
#include "shared/util/String.hxx"
#include "shared/util/UrlHelper.hxx"

#include <cctype>
#include <ctime>
#include <utility>


#undef Expect
#define Expect(expression, message) Expect3(expression, message, OcspService::OcspError)

namespace
{
    // value "Toleranz t" from GS-A_5215
    constexpr const int tolerance = 37;

    OcspRequestPtr
    createOcspRequest (const OcspCertidPtr& certId)
    {
        Expect(certId != nullptr, "OCSP certificate ID must be provided");

        OcspRequestPtr request{OCSP_REQUEST_new()};
        Expect(request != nullptr, "Could not create OCSP request!");

        OcspCertidPtr id(OCSP_CERTID_dup(certId.get()));
        Expect(id, "Could not create OCSP request!");

        if (!OCSP_request_add0_id(request.get(), id.get()))
        {
            Fail2("Could not create OCSP request!", OcspService::OcspError);
        }

        // after this, id will be part of request, so it does no longer have to be freed
        id.release(); // NOLINT(bugprone-unused-return-value)
        return request;
    }


    util::Buffer toBuffer (const OcspRequestPtr& ocspRequest)
    {
        unsigned char* buffer = nullptr;
        const int bufferLength = i2d_OCSP_REQUEST(ocspRequest.get(), &buffer);
        Expect(bufferLength > 0, "Could not create OCSP request!");

        std::unique_ptr<unsigned char, void(*)(unsigned char*)> bufferPtr(
            buffer,
            [](unsigned char* pointer) -> void {OPENSSL_free(pointer);});

        return util::Buffer{util::rawToBuffer(buffer, static_cast<size_t>(bufferLength))};
    }


    // GEMREQ-start A_20159-03#sendOcspRequest
    std::string sendOcspRequest (const UrlRequestSender& requestSender,
                                 const OcspUrl& url,
                                 const util::Buffer& request)
    {
        const auto urlParts = UrlHelper::parseUrl(url.url);
        auto timer = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryOcspRequest, urlParts.mHost);
        timer.keyValue("url", url.url);

        try
        {
            const std::string contentType = "application/ocsp-request";

            TVLOG(2) << "OCSP request (base64-encoded) on Url: " << url.url << "\n"
                    << Base64::encode(request) << "\n\n";

            const auto response = requestSender.send(
                url.url,
                HttpMethod::POST,
                std::string(reinterpret_cast<const char*>(request.data()), request.size()),
                contentType);

            TVLOG(2) << "OCSP response, status=" << response.getHeader().status()
                    << " (base-encoded):\n" << Base64::encode(response.getBody()) << "\n\n";

            timer.keyValue("response_code", std::to_string(toNumericalValue(response.getHeader().status())));

            if (response.getHeader().status() != HttpStatus::OK)
            {
                timer.notifyFailure(BoostBeastStringWriter::serializeResponse(response.getHeader(),
                                                                              Base64::encode(response.getBody())));
                Fail(std::string("OCSP call failed, status=") + toString(response.getHeader().status()));
            }

            return response.getBody();
        }
        catch (const std::exception& e)
        {
            timer.notifyFailure(e.what());
            TslFailWithStatus(e.what() + std::string(", ocsp url: ") + url.url,
                              TslErrorCode::OCSP_NOT_AVAILABLE,
                              url.isRequestData ? HttpStatus::BadRequest : HttpStatus::BackendCallFailed,
                              trustStore.getTslMode());
        }
    }
    // GEMREQ-end A_20159-03#sendOcspRequest


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


    std::chrono::system_clock::time_point getReferenceTimePoint(
        const std::optional<std::chrono::system_clock::time_point>& referenceTimePoint)
    {
        if (referenceTimePoint.has_value())
            return *referenceTimePoint;
        return std::chrono::system_clock::now();
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
     * @param referenceTimePoint the reference time point to use for the check
     * @paramm gracePeriod gracePeriod for certificate
     * @see gemSpec_PKI, § 9.1.2.2 OCSP-Response - Zeiten, GS-A_5215, (a) and (b)
     */
    bool checkProducedAt (
        const ASN1_GENERALIZEDTIME* producedAt,
        std::time_t referenceTimePoint,
        const std::chrono::system_clock::duration& gracePeriod)
    {
        Expect(producedAt != nullptr, "producedAt must not be null");

        if (ASN1_GENERALIZEDTIME_check(producedAt) != 1)
        {
            return false;
        }

        // the OCSP forwarder may cache
        std::time_t timePoint =
            referenceTimePoint - tolerance - std::chrono::duration_cast<std::chrono::seconds>(gracePeriod).count();
        // (a) ensure, that the OCSP response is not older than (tolerance + gracePeriod) for the reference time point
        if (X509_cmp_time(producedAt, &timePoint) < 0)
        {
            return false;   // producedAt + gracePeriod <= now - tolerance
        }

        // (b) ensure, that the OCSP response is not signed in the future
        std::time_t now{std::time(nullptr)};
        timePoint = now + tolerance;
        return X509_cmp_time(producedAt, &timePoint) <= 0;  // i.e.: producedAt <= now + tolerance
    }


    /**
     * Checks the validity and plausibility of the two ASN1_GENERALIZEDTIME values
     * 'thisUpdate' and 'nextUpdate' that were contained in an OCSP response.
     *
     * @param thisUpdate is the time point, when the status information is correct
     * @param nextUpdate is the time point, when new status information will be
     *                   available
     * @param referenceTimePoint the reference time point to use for the check
     * @param gracePeriod gracePeriod for certificate
     *
     * @see gemSpec_PKI, § 9.1.2.2 OCSP-Response - Zeiten, GS-A_5215, (c) and (d)
     */
    bool
    checkValidity (
        const ASN1_GENERALIZEDTIME* thisUpdate,
        const ASN1_GENERALIZEDTIME* nextUpdate,
        std::time_t referenceTimePoint,
        const std::chrono::system_clock::duration& gracePeriod)
    {
        Expect(thisUpdate != nullptr, "thisUpdate must not be null");

        if (ASN1_GENERALIZEDTIME_check(thisUpdate) != 1)
        {
            return false;
        }

        std::time_t now{std::time(nullptr)};
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
        timePoint =
            referenceTimePoint - tolerance - std::chrono::duration_cast<std::chrono::seconds>(gracePeriod).count();
        // (d) ensure, that the status information was Called getOcspResponseString
        // still valid (tolerance + gracePeriod) ticks
        // ago for the reference time point, i.e.: nextUpdate + gracePeriod > referenceTimePoint - tolerance
        return X509_cmp_time(nextUpdate, &timePoint) >= 0;
    }


    /// Checks, if the cert hash extension is present in singleResponse and if the value
    /// is identical to the calculated hash of the certificate
    void
    checkCertHashExtension (OCSP_SINGLERESP* singleResponse, const X509Certificate& certificate, const TslMode mode)
    {
        Expect(singleResponse != nullptr, "singleResponse must not be null");

        // search for cert hash single extension
        const Asn1ObjectPtr obj{OBJ_txt2obj("1.3.36.8.3.13", 1)};
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
        unsigned int digestLength{0};
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

    CertificateStatus certificateStatusFromSSL(int status)
    {
        switch (status)
        {
            case V_OCSP_CERTSTATUS_REVOKED:
                return CertificateStatus::revoked;
            case V_OCSP_CERTSTATUS_GOOD:
                return CertificateStatus::good;
            case V_OCSP_CERTSTATUS_UNKNOWN:
                return CertificateStatus::unknown;
            default: // V_OCSP_CERTSTATUS_* are pre-processor-macros so we have to provide a default here to silence clang-tidy
                break;
        }
        TLOG(INFO) << "unexpected value for OCSP-Status";
        return CertificateStatus::unknown;
    }

    OcspResponse
    parseResponse (
            const X509Certificate& certificate,
            const OcspResponsePtr& response,
            const OcspCertidPtr& id,
            const OcspCheckDescriptor::TimeSettings& timeSettings,
            const TrustStore& trustStore,
            const std::optional<std::vector<X509Certificate>>& ocspSignerCertificates,
            const bool validateHashExtension,
            bool validateProducedAt)
    {
        try {
            TslExpect4(response != nullptr,
                       "Can not parse OCSP response",
                       TslErrorCode::PROVIDED_OCSP_RESPONSE_NOT_VALID,
                       trustStore.getTslMode());

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

            if (validateProducedAt)
            {
                TslExpect4(checkProducedAt(producedAt,
                                           std::chrono::system_clock::to_time_t(
                                               getReferenceTimePoint(timeSettings.referenceTimePoint)),
                                           timeSettings.gracePeriod),
                           "OCSP producedAt is not valid", TslErrorCode::PROVIDED_OCSP_RESPONSE_NOT_VALID,
                           trustStore.getTslMode());
            }

            X509* signer{checkSignatureAndGetSigner(*basicResponse)};
            TslExpect4(signer,
                      "OCSP Signature check has failed",
                      TslErrorCode::OCSP_SIGNATURE_ERROR,
                      trustStore.getTslMode());

            TslExpect4(isSignerInTsl(signer, trustStore, ocspSignerCertificates),
                       "OCSP Signer cert not found in TrustServiceStatusList",
                       TslErrorCode::OCSP_CERT_MISSING,
                       trustStore.getTslMode());

            const int index{OCSP_resp_find(basicResponse.get(), id.get(), -1)};
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

            TslExpect4(checkValidity(thisUpdate, nextUpdate, now, timeSettings.gracePeriod),
                       "OCSP update time is not plausible.",
                       TslErrorCode::PROVIDED_OCSP_RESPONSE_NOT_VALID,
                       trustStore.getTslMode());
            // GEMREQ-end GS-A_5215

            if ((status == V_OCSP_CERTSTATUS_REVOKED || status == V_OCSP_CERTSTATUS_GOOD)
                && validateHashExtension)
            {
                checkCertHashExtension(singleResponse, certificate, trustStore.getTslMode());
            }
            const auto certStatus = certificateStatusFromSSL(status);
            OcspResponse response{
                .status =
                    {
                        .certificateStatus = certStatus,
                        .revocationTime = certStatus == CertificateStatus::revoked
                                            ? toTimePoint(revocationTime)
                                            : std::chrono::system_clock::time_point{},
                    },
                .gracePeriod = timeSettings.gracePeriod,
                .producedAt = toTimePoint(producedAt),
                .receivedAt = std::chrono::system_clock::now(),
                .fromCache = false,
                .response = {},
            };
            return response;
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


    OcspResponse requestStatus(
            OcspCertidPtr certId,
            const X509Certificate& certificate,
            const OcspResponsePtr& ocspResponse,
            const std::optional<std::string>& serializedOcspResponse,
            const OcspCheckDescriptor::TimeSettings& timeSettings,
            TrustStore& trustStore,
            const std::optional<std::vector<X509Certificate>>& ocspSignerCertificates,
            const bool validateHashExtension,
            bool validateProducedAt,
            bool allowCaching)
    {
        auto response = parseResponse(
                certificate,
                ocspResponse,
                certId,
                timeSettings,
                trustStore,
                ocspSignerCertificates,
                validateHashExtension,
                validateProducedAt);

        TVLOG(2) << "Returning new OCSP status: " << response.status.to_string();
        response.response = (serializedOcspResponse.has_value() ? *serializedOcspResponse
                                                                : OcspHelper::ocspResponseToString(*ocspResponse));
        if (allowCaching && response.status.certificateStatus != CertificateStatus::unknown)
        {
            // the returned OCSP response is only cached if the status is not unknown
            trustStore.setCacheOcspData(certificate.getSha256FingerprintHex(), response);
        }
        return response;
    }


    OcspResponse sendOcspRequestAndGetStatus(
            const X509Certificate& certificate,
            OcspCertidPtr certId,
            const UrlRequestSender& requestSender,
            const OcspUrl& ocspUrl,
            TrustStore& trustStore,
            const std::optional<std::vector<X509Certificate>>& ocspSignerCertificates,
            const bool validateHashExtension,
            const OcspCheckDescriptor& ocspCheckDescriptor)
    {
        OcspRequestPtr request = createOcspRequest(certId);
        TVLOG(2) << "Sending new OCSP request with URL: " << ocspUrl.url;
        const std::string response{
            sendOcspRequest(requestSender, ocspUrl, toBuffer(request))};
        OcspResponsePtr ocspResponse = OcspHelper::stringToOcspResponse(response);

        return requestStatus(
            std::move(certId),
            certificate,
            ocspResponse,
            response,
            ocspCheckDescriptor.timeSettings,
            trustStore,
            ocspSignerCertificates,
            validateHashExtension,
            true, // validateProducedAt
            true  // allowCaching
        );
    }


// GEMREQ-start A_20765-02#ocspCheckModes, A_20159-03#getCurrentResponseInternal
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
     * @param validateHashExtension whether the response hash extension should be validated
     * @param ocspCheckDescriptor describes the approach to do the OCSP check
     *
     * @return  OCSP status (good / revoked / unknown)
     * @throws OcspService::OcspError on error
     */
    OcspResponse getCurrentResponseInternal (
            const X509Certificate& certificate,
            const X509Certificate& issuer,
            const UrlRequestSender& requestSender,
            const OcspUrl& ocspUrl,
            TrustStore& trustStore,
            const std::optional<std::vector<X509Certificate>>& ocspSignerCertificates,
            const bool validateHashExtension,
            const OcspCheckDescriptor& ocspCheckDescriptor)
    {
        TVLOG(2) << "OcspService: getCurrentResponseInternal";

        Expect( certificate.hasX509Ptr() && issuer.hasX509Ptr(),
                "Invalid certificate is provided for OCSP verification.");

        OcspCertidPtr certId =
            OcspCertidPtr(OCSP_cert_to_id(nullptr, certificate.getX509ConstPtr(), issuer.getX509ConstPtr()));
        Expect(certId, "Could not create OCSP certificate id!");
        switch(ocspCheckDescriptor.mode)
        {
            case OcspCheckDescriptor::FORCE_OCSP_REQUEST_STRICT:
            case OcspCheckDescriptor::FORCE_OCSP_REQUEST_ALLOW_CACHE:
                try
                {
                    // send OCSP-request
                    return sendOcspRequestAndGetStatus(
                        certificate,
                        std::move(certId),
                        requestSender,
                        ocspUrl,
                        trustStore,
                        ocspSignerCertificates,
                        validateHashExtension,
                        ocspCheckDescriptor);
                }
                catch(const std::exception& e)
                {
                    // in case of error during request sending use cache fallback if allowed
                    if (ocspCheckDescriptor.mode == OcspCheckDescriptor::FORCE_OCSP_REQUEST_ALLOW_CACHE)
                    {
                        const auto cachedOcspData =
                            trustStore.getCachedOcspData(certificate.getSha256FingerprintHex());
                        if (cachedOcspData.has_value())
                        {
                            TVLOG(2) << "Returning cached OCSP status: " << cachedOcspData->status.to_string();
                            return *cachedOcspData;
                        }
                    }
                    throw;
                }
                break;
            // GEMREQ-start A_24913#outdatedResponse
            case OcspCheckDescriptor::PROVIDED_OR_CACHE_REQUEST_IF_OUTDATED:
            case OcspCheckDescriptor::PROVIDED_OR_CACHE: {
                if (ocspCheckDescriptor.providedOcspResponse != nullptr)
                {
                    bool validateProducedAt = (ocspCheckDescriptor.mode != OcspCheckDescriptor::PROVIDED_OR_CACHE_REQUEST_IF_OUTDATED);
                    // if there is a provided OCSP request it must be valid or an error must be reported
                    auto ocspResponse = requestStatus(
                        std::move(certId),
                        certificate,
                        ocspCheckDescriptor.providedOcspResponse,
                        std::nullopt,
                        ocspCheckDescriptor.timeSettings,
                        trustStore,
                        ocspSignerCertificates,
                        validateHashExtension,
                        validateProducedAt,
                        false // allowCaching
                    );
                    // validate the producedAt afterwards to check if we can fall
                    // back to cache
                    auto now = std::chrono::system_clock::now();
                    const auto oldestProducedAt = ocspCheckDescriptor.timeSettings.referenceTimePoint.value_or(now) -
                                                  std::chrono::seconds{tolerance} -
                                                  ocspCheckDescriptor.timeSettings.gracePeriod;
                    bool producedAtValid =
                        ocspResponse.producedAt >= oldestProducedAt && ocspResponse.producedAt <= now;
                    if (validateProducedAt || producedAtValid)
                    {
                        return ocspResponse;
                    }
                    TVLOG(1) << "Response is valid but outdated, fallback to cache";
                    // recreate the certId pointer which has been moved before
                    certId = OcspCertidPtr(
                        OCSP_cert_to_id(nullptr, certificate.getX509ConstPtr(), issuer.getX509ConstPtr()));
                }
                // if there is no provided OCSP request, or we skipped the expired response
                // first try the OCSP response from the cache
                const auto cachedOcspData =
                    trustStore.getCachedOcspData(certificate.getSha256FingerprintHex());
                if (cachedOcspData.has_value())
                {
                    TVLOG(2) << "Returning cached OCSP status: " << cachedOcspData->status.to_string();
                    return *cachedOcspData;
                }

                // and if there is no valid OCSP response in the cache,
                // send OCSP-request
                return sendOcspRequestAndGetStatus(
                    certificate,
                    std::move(certId),
                    requestSender,
                    ocspUrl,
                    trustStore,
                    ocspSignerCertificates,
                    validateHashExtension,
                    ocspCheckDescriptor);
                break;
            }
            // GEMREQ-end A_24913#outdatedResponse
            case OcspCheckDescriptor::CACHED_ONLY:
                {
                    const auto cachedOcspData = trustStore.getCachedOcspData(certificate.getSha256FingerprintHex());
                    TslExpect4(cachedOcspData.has_value(), "OCSP Response not in cache", TslErrorCode::OCSP_NOT_AVAILABLE,
                            trustStore.getTslMode());
                    TVLOG(2) << "Returning cached OCSP status: " << cachedOcspData->status.to_string();
                    return *cachedOcspData;
                }
            case OcspCheckDescriptor::PROVIDED_ONLY:
                TslExpect4(ocspCheckDescriptor.providedOcspResponse != nullptr, "OCSP Response not provided", TslErrorCode::OCSP_NOT_AVAILABLE,
                        trustStore.getTslMode());
                // if there is a provided OCSP request it must be valid or an error must be reported
                return requestStatus(
                    std::move(certId),
                    certificate,
                    ocspCheckDescriptor.providedOcspResponse,
                    std::nullopt,
                    ocspCheckDescriptor.timeSettings,
                    trustStore,
                    ocspSignerCertificates,
                    validateHashExtension,
                    true,
                    false // allowCaching
                );
        }
        Fail("Invalid value for OcspCheckMode: " + std::to_string(static_cast<uintmax_t>(ocspCheckDescriptor.mode)));
    }
// GEMREQ-end A_20765-02#ocspCheckModes, A_20159-03#getCurrentResponseInternal


    OcspCheckDescriptor getTslSignerOcspCheckDescriptor()
    {
        std::chrono::seconds gracePeriod(
            Configuration::instance().getIntValue(ConfigurationKey::OCSP_NON_QES_GRACE_PERIOD));

        return {
            OcspCheckDescriptor::OcspCheckMode::PROVIDED_OR_CACHE,
            {.referenceTimePoint = std::nullopt, .gracePeriod = gracePeriod},
            {}};
    }
}   // anonymous namespace


// GEMREQ-start A_20159-03#getCurrentResponse
OcspResponse
OcspService::getCurrentResponse (const X509Certificate& certificate,
                               const UrlRequestSender& requestSender,
                               const OcspUrl& ocspUrl,
                               TrustStore& trustStore,
                               const bool validateHashExtension,
                               const OcspCheckDescriptor& ocspCheckDescriptor)
{
    const auto caInfo = trustStore.lookupCaCertificate(certificate);
    Expect(caInfo.has_value(), "CA of the certificate must be in trust store.");
    return ::getCurrentResponseInternal(certificate, caInfo->certificate,
                                         requestSender, ocspUrl, trustStore,
                                         std::nullopt,
                                         validateHashExtension,
                                         ocspCheckDescriptor);
}
// GEMREQ-end A_20159-03#getCurrentResponse


OcspStatus
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
            return ::getCurrentResponseInternal(certificate,
                                                 issuerCertificate,
                                                 requestSender,
                                                 ocspUrl,
                                                 oldTrustStore,
                                                 ocspSignerCertificates,
                                                 validateHashExtension,
                                                 getTslSignerOcspCheckDescriptor()).status;
        }
    }

    // If the certificate was not signed by any of the TSL signer certificates, then we don't even
    // send an OCSP request because we should never have gotten here.
    LogicErrorFail("getCurrentStatusOfTslSigner must only be called for certificates that "
                   "are signed by one of the TSL signer certificates");

}
