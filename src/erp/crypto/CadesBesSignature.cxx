/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/crypto/CadesBesSignature.hxx"

#include "erp/crypto/Certificate.hxx"
#include "erp/crypto/EssCertificateHelper.hxx"
#include "erp/pc/ProfessionOid.hxx"
#include "erp/tsl/TslManager.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Gsl.hxx"
#include "erp/ErpRequirements.hxx"


namespace
{
    bool hasSignerInfosWithoutCertificate(const STACK_OF(CMS_SignerInfo)& signerInfos)
    {
        const int signerInfosCount = sk_CMS_SignerInfo_num(&signerInfos);
        OpenSslExpect(signerInfosCount > 0, "No signer infos provided.");

        for (int ind = 0; ind < signerInfosCount; ind++)
        {
            auto* signerInfo = sk_CMS_SignerInfo_value(&signerInfos, ind);
            OpenSslExpect(signerInfo != nullptr, "Signer info must be provided.");
            X509* signer = nullptr;
            CMS_SignerInfo_get0_algs(signerInfo, nullptr, &signer, nullptr, nullptr);
            if (!signer)
                return true;
        }

        return false;
    }


    void setSignerInfoOnDemand(
        CMS_ContentInfo& cmsContentInfo,
        const STACK_OF(CMS_SignerInfo)& signerInfos,
        STACK_OF(X509)* certificates)
    {
        if (hasSignerInfosWithoutCertificate(signerInfos))
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


    std::vector<X509*> getSignerCertificates(CMS_ContentInfo& cmsContentInfo, STACK_OF(X509)* certificates)
    {
        auto* signerInfos = CMS_get0_SignerInfos(&cmsContentInfo);
        OpenSslExpect(signerInfos != nullptr, "No signer infos is provided.");

        const int signerInfosCount = sk_CMS_SignerInfo_num(signerInfos);
        OpenSslExpect(signerInfosCount > 0, "No signer infos provided.");

        setSignerInfoOnDemand(cmsContentInfo, *signerInfos, certificates);

        std::vector<X509*> signerCertificates;
        for (int ind = 0; ind < signerInfosCount; ind++)
        {
            auto* signerInfo = sk_CMS_SignerInfo_value(signerInfos, ind);
            OpenSslExpect(signerInfo != nullptr, "Signer info must be provided.");

            verifySignerInfo(*signerInfo);

            X509* signerCertificate = nullptr;
            CMS_SignerInfo_get0_algs(signerInfo, nullptr, &signerCertificate, nullptr, nullptr);
            OpenSslExpect(signerCertificate != nullptr, "No signer certificate.");

            EssCertificateHelper::verifySigningCertificateFromSignedData(*signerInfo, *signerCertificate);

            signerCertificates.emplace_back(signerCertificate);
        }

        return signerCertificates;
    }


    void addCertificateToSignerInfo(
        CMS_ContentInfo& cmsContentInfo, const Certificate& certificate)
    {
        auto* signerInfos = CMS_get0_SignerInfos(&cmsContentInfo);
        OpenSslExpect(signerInfos != nullptr, "No signer infos is provided.");

        const int signerInfosCount = sk_CMS_SignerInfo_num(signerInfos);
        OpenSslExpect(signerInfosCount > 0, "No signer infos provided.");

        for (int ind = 0; ind < signerInfosCount; ind++)
        {
            auto* signerInfo = sk_CMS_SignerInfo_value(signerInfos, ind);
            OpenSslExpect(signerInfo != nullptr, "Signer info must be provided.");

            if (CMS_signed_get_attr_by_NID(signerInfo, NID_id_smime_aa_signingCertificate, -1) < 0 &&
                CMS_signed_get_attr_by_NID(signerInfo, NID_id_smime_aa_signingCertificateV2, -1) < 0)
            {
                // add signing certificate to the signerInfo
                EssCertificateHelper::setSigningCertificateInSignedData(*signerInfo, *certificate.toX509().removeConst());
            }
        }
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
            const_cast<void*>(static_cast<const void*>(&ocspResponse)),
            &data);
        OpenSslExpect(data != nullptr, "Can not create ASN1_TYPE for OCSP_RESPOSE");
        Asn1TypePtr dataPtr(data);
        OpenSslExpect(
            CMS_add0_otherRevocationInfoChoice(
                &cmsContentInfo,
                copyFormatObject.get(),
                data) != 0,
            "Can not create OCSP revocation info choice in CMS");

        dataPtr.release();
        copyFormatObject.release();
    }


