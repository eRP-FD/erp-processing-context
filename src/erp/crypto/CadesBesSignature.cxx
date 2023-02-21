/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/crypto/CadesBesSignature.hxx"

#include "erp/crypto/Certificate.hxx"
#include "erp/crypto/EssCertificateHelper.hxx"
#include "erp/pc/ProfessionOid.hxx"
#include "erp/tsl/OcspHelper.hxx"
#include "erp/tsl/TslManager.hxx"
#include "erp/util/Base64.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "erp/ErpRequirements.hxx"


namespace
{
    class SignerCertificateInfo
    {
    public:
        X509* certificate{};
        std::optional<model::Timestamp> signingTimestamp;
    };


    const auto releaseList = [] (STACK_OF(X509)* lst) {
        sk_X509_pop_free(lst, X509_free);
    };


    bool hasNoCertificate(CMS_SignerInfo& signerInfo)
    {
        X509* signer = nullptr;
        CMS_SignerInfo_get0_algs(&signerInfo, nullptr, &signer, nullptr, nullptr);
        return (signer == nullptr);
            return true;

        return false;
    }


    void setSignerInfoOnDemand(
        CMS_ContentInfo& cmsContentInfo,
        CMS_SignerInfo& signerInfo,
        STACK_OF(X509)* certificates)
    {
        if (hasNoCertificate(signerInfo))
        {
            CMS_set1_signers_certs(&cmsContentInfo, certificates, 0);
        }
    }


    void verifySignerInfo(CMS_SignerInfo& signerInfo)
    {
        Expect3(CMS_signed_get_attr_by_NID(&signerInfo, NID_pkcs9_contentType, -1) >= 0,
                "No content type in signed info.",
                CadesBesSignature::VerificationException);
        Expect3(CMS_signed_get_attr_by_NID(&signerInfo, NID_pkcs9_signingTime, -1) >= 0,
                "No signing time in signed info.",
                CadesBesSignature::VerificationException);
        Expect3(CMS_signed_get_attr_by_NID(&signerInfo, NID_pkcs9_messageDigest, -1) >= 0,
                "No message digest in signed info.",
                CadesBesSignature::VerificationException);

        Expect3(CMS_signed_get_attr_by_NID(&signerInfo, NID_id_smime_aa_signingCertificate, -1) >= 0
                  || CMS_signed_get_attr_by_NID(&signerInfo, NID_id_smime_aa_signingCertificateV2, -1) >= 0,
                "No certificate in signed info.",
                CadesBesSignature::VerificationException);
    }


    std::optional<model::Timestamp> getSigningTimeFromSignerInfo(CMS_SignerInfo& signerInfo)
    {
        const int attributeIndex = CMS_signed_get_attr_by_NID(&signerInfo, NID_pkcs9_signingTime, -1);
        if (attributeIndex != -1)
        {
            auto* x509Attributes = CMS_signed_get_attr(&signerInfo, attributeIndex);
            ASN1_TYPE* asn1Type = X509_ATTRIBUTE_get0_type(x509Attributes, 0);
            if (asn1Type != nullptr)
            {
                OpenSslExpect(asn1Type->type == V_ASN1_UTCTIME && asn1Type->value.utctime != nullptr,
                              "Unexpected type of signingTime.");
                tm tm{};
                OpenSslExpect(ASN1_TIME_to_tm(asn1Type->value.utctime, &tm) == 1,
                              "Failed to convert ASN1_UTCTIME to struct tm");
                return model::Timestamp::fromTmInUtc(tm);
            }
        }

        return {};
    }


    CMS_SignerInfo& getSignerInfo(CMS_ContentInfo& cmsContentInfo)
    {
        auto* signerInfos = CMS_get0_SignerInfos(&cmsContentInfo);
        OpenSslExpect(signerInfos != nullptr, "No signer infos is provided.");

        const int signerInfosCount = sk_CMS_SignerInfo_num(signerInfos);
        OpenSslExpect(signerInfosCount > 0, "No signer infos provided.");
        Expect(signerInfosCount == 1,
               "Only one signer info is expected. Multiple signatures are not expected in CAdES-BES packet.");

        CMS_SignerInfo* signerInfo = sk_CMS_SignerInfo_value(signerInfos, 0);
        OpenSslExpect(signerInfo != nullptr, "Signer info must be provided.");

        return *signerInfo;
    }


