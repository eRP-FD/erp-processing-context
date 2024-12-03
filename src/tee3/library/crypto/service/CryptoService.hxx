/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_SERVICE_CRYPTOSERVICE_HXX
#define EPA_LIBRARY_CRYPTO_SERVICE_CRYPTOSERVICE_HXX

#include "library/crypto/CryptoTypes.hxx"
#include "library/crypto/Signature.hxx"
#include "library/util/CertificateVersion.hxx"
#include "library/util/Configuration.hxx"

#include <functional>
#include <string>

#include "shared/crypto/Certificate.hxx"
#include "shared/tsl/TslService.hxx"
namespace epa
{


// GEMREQ-start A_16901, GS_A_4367
/**
 * Interface for cryptographic operations that are provided by the HSM in production.
 *
 * Access to instances (well a single instance, since CryptoService is a singleton) is
 * provided by CryptoServiceProvider.
 */
class CryptoService
{
public:
    enum SignMode
    {
        /** Accept arbitrary input data, which is to be hashed, padded, and signed. */
        HashAndSign,

        /** Accept a padded hash as input data, which is to be signed only. */
        SignOnly,
    };

    virtual ~CryptoService() = default;

    /**
     * Checks authentication and presence of required keys.
     * @param providerKeyName selects the key that the HSM uses for the derivation.
     * @throws std::runtime_error if authentication fails or the providerKeyName is not known
     *         to the HSM
     */
    virtual void readyCheck(const std::string& providerKeyName) const = 0;

    /**
     * Whether certificate of type and version exists.
     */
    virtual bool hasSignerCertificate(
        CertificateType certificateType,
        CertificateVersion certificateVersion = CertificateVersion::Current) const = 0;

    /**
     * Signs data with an Elliptic Curve private key which never leaves the HSM. The hashing
     * algorithm is SHA-256. (CXI_MECH_HASH_ALGO_SHA256).
     *
     * @param data data to be signed
     * @param certificateType certificate to use for signing
     * @param mode the mode of operation (hash and sign or sign only)
     * @throws cxi::Exception
     * @return pointer to signature
     */
    virtual std::unique_ptr<Signature> sign(
        const std::string& data,
        CertificateType certificateType = CertificateType::C_FD_AUT,
        SignMode mode = HashAndSign,
        CertificateVersion certificateVersion = CertificateVersion::Current) const = 0;

    /**
     * Derives an AES-256 key for the insurant. The derivation is done by calculating an HMAC over
     * the given kvnr with a secret AES-key that is always kept in the HSM. The hash is calculated
     * using SHA-256.
     *
     * The key name and group name depend on the HSM configuration and correct values must be
     * obtained before a derivation attempt is made.
     *
     * @param kvnr insurant id
     * @param providerKeyName selects the key that is used by the HSM to derive from
     * @throws cxi::Exception
     * @return derived AES key
     */
    virtual EncryptionKey deriveKey(const std::string& kvnr, const std::string& providerKeyName)
        const = 0;

    /**
     * Fetches \<numBytes> random bytes from the HSM device.
     *
     * @param numBytes number of random bytes to be returned
     * @throws cxi::Exception
     * @return random data
     */
    virtual std::string randomBytes(const size_t numBytes) const = 0;

    /**
     * @param certificateType type of the certificate to return
     * @return certificate object matching the private key used by the sign method
     */
    virtual std::string getSignerCertificateInPemFormat(
        CertificateType certificateType = CertificateType::C_FD_AUT,
        CertificateVersion certificateVersion = CertificateVersion::Current) const = 0;

    virtual shared_X509 getSignerCertificate(
        CertificateType certificateType,
        CertificateVersion certificateVersion = CertificateVersion::Current) const = 0;

    virtual bool isProductionEnvironment() const;

    /**
     * A short test that verifies that the HSM is accessible via the CXI API.
     * @param providerKeyName is passed on to deriveKey() if given.
     *                        Must be a key name that is known to the HSM.
     *                        If not given, the deriveKey() test is skipped.
     */
    virtual void quickCheck(const std::optional<std::string>& providerKeyName) const;

    /**
     * Performs ECDH with an HSM key and a peer key.
     * @param certificateType the type of the hsm key to use
     * @param peerKey the peer key to use for the key derivation
     * @return the resulting binary data in a string
     */
    virtual std::string ecdhKeyExchange(
        CertificateType certificateType,
        const EC_POINT* peerKey,
        CertificateVersion certificateVersion = CertificateVersion::Current) const = 0;

protected:
    CryptoService() = default;
};
// GEMREQ-end A_16901, GS_A_4367

} // namespace epa

#endif