    void checkProfessionOids(X509Certificate& certificate)
    {
        A_19225.start("Check ProfessionOIDs in QES certificate.");
        Expect3(certificate.checkRoles({
                      std::string(profession_oid::oid_arzt),
                      std::string(profession_oid::oid_zahnarzt),
                      std::string(profession_oid::oid_arztekammern)}),
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


    void verifySignerCertificates(
        CMS_ContentInfo& cmsContentInfo,
        TslManager& tslManager)
    {
        OcspResponsePtr ocspResponse = getOcspResponse(cmsContentInfo);

        const auto releaseList = [] (STACK_OF(X509)* lst) {
            sk_X509_pop_free(lst, X509_free);
        };
        auto certificates = std::unique_ptr<STACK_OF(X509), std::function<void(STACK_OF(X509)*)> >(CMS_get1_certs(&cmsContentInfo), releaseList);
        const std::vector<X509*> signerCertificates = getSignerCertificates(
            cmsContentInfo, certificates.get());

        for (X509* signerCertificate : signerCertificates)
        {
            X509Certificate certificate = X509Certificate::createFromX509Pointer(signerCertificate);
            tslManager.verifyCertificate(
                TslMode::BNA,
                certificate,
                {CertificateType::C_HP_QES, CertificateType::C_HP_ENC},
                ocspResponse);
            checkProfessionOids(certificate);
        }
    }


    int cmsVerify(
        TslManager* tslManager,
        CMS_ContentInfo& cmsContentInfo,
        BIO& out)
    {
        if (tslManager != nullptr)
        {
            // CMS_verify does not allow to let the signer certificate be verified
            // and accept trusted intermediate anchors.
            // To workaround it we have to verify signer certificate explicitly
            // and run CMS_verify in CMS_NO_SIGNER_CERT_VERIFY mode.
            verifySignerCertificates(cmsContentInfo, *tslManager);

            return CMS_verify(&cmsContentInfo,
                              nullptr,
                              nullptr,
                              nullptr,
                              &out,
                              CMS_NO_SIGNER_CERT_VERIFY | CMS_BINARY);
        }
        else
        {
            return CMS_verify(&cmsContentInfo, nullptr, nullptr, nullptr, &out, CMS_NOVERIFY);
        }
    }


    void handleExceptionByVerification(
        std::exception_ptr exception,
        const FileNameAndLineNumber& fileNameAndLineNumber)
    {
        try
        {
            std::rethrow_exception(exception);
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
}


#define HANDLE_EXCEPTION_BY_VERIFICATION \
    handleExceptionByVerification(std::current_exception(), fileAndLine)


void CadesBesSignature::internalInitialization(const std::string& base64Data,
                                               const std::function<int(CMS_ContentInfo&, BIO&)>& cmsVerifyFunction)
{
    const auto plain = Base64::decode(Base64::cleanupForDecoding(base64Data));
    auto bioIndata = shared_BIO::make();
    OpenSslExpect(plain.size() <= static_cast<size_t>(std::numeric_limits<int>::max()), "Incoming data is too large");
    OpenSslExpect(BIO_write(bioIndata.get(), plain.data(), static_cast<int>(plain.size())) ==
                  static_cast<int>(plain.size()),
                  "BIO_write function failed.");

    mCmsHandle = shared_CMS_ContentInfo ::make(d2i_CMS_bio(bioIndata.get(), nullptr));
    OpenSslExpect(mCmsHandle, "d2i_CMS_bio failed.");
    auto bioOutdata = shared_BIO::make();

    const int cmsVerifyResult = cmsVerifyFunction(*mCmsHandle, *bioOutdata);
    OpenSslExpect(cmsVerifyResult == 1, "CMS_verify failed.");

    // it is safe to assume that the payload will be less than the whole pkcs7 file.
    mPayload.resize(plain.size());
    const auto bytesRead = BIO_read(bioOutdata.get(), mPayload.data(), gsl::narrow<int>(mPayload.size()));
    OpenSslExpect(bytesRead > 0, "BIO_read failed");
    OpenSslExpect(BIO_eof(bioOutdata.get()) == 1, "Did not read all data from signature");
    mPayload.resize(bytesRead);
}


CadesBesSignature::CadesBesSignature(const std::string& base64Data, TslManager* tslManager)
{
    try
    {
        internalInitialization(
            base64Data,
            [tslManager] (CMS_ContentInfo& cmsHandle, BIO& out) -> int
            {
              return cmsVerify(tslManager, cmsHandle, out);
            });
    }
    catch(...)
    {
        HANDLE_EXCEPTION_BY_VERIFICATION;
    }
}


CadesBesSignature::CadesBesSignature(const std::list<Certificate>& trustedCertificates,
                                     const std::string& base64Data)
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

CadesBesSignature::CadesBesSignature(
    const Certificate& cert,
    const shared_EVP_PKEY& privateKey,
    const std::string& payload,
    const std::optional<model::Timestamp>& signingTime,
    OcspResponsePtr ocspResponse)
    : mPayload(payload)
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


std::string CadesBesSignature::get()
{
    std::ostringstream outStream(std::ios::binary);
    auto outBio = shared_BIO::make();
    i2d_CMS_bio(outBio.get(), mCmsHandle.get());
    while (!BIO_eof(outBio.get()))
    {
        std::array<char, 4096> buffer; //NOLINT[cppcoreguidelines-pro-type-member-init,hicpp-member-init]
        auto bytesRead = BIO_read(outBio.get(), buffer.data(), gsl::narrow_cast<int>(buffer.size()));
        OpenSslExpect(bytesRead > 0, "Failed reading CMS Data");
        outStream.write(buffer.data(), bytesRead);
    }
    return outStream.str();
}


const std::string& CadesBesSignature::payload() const
{
    return mPayload;
}


std::optional<model::Timestamp> CadesBesSignature::getSigningTime() const
{
    const auto* signerInfos = CMS_get0_SignerInfos(mCmsHandle.removeConst().get());
    OpenSslExpect(signerInfos != nullptr, "Error getting signer info from CMS structure");
    const auto signerInfosCount = sk_CMS_SignerInfo_num(signerInfos);
    OpenSslExpect(signerInfosCount > 0, "Error getting signer info from CMS structure");

    for (int ind = 0; ind < signerInfosCount; ++ind)
    {
        auto* signerInfo = sk_CMS_SignerInfo_value(signerInfos, ind);
        OpenSslExpect(signerInfo != nullptr, "got nullptr from sk_CMS_SignerInfo_value");
        const int attributeIndex = CMS_signed_get_attr_by_NID(signerInfo, NID_pkcs9_signingTime, -1);
        auto* x509Attributes = CMS_signed_get_attr(signerInfo, attributeIndex);
        ASN1_TYPE *asn1Type = X509_ATTRIBUTE_get0_type(x509Attributes, 0);
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

::std::optional<::std::string> CadesBesSignature::getMessageDigest() const
{
    const auto* const signerInfos = CMS_get0_SignerInfos(mCmsHandle.removeConst().get());
    OpenSslExpect(signerInfos, "Error getting signer info from CMS structure");
    const auto signerInfoCount = sk_CMS_SignerInfo_num(signerInfos);
    OpenSslExpect(signerInfoCount > 0, "Error getting signer info from CMS structure");

    for (auto index = 0; index < signerInfoCount; ++index)
    {
        const auto* const signerInfo = sk_CMS_SignerInfo_value(signerInfos, index);
        OpenSslExpect(signerInfo, "got nullptr from sk_CMS_SignerInfo_value");
        const auto attributeIndex = CMS_signed_get_attr_by_NID(signerInfo, NID_pkcs9_messageDigest, -1);
        auto* x509Attributes = CMS_signed_get_attr(signerInfo, attributeIndex);
        const auto* const asn1Type = X509_ATTRIBUTE_get0_type(x509Attributes, 0);
        if (asn1Type)
        {
            OpenSslExpect((asn1Type->type == V_ASN1_OCTET_STRING) && asn1Type->value.octet_string,
                          "Unexpected type of messageDigest.");

            return ::std::string{reinterpret_cast<const char*>(asn1Type->value.octet_string->data),
                                 static_cast<size_t>(asn1Type->value.octet_string->length)};
        }
    }

    return {};
}

void CadesBesSignature::setSigningTime(const model::Timestamp& signingTime)
{
    using namespace std::chrono;
    auto secondsSinceEpoch = duration_cast<seconds>(signingTime.toChronoTimePoint().time_since_epoch()).count();
    std::unique_ptr<ASN1_TIME, void (*)(ASN1_TIME*)> sigTimeAsn1{ASN1_TIME_new(), &ASN1_TIME_free};
    ASN1_TIME_set(sigTimeAsn1.get(), secondsSinceEpoch);
    const auto* const signerInfos = CMS_get0_SignerInfos(mCmsHandle);
    OpenSslExpect(signerInfos != nullptr, "Error getting signer info from CMS structure");

    const auto signerInfosCount = sk_CMS_SignerInfo_num(signerInfos);
    OpenSslExpect(signerInfosCount > 0, "Error getting signer info from CMS structure");

    for (int ind = 0; ind < signerInfosCount; ++ind)
    {
        auto* const signerInfo = sk_CMS_SignerInfo_value(signerInfos, ind);
        OpenSslExpect(signerInfo != nullptr, "got nullptr from sk_CMS_SignerInfo_value");
        const auto CMS_signed_add1_attr_by_NID_result =
            CMS_signed_add1_attr_by_NID(signerInfo, NID_pkcs9_signingTime, sigTimeAsn1->type, sigTimeAsn1.get(), -1);
        OpenSslExpect(CMS_signed_add1_attr_by_NID_result == 1, "Failed to set signing time");
    }
}