    SignerCertificateInfo getSignerCertificate(
        CMS_ContentInfo& cmsContentInfo, STACK_OF(X509)* certificates)
    {
        CMS_SignerInfo& signerInfo = getSignerInfo(cmsContentInfo);

        setSignerInfoOnDemand(cmsContentInfo, signerInfo, certificates);

        std::vector<SignerCertificateInfo> signerCertificates;

        verifySignerInfo(signerInfo);

        X509* signerCertificate = nullptr;
        CMS_SignerInfo_get0_algs(&signerInfo, nullptr, &signerCertificate, nullptr, nullptr);
        OpenSslExpect(signerCertificate != nullptr, "No signer certificate.");

        EssCertificateHelper::verifySigningCertificateFromSignedData(signerInfo, *signerCertificate);

        return SignerCertificateInfo{signerCertificate, getSigningTimeFromSignerInfo(signerInfo)};
    }


    void addCertificateToSignerInfo(
        CMS_ContentInfo& cmsContentInfo, const Certificate& certificate)
    {
        CMS_SignerInfo& signerInfo = getSignerInfo(cmsContentInfo);

        if (CMS_signed_get_attr_by_NID(&signerInfo, NID_id_smime_aa_signingCertificate, -1) < 0 &&
            CMS_signed_get_attr_by_NID(&signerInfo, NID_id_smime_aa_signingCertificateV2, -1) < 0)
        {
            // add signing certificate to the signerInfo
            EssCertificateHelper::setSigningCertificateInSignedData(signerInfo, *certificate.toX509().removeConst());
        }
    }


    ASN1_TYPE& getMessageDigestAsn1Type(CMS_SignerInfo& signerInfo)
    {
        const int index = CMS_signed_get_attr_by_NID(&signerInfo, NID_pkcs9_messageDigest, -1);
        OpenSslExpect(index >= 0, "No message digest in signed info.");

        X509_ATTRIBUTE* digestAttribute = CMS_signed_get_attr(&signerInfo, index);
        OpenSslExpect(digestAttribute != nullptr, "The digest attribute must not be null.");

        ASN1_TYPE* asn1Type = X509_ATTRIBUTE_get0_type(digestAttribute, 0);
        OpenSslExpect(asn1Type != nullptr, "ASN1_TYPE pointer must not be null");
        OpenSslExpect((asn1Type->type == V_ASN1_OCTET_STRING) && asn1Type->value.octet_string,
                      "Unexpected type of messageDigest.");

        return *asn1Type;
    }


    int getOcspResponseRevocationNid()
    {
        static const int nidOcspResponseRevocationContainer =
            OBJ_create("1.3.6.1.5.5.7.16.2", "OCSP container", "OCSP response revocation container");
        return nidOcspResponseRevocationContainer;
    }


    void addOcspToSignerInfo(
        CMS_ContentInfo& cmsContentInfo, const OCSP_RESPONSE& ocspResponse)
    {
        const int nidOcspResponseRevocationContainer = getOcspResponseRevocationNid();
        Expect(nidOcspResponseRevocationContainer != NID_undef, "Could not create OCSP container NID");
        ASN1_OBJECT* formatObject = OBJ_nid2obj(nidOcspResponseRevocationContainer);
        OpenSslExpect(formatObject != nullptr, "Can not get OCSP format object.");

        Asn1ObjectPtr copyFormatObject(OBJ_dup(formatObject));
        OpenSslExpect(copyFormatObject != nullptr, "Can not get copy OCSP format object.");

        ASN1_TYPE* data = nullptr;
        ASN1_TYPE_pack_sequence(
            ASN1_ITEM_rptr(OCSP_RESPONSE),
            const_cast<void*>(static_cast<const void*>(&ocspResponse)), // NOLINT(cppcoreguidelines-pro-type-const-cast)
            &data);
        OpenSslExpect(data != nullptr, "Can not create ASN1_TYPE for OCSP_RESPOSE");
        Asn1TypePtr dataPtr(data);
        OpenSslExpect(
            CMS_add0_otherRevocationInfoChoice(
                &cmsContentInfo,
                copyFormatObject.get(),
                data) != 0,
            "Can not create OCSP revocation info choice in CMS");

        dataPtr.release(); // NOLINT(bugprone-unused-return-value)
        // Ownership transferred to CMS_add0_otherRevocationInfoChoice.
        copyFormatObject.release(); // NOLINT(bugprone-unused-return-value)
    }


