/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_CRYPTO_CERTIFICATE_HXX
#define E_LIBRARY_CRYPTO_CERTIFICATE_HXX

#include "shared/crypto/AuthorizedIdentity.hxx"
#include "shared/crypto/OpenSslHelper.hxx"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>


/**
 * Represent an X509 certificate.
 */
class Certificate
{
public:
    explicit Certificate (shared_X509 x509Certificate);
    static Certificate fromBinaryDer(const std::string& binaryDer);
    static Certificate fromBase64(const std::string& base64);
    static Certificate fromPem (const std::string& pem);
    static Certificate fromBase64Der (const std::string& base64Der);

    Certificate (const Certificate& other);
    Certificate& operator=(const Certificate&) = delete;

    Certificate (Certificate&& other) noexcept = default;
    Certificate& operator=(Certificate&& other) noexcept = delete;

    ~Certificate() = default;

    std::string toPem(void) const;
    std::string toBinaryDer(void) const;
    std::string toBase64Der(void) const;

    shared_EVP_PKEY getPublicKey (void) const;
    const X509_NAME* getSubjectName (void) const;
    const X509_NAME* getIssuerName (void) const;

    bool isEllipticCurve (void) const;

    shared_X509 toX509 (void);

    const shared_X509& toX509 (void) const;

    void setNextCertificate (const Certificate& certificate);
    const Certificate* getNextCertificate (void) const;

    class Builder
    {
    public:
        Builder& withRandomSerial (void);
        Builder& withSerial (long int serial);
        Builder& withSubjectName (const X509_NAME* name);
        Builder& withSubjectName (const std::vector<std::string>& parts);
        Builder& withSubjectAlternateName (const std::string& name);
        Builder& withIssuerName (const X509_NAME* name);
        Builder& withIssuerName (const std::vector<std::string>& parts);
        Builder& fromNow (void);
        Builder& until (const std::chrono::duration<int>& end);
        Builder& withPublicKey (const shared_EVP_PKEY& publicKey);
        Builder& withRegistrationNumber (const std::string& registrationNumber);

        Certificate build() const;

    private:
        friend class Certificate;
        shared_X509 mX509Certificate;

        Builder (void);
    };

    /**
     * Currently it produces per default an empty unsigned certificate that is not really usable,
     * use very carefully.
     */
    static Builder build (void);

    /**
     * Return a valid but self signed certificate.
     * The returned certificate is regarded as a mock because
     * a) its self-signed and
     * b) its parameters are less carefully set than
     *    would be necessary for a production certificate.
     */
    static Certificate createSelfSignedCertificateMock (
        const shared_EVP_PKEY& keyPair,
        const AuthorizedIdentity& authorizedIdentity = { .identity="kvnr",
                                                         .type=AuthorizedIdentity::IDENTITY_SUBJECT_ID},
        const std::string& hostname = "localhost",
        const std::optional<std::string>& registrationNumber = std::nullopt,
        const std::string& subjectAlternameName = "IP:127.0.0.1");

    static Certificate createCertificateMock (
        const Certificate& rootCertificate,
        const shared_EVP_PKEY& rootCertificatePrivateKey,
        const shared_EVP_PKEY& keyPair);

private:
    shared_X509 mX509Certificate;
    std::unique_ptr<Certificate> mNextCertificate;
};

#endif
