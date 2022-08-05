/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_CADESBESSIGNATURE_HXX
#define ERP_PROCESSING_CONTEXT_CADESBESSIGNATURE_HXX

#include "erp/crypto/OpenSslHelper.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "fhirtools/model/Timestamp.hxx"

#include <functional>
#include <list>

class Certificate;
class TslManager;

/// @brief represents a CMS signature file, using CAdES-BES profile
class CadesBesSignature
{
public:
    using VerificationException =
        WrappedErrorClass<std::runtime_error, struct SignatureVerificationExceptionTag>;
    using UnexpectedProfessionOidException =
        WrappedErrorClass<VerificationException, struct UnexpectedProfessionOidExceptionTag>;

    /// @brief initialize from a base64 encoded CMS file signature format. Verifies the incoming signature.
    /// @throws TslError                            if there are problems by certificate verification.
    /// @throws UnexpectedProfessionOidException    if certificate does not contain expected ProfessionOID
    /// @throws std::runtime_error                  if the signature verification fails.
    CadesBesSignature(const std::string& base64Data,
                      TslManager& tslManager,
                      bool allowNonQesCertificate = false,
                      const std::vector<std::string_view>& professionOids = {});

    /// @brief initialize from a base64 encoded CMS file signature format. NO VERIFICATION IS DONE.
    explicit CadesBesSignature(const std::string& base64Data);

    /// @brief initialize from a base64 encoded CMS file signature format. Verifies the incoming signature
    /// @throws std::runtime_error if the signature verification fails.
    /// The following constructor is currently intended only to be used by tests, where the trusted certificates
    /// are provided explicitly. The production implementation should use the constructor based on TslManager.
    CadesBesSignature(const std::list<Certificate>& trustedCertificates, const std::string& base64Data);

    /// @brief initialize from plain payload data, that shall be signed using cert and privateKey.
    CadesBesSignature(
        const Certificate& cert,
        const shared_EVP_PKEY& privateKey,
        const std::string& payload,
        const std::optional<fhirtools::Timestamp>& signingTime = std::nullopt,
        OcspResponsePtr ocspResponse = {});

    /// @brief returns the the signature as Base64 encoded CMS file.
    std::string getBase64() const;

    /// @brief returns the payload, that has been signed.
    const std::string& payload() const;

    /// return the signingTime (OID 1.2.840.113549.1.9.5) from the CMS structure
    std::optional<fhirtools::Timestamp> getSigningTime() const;

    [[nodiscard]] ::std::optional<::std::string> getMessageDigest() const;

private:
    using CmsVerifyFunction = std::function<int(CMS_ContentInfo&, BIO&)>;

    void setSigningTime(const fhirtools::Timestamp& signingTime);
    void internalInitialization(const std::string& base64Data, const CmsVerifyFunction& cmsVerifyFunction);

    std::string mPayload;
    shared_CMS_ContentInfo mCmsHandle;
};


#endif//ERP_PROCESSING_CONTEXT_CADESBESSIGNATURE_HXX