    void setSigningTimeToCms(CMS_ContentInfo& cmsContentInfo, const model::Timestamp& signingTime)
    {
        using namespace std::chrono;
        auto secondsSinceEpoch = duration_cast<seconds>(signingTime.toChronoTimePoint().time_since_epoch()).count();
        std::unique_ptr<ASN1_TIME, void (*)(ASN1_TIME*)> sigTimeAsn1{ASN1_TIME_new(), &ASN1_TIME_free};
        ASN1_TIME_set(sigTimeAsn1.get(), secondsSinceEpoch);

        CMS_SignerInfo& signerInfo = getSignerInfo(cmsContentInfo);
        const auto CMS_signed_add1_attr_by_NID_result =
            CMS_signed_add1_attr_by_NID(&signerInfo, NID_pkcs9_signingTime, sigTimeAsn1->type, sigTimeAsn1.get(), -1);
        OpenSslExpect(CMS_signed_add1_attr_by_NID_result == 1, "Failed to set signing time");
    }


    void checkProfessionOids(X509Certificate& certificate, const std::vector<std::string_view>& oids)
    {
        if (oids.empty())
        {
            return;
        }

        std::vector<std::string> oidsStr(oids.size());
        std::transform(oids.cbegin(),
                       oids.cend(),
                       oidsStr.begin(),
                       [](const auto& oid)
                       {
                           return oid.data();
                       });

        A_19225.start("Check ProfessionOIDs in QES certificate.");
        Expect3(certificate.checkRoles(oidsStr),
                "The QES-Certificate does not have expected ProfessionOID.",
                CadesBesSignature::UnexpectedProfessionOidException);
        A_19225.finish();
    }


    OcspResponsePtr getOcspResponse(CMS_ContentInfo& cmsContentInfo)
    {
        const int nidOcspResponseRevocationContainer = getOcspResponseRevocationNid();

        if (nidOcspResponseRevocationContainer != NID_undef)
        {
            ASN1_OBJECT* formatObject = OBJ_nid2obj(nidOcspResponseRevocationContainer);
            Expect(formatObject != nullptr, "Failure by creation of ASN1 format object.");
            int index = CMS_get_anotherRevocationInfo_by_format(&cmsContentInfo, formatObject, -1);
            if (index >= 0)
            {
                ASN1_TYPE* ocspResponseAny = CMS_get0_anotherRevocationInfo(&cmsContentInfo, index);
                if (ocspResponseAny != nullptr)
                {
                    int objectType = ASN1_TYPE_get(ocspResponseAny);
                    if (objectType == V_ASN1_SEQUENCE)
                    {
                        return OcspResponsePtr(
                            reinterpret_cast<OCSP_RESPONSE*>(
                                ASN1_TYPE_unpack_sequence(ASN1_ITEM_rptr(OCSP_RESPONSE), ocspResponseAny)));
                    }
                }
            }
        }

        TLOG(WARNING) << "No OCSP-response is provided in CMS.";

        return {};
    }


