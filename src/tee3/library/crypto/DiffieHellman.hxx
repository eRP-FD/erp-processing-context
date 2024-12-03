/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_DIFFIEHELLMAN_HXX
#define EPA_LIBRARY_CRYPTO_DIFFIEHELLMAN_HXX

#include "library/crypto/CryptoTypes.hxx"

#include <string>

#include "shared/crypto/OpenSslHelper.hxx"

namespace epa
{

/**
 * Interface for a DiffieHellman key exchange.
 *
 * It is run in two steps:
 * - exchange of the public keys. This could be done via the TEE protocol at the Hello stage.
 *   The getPublicKey() and setPeerPublicKey() methods can be called in any order.
 * - compute the symmetric key by calling `getSymmetricKey`
 *
 */
class DiffieHellman
{
public:
    /**
     * Create a new object that is based on a new public/private key pair for the specified curve.
     */
    DiffieHellman(int publicKeyAlgorithmNID, int curveNID);
    /**
     * Create a new object for the given public/private key pair.
     * This variant is likely ever used in tests as for production the private key
     * will likely reside in the HSM.
     */
    explicit DiffieHellman(const shared_EVP_PKEY& keyPair);
    ~DiffieHellman() = default;

    /**
     * Overwrites the private key in the internal key pair with the given one, then derives the
     * public key (coordinates) from the private key and also updates it.
     *
     * This is useful for A_21888, where a fixed key pair must be used for the VAU/TEE protocol
     * handshake.
     */
    void replaceKeyPairFromPrivateKey(const std::string& privateKeyHex);

    /**
     * Overwrites the local key pair with the one.
     *
     * Used to implement A_24608 (e4a), where a fixed key pair must be used for the VAU/TEE protocol
     * handshake.
     */
    DiffieHellman& replaceKeyPairFromPrivateKey(const shared_EVP_PKEY& privateKey);

    shared_EVP_PKEY getPublicKey();
    shared_EVP_PKEY getPrivateKey();
    DiffieHellman& setPeerPublicKey(const shared_EVP_PKEY& peerPublicKey);
    std::string getSharedKey();
    std::string getSharedKey(shared_EVP_PKEY eckey);

    /**
     * The HKDF does not really belong here, but there is also not a better place for it (yet).
     *
     * Control of the length of the resulting key is one of the features of the HKDF algorithm.
     * We will always use it for Aes256Gcm and for KeyId. Both have a length of 32 bytes.
     */
    static EncryptionKey hkdf(
        const std::string& sharedKey,
        const std::string& derivationKey,
        size_t keyLength);

    /**
     * Delete the key pair.
     * Typically called as reaction to an error which requires safe deletion of all keys.
     */
    void clear();

private:
    // GEMREQ-start A_15547
    shared_EVP_PKEY mKeyPair;
    shared_EVP_PKEY mPeerPublicKey;
    // GEMREQ-end A_15547
};

} // namespace epa
#endif
