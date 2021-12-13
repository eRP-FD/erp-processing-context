/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_CADESBESSIGNATURE_HXX
#define ERP_PROCESSING_CONTEXT_CADESBESSIGNATURE_HXX

#include "erp/crypto/OpenSslHelper.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/model/Timestamp.hxx"

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
    /// In case TslManager is provided it is used to verify the signature, otherwise no verification is done.
    CadesBesSignature(const std::string& base64Data, TslManager* tslManager);

    /// @brief initialize from a base64 encoded CMS file signature format. Verifies the incoming signature
    /// @throws std::runtime_error if the signature verification fails.
    /// The following constructor is currently intended only to be used by tests, where the trusted certificates
    /// are provided explicitly. The production implementation should use the constructor based on TslManager.
    CadesBesSignature(const std::list<Certificate>& trustedCertificates, const std::string& base64Data);

    /// @brief initialize from plain payload data, that shall be signed using cert and privateKey.
    CadesBesSignature(const Certificate& cert, const shared_EVP_PKEY& privateKey, const std::string& payload,
                      const std::optional<model::Timestamp>& signingTime = std::nullopt);

    /// @brief returns the the signature as CMS file.
    std::string get();

    /// @brief returns the payload, that has been signed.
    const std::string& payload() const;

    /// return the signingTime (OID 1.2.840.113549.1.9.5) from the CMS structure
    std::optional<model::Timestamp> getSigningTime() const;

    [[nodiscard]] ::std::optional<::std::string> getMessageDigest() const;

private:
    void setSigningTime(const model::Timestamp& signingTime);
    void internalInitialization(const std::string& base64Data,
                                const std::function<int(CMS_ContentInfo&, BIO&)>& cmsVerifyFunction);

    std::string mPayload;
    shared_CMS_ContentInfo mCmsHandle;
};


#endif//ERP_PROCESSING_CONTEXT_CADESBESSIGNATURE_HXX