    // GEMREQ-start A_22141#SMC-B
    std::tuple<TslMode, std::unordered_set<CertificateType>, std::chrono::system_clock::duration>
    getVerificationExpectations(
        const X509Certificate& certificate,
        const bool allowNonQESCertificate)
    {
        const bool isSmcBOsig = certificate.checkCertificatePolicy(TslService::oid_smc_b_osig);
        if (allowNonQESCertificate && isSmcBOsig)
        {
            // This is a validation of SMC-B Signing Certificate specified by A_22141
            static constexpr int defaultSmcBOsigGracePeriodSeconds = 12 * 60 * 60;
            return {TslMode::TSL, { CertificateType::C_HCI_OSIG },
                    std::chrono::seconds(
                        Configuration::instance().getOptionalIntValue(ConfigurationKey::OCSP_SMC_B_OSIG_GRACE_PERIOD,
                                                                      defaultSmcBOsigGracePeriodSeconds))};
        }
        else
        {
            return {TslMode::BNA, { CertificateType::C_HP_ENC, CertificateType::C_HP_QES },
                    OcspHelper::getOcspGracePeriod(TslMode::BNA)};
        }
    }
    // GEMREQ-end A_22141#SMC-B


    // GEMREQ-start A_22141#verifySignerCertificates, A_20159-02#verifySignerCertificates
    /**
     * Verifies signer certificates ( it must be only one certificate ) of the provided CAdES-BES,
     * and embeds the missing in the package OCSP response for QES related packages.
     */
    void verifySignerCertificates(
        CMS_ContentInfo& cmsContentInfo,
        TslManager& tslManager,
        const bool allowNonQESCertificate,
        const std::vector<std::string_view>& professionOids)
    {
        auto certificates = std::unique_ptr<STACK_OF(X509), std::function<void(STACK_OF(X509)*)> >(CMS_get1_certs(&cmsContentInfo), releaseList);
        const SignerCertificateInfo signerCertificateInfo = getSignerCertificate(
            cmsContentInfo, certificates.get());

        auto certificate = X509Certificate::createFromX509Pointer(signerCertificateInfo.certificate);
        const auto [tslMode, certificateTypes, gracePeriod] =
            getVerificationExpectations(certificate, allowNonQESCertificate);

        std::optional<std::chrono::system_clock::time_point> referenceTimePoint;
        if (tslMode == TslMode::BNA && signerCertificateInfo.signingTimestamp.has_value())
        {
            referenceTimePoint = signerCertificateInfo.signingTimestamp->toChronoTimePoint();
        }

        OcspCheckDescriptor ocspCheckDescriptor{
            OcspCheckDescriptor::OcspCheckMode::PROVIDED_OR_CACHE,
            {referenceTimePoint, gracePeriod},
            getOcspResponse(cmsContentInfo)};

        tslManager.verifyCertificate(
            tslMode,
            certificate,
            certificateTypes,
            ocspCheckDescriptor);

        checkProfessionOids(certificate, professionOids);

        // if no OCSP response is provided in CAdES-BES, the requested OCSP response should be attached,
        // it must be done for both QES and non QES scenarios
        if (ocspCheckDescriptor.providedOcspResponse == nullptr)
        {
            // no ocsp request should be triggered, the ocsp response should be provided from the cache
            TrustStore::OcspResponseData requestedOcspResponseData =
                tslManager.getCertificateOcspResponse(
                    tslMode, certificate, certificateTypes, ocspCheckDescriptor);
            OcspResponsePtr requestedOcspResponse =
                OcspHelper::stringToOcspResponse(requestedOcspResponseData.response);
            OpenSslExpect(requestedOcspResponse != nullptr,
                          "can not deserialize cached OCSP response");
            addOcspToSignerInfo(cmsContentInfo, *requestedOcspResponse);
        }
    }
    // GEMREQ-end A_22141#verifySignerCertificates, A_20159-02#verifySignerCertificates


    // GEMREQ-start A_22141#cmsVerify, A_20159-02#cmsVerify
    int cmsVerify(
        TslManager* tslManager,
        CMS_ContentInfo& cmsContentInfo,
        BIO& out,
        bool allowNonQESCertificate,
        const std::vector<std::string_view>& professionOids)
    {
        if (tslManager != nullptr)
        {
            // CMS_verify does not allow to let the signer certificate be verified
            // and accept trusted intermediate anchors.
            // To workaround it we have to verify signer certificate explicitly
            // and run CMS_verify in CMS_NO_SIGNER_CERT_VERIFY mode.
            verifySignerCertificates(cmsContentInfo, *tslManager, allowNonQESCertificate, professionOids);
        }

        return CMS_verify(&cmsContentInfo,
                          nullptr,
                          nullptr,
                          nullptr,
                          &out,
                          CMS_NO_SIGNER_CERT_VERIFY | CMS_BINARY);
    }
    // GEMREQ-end A_22141#cmsVerify, A_20159-02#cmsVerify


