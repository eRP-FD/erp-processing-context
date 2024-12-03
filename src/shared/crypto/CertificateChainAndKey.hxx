/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_SHARED_CRYPTO_CERTIFICATECHAINANDKEY_HXX
#define ERP_SHARED_CRYPTO_CERTIFICATECHAINANDKEY_HXX

#include "shared/crypto/CertificateKeyPair.hxx"


/**
 * Aggregates a certificate + private key pair and the corresponding (not necessarily complete)
 * chain of CA certificates.
 */
class CertificateChainAndKey
{
public:
    explicit CertificateChainAndKey(
        CertificateKeyPair certificateKeyPair,
        std::string caCertificates = "");

    /**
     * Create CertificateChainAndKey from a string of concatenated certificates in PEM format and
     * the private key in PEM format.
     *
     * @param certificates The concatenated certificates in PEM format. The order is not relevant.
     *        The first certificate not marked as CA is considered to be the end-entity certificate
     *        that belongs to the key.
     * @param key The private key in PEM format.
     * @return The CertificateChainAndKey created from the given certificates and private key.
     */
    static CertificateChainAndKey fromPem(const std::string& certificates, const std::string& key);

    /** A certificate and corresponding private key (usually from an end entity). */
    CertificateKeyPair certificateKeyPair;

    /**
     * The (not necessarily complete) chain of CA certificates for the certificate in
     * certificateKeyPair. Concatenated PEM format. May be an empty string.
     */
    std::string caCertificates;
};


#endif
