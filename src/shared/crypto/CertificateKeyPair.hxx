/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_SHARED_CRYPTO_CERTIFICATEKEYPAIR_HXX
#define ERP_SHARED_CRYPTO_CERTIFICATEKEYPAIR_HXX

#include "shared/crypto/OpenSslHelper.hxx"

#include <string>


/**
 * Represents the tuple of certificate and private key.
 * Can be used for client authentication in mtls setups.
 * Can hold the private key in PEM format or in HSM.
 * The private key is encapsulated in EVP_PKEY.
 */
class CertificateKeyPair
{
public:
    CertificateKeyPair() = delete;

    static CertificateKeyPair fromPem(const std::string& pemCertificate, const std::string& pemKey);

    shared_EVP_PKEY getKey() const
    {
        return mKey;
    }
    shared_X509 getCertificate() const
    {
        return mCertificate;
    }

private:
    shared_EVP_PKEY mKey;
    shared_X509 mCertificate;

    CertificateKeyPair(shared_X509 certificate, shared_EVP_PKEY key);
};

#endif