    void handleExceptionByVerification(
        std::exception_ptr exception,
        const FileNameAndLineNumber& fileNameAndLineNumber)
    {
        try
        {
            std::rethrow_exception(std::move(exception));
        }
        catch(const TslError&)
        {
            throw;
        }
        catch(const CadesBesSignature::VerificationException&)
        {
            throw;
        }
        catch(const std::runtime_error& e)
        {
            local::logAndThrow<CadesBesSignature::VerificationException>(
                std::string("CAdES-BES signature verification has failed: ") + e.what(),
                fileNameAndLineNumber);
        }
    }


    X509Certificate getSignerInfoCertificate(
        CMS_ContentInfo& cmsContentInfo,
        CMS_SignerInfo& signerInfo)
    {
        auto certificates = std::unique_ptr<STACK_OF(X509), std::function<void(STACK_OF(X509)*)> >(CMS_get1_certs(&cmsContentInfo), releaseList);
        for (int ind = 0; ind < sk_X509_num(certificates.get()); ind++) {
            X509* cert = sk_X509_value(certificates.get(), ind);
            if (CMS_SignerInfo_cert_cmp(&signerInfo, cert) == 0) {
                return X509Certificate::createFromX509Pointer(cert);
            }
        }

        return {};
    }
}


#define HANDLE_EXCEPTION_BY_VERIFICATION \
    handleExceptionByVerification(std::current_exception(), fileAndLine)


// GEMREQ-start A_22141#internalInitialization
void CadesBesSignature::internalInitialization(const std::string& base64Data,
                                               const CmsVerifyFunction& cmsVerifyFunction)
{
    const auto plain = Base64::decode(Base64::cleanupForDecoding(base64Data));
    auto bioIndata = shared_BIO::make();
    OpenSslExpect(plain.size() <= static_cast<size_t>(std::numeric_limits<int>::max()), "Incoming data is too large");
    OpenSslExpect(BIO_write(bioIndata.get(), plain.data(), static_cast<int>(plain.size())) ==
                  static_cast<int>(plain.size()),
                  "BIO_write function failed.");

    mCmsHandle = shared_CMS_ContentInfo::make(d2i_CMS_bio(bioIndata.get(), nullptr));
    OpenSslExpect(mCmsHandle, "d2i_CMS_bio failed.");
    auto bioOutdata = shared_BIO::make();

    const int cmsVerifyResult = cmsVerifyFunction(*mCmsHandle, *bioOutdata);
    OpenSslExpect(cmsVerifyResult == 1, "CMS_verify failed.");

    // it is safe to assume that the payload will be less than the whole pkcs7 file.
    mPayload.resize(plain.size());
    const auto bytesRead = BIO_read(bioOutdata.get(), mPayload.data(), gsl::narrow<int>(mPayload.size()));
    OpenSslExpect(bytesRead > 0, "BIO_read failed");
    OpenSslExpect(BIO_eof(bioOutdata.get()) == 1, "Did not read all data from signature");
    mPayload.resize(static_cast<size_t>(bytesRead));
}
// GEMREQ-end A_22141#internalInitialization

// GEMREQ-start A_22141#CadesBesSignature, A_20159-02#CadesBesSignature
CadesBesSignature::CadesBesSignature(const std::string& base64Data,
                                     TslManager& tslManager,
                                     bool allowNonQESCertificate,
                                     const std::vector<std::string_view>& professionOids)
: mPayload{},
  mCmsHandle{}
{
    try
    {
        internalInitialization(
            base64Data,
            [&] (CMS_ContentInfo& cmsHandle, BIO& out) -> int
            {
              return cmsVerify(&tslManager, cmsHandle, out, allowNonQESCertificate, professionOids);
            });
    }
    catch(...)
    {
        HANDLE_EXCEPTION_BY_VERIFICATION;
    }
}
// GEMREQ-end A_22141#CadesBesSignature, A_20159-02#CadesBesSignature

CadesBesSignature::CadesBesSignature(const std::string& base64Data)
: mPayload{},
  mCmsHandle{}
{
    try
    {
        internalInitialization(
            base64Data,
            [] (CMS_ContentInfo& cmsHandle, BIO& out) -> int
            {
                return cmsVerify(nullptr, cmsHandle, out, false, {});
            });
    }
    catch(...)
    {
        HANDLE_EXCEPTION_BY_VERIFICATION;
    }
}

CadesBesSignature::CadesBesSignature(const std::list<Certificate>& trustedCertificates,
                                     const std::string& base64Data)
: mPayload{},
  mCmsHandle{}
{
    try
    {
        internalInitialization(
            base64Data,
            [&trustedCertificates] (CMS_ContentInfo& cmsHandle, BIO& out) -> int
            {
                shared_X509_Store certStore = shared_X509_Store::make();
                for (const auto& cert : trustedCertificates)
                {
                    OpenSslExpect(X509_STORE_add_cert(certStore, cert.toX509().removeConst()) == 1,
                                  "Error adding certificate to certificate store");
                }

                return CMS_verify(&cmsHandle, nullptr, certStore.get(), nullptr, &out, CMS_BINARY);
            });
    }
    catch(...)
    {
        HANDLE_EXCEPTION_BY_VERIFICATION;
    }

}

// GEMREQ-start A_22127#addOscpInfo
CadesBesSignature::CadesBesSignature(
    const Certificate& cert,
    const shared_EVP_PKEY& privateKey,
    const std::string& payload,
    const std::optional<model::Timestamp>& signingTime,
    OcspResponsePtr ocspResponse)
    : mPayload(payload),
      mCmsHandle{}
{
    auto bio = shared_BIO::make();
    auto written = BIO_write(bio.get(), payload.data(), gsl::narrow<int>(payload.size()));
    OpenSslExpect(written == gsl::narrow<int>(payload.size()), "BIO_write failed." + std::to_string(written));

    mCmsHandle = shared_CMS_ContentInfo::make(
        CMS_sign(cert.toX509().removeConst(), privateKey.removeConst(), nullptr, bio.get(), CMS_BINARY | CMS_STREAM | CMS_PARTIAL));
    OpenSslExpect(mCmsHandle.isSet(), "Failed to sign with CMS");

    if (signingTime.has_value())
    {
        setSigningTime(*signingTime);
    }

    addCertificateToSignerInfo(*mCmsHandle, cert);

    if (ocspResponse != nullptr)
    {
        addOcspToSignerInfo(*mCmsHandle, *ocspResponse);
    }

    // finalise CMS
    OpenSslExpect(1 == CMS_final(mCmsHandle, bio.get(), nullptr, CMS_BINARY), "Can not finalise CMS signature");
}
// GEMREQ-end A_22127#addOscpInfo


std::string CadesBesSignature::getBase64() const
{
    std::ostringstream outStream(std::ios::binary);
    auto outBio = shared_BIO::make();
    // the CMS handle is not changed, but the openssl API requires non-const pointer
    i2d_CMS_bio(outBio.get(), mCmsHandle.removeConst());
    while (!BIO_eof(outBio.get()))
    {
        std::array<char, 4096> buffer; //NOLINT[cppcoreguidelines-pro-type-member-init,hicpp-member-init]
        auto bytesRead = BIO_read(outBio.get(), buffer.data(), gsl::narrow_cast<int>(buffer.size()));
        OpenSslExpect(bytesRead > 0, "Failed reading CMS Data");
        outStream.write(buffer.data(), bytesRead);
    }
    return Base64::encode(outStream.str());
}


const std::string& CadesBesSignature::payload() const
{
    return mPayload;
}


std::optional<model::Timestamp> CadesBesSignature::getSigningTime() const
{
    Expect(mCmsHandle != nullptr, "CadesBesSignature object is not properly initialized.");
    CMS_SignerInfo& signerInfo = getSignerInfo(*mCmsHandle.removeConst());

    auto result = getSigningTimeFromSignerInfo(signerInfo);
    if (result.has_value())
        return result;

    return {};
}


::std::string CadesBesSignature::getMessageDigest() const
{
    CMS_SignerInfo& signerInfo = getSignerInfo(*mCmsHandle.removeConst());
    ASN1_TYPE& digestAsn1Type = getMessageDigestAsn1Type(signerInfo);
    return ::std::string{reinterpret_cast<const char*>(digestAsn1Type.value.octet_string->data),
                         static_cast<size_t>(digestAsn1Type.value.octet_string->length)};
}


void CadesBesSignature::setSigningTime(const model::Timestamp& signingTime)
{
    setSigningTimeToCms(*mCmsHandle.removeConst(), signingTime);
}


void CadesBesSignature::addCounterSignature(const Certificate& cert, const shared_EVP_PKEY& privateKey)
{
    Expect(mCmsHandle != nullptr, "CadesBesSignature object is not properly initialized.");
    CMS_SignerInfo& signerInfo = getSignerInfo(*mCmsHandle);
    OpenSslExpect(
        1 == CMS_add1_counter_signature(
                 &signerInfo,
                 mCmsHandle.get(),
                 cert.toX509().removeConst(),
                 privateKey.removeConst(),
                 nullptr,
                 CMS_CADES),
        "Failed to create counter signature.");
}

void CadesBesSignature::validateCounterSignature(const Certificate& signerCertificate) const
{
    struct CMS_SignerInfoDeleter {
        void operator()(CMS_SignerInfo* ptr)
        {
            ASN1_item_free(reinterpret_cast<ASN1_VALUE*>(ptr), ASN1_ITEM_rptr(CMS_SignerInfo));
        }
    };
    using CMS_SignerInfoPtr = std::unique_ptr<CMS_SignerInfo, CMS_SignerInfoDeleter>;

    Expect(mCmsHandle != nullptr, "CadesBesSignature object is not properly initialized.");
    const CMS_SignerInfo& signerInfo = getSignerInfo(*mCmsHandle.removeConst());
    const int counterSignatureIndex = CMS_unsigned_get_attr_by_NID(&signerInfo, NID_pkcs9_countersignature, -1);
    Expect(counterSignatureIndex != 1, "Could not find counter signature");

    X509_ATTRIBUTE* x509Attributes = CMS_unsigned_get_attr(&signerInfo, counterSignatureIndex);
    const ASN1_TYPE* asn1Type = X509_ATTRIBUTE_get0_type(x509Attributes, 0);
    OpenSslExpect(asn1Type != nullptr && asn1Type->type == V_ASN1_SEQUENCE && asn1Type->value.sequence != nullptr,
                  "Unexpected X509 attribute properties");
    const unsigned char* data = asn1Type->value.sequence->data;

    const CMS_SignerInfoPtr counterSignatureSignerInfo{reinterpret_cast<CMS_SignerInfo*>(
        //NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        ASN1_item_d2i(nullptr, const_cast<const unsigned char**>(&data), asn1Type->value.sequence->length,
                      ASN1_ITEM_rptr(CMS_SignerInfo)))};
    OpenSslExpect(counterSignatureSignerInfo != nullptr, "unable to get CMS_SignerInfo");

    X509Certificate counterSignatureSigner
        = getSignerInfoCertificate(*mCmsHandle.removeConst(), *counterSignatureSignerInfo);
    Expect(counterSignatureSigner == X509Certificate::createFromBase64(signerCertificate.toBase64Der()),
           "The provided signer certificate was not used for counter signature.");

    CMS_SignerInfo_set1_signer_cert(counterSignatureSignerInfo.get(), signerCertificate.toX509().removeConst());
    const int verifyResult = CMS_SignerInfo_verify(counterSignatureSignerInfo.get());
    OpenSslExpect(verifyResult == 1, "CMS_SignerInfo_verify failed.");
}
