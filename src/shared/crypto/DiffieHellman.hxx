/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CRYPTO_DIFFIEHELLMAN_HXX
#define ERP_PROCESSING_CONTEXT_CRYPTO_DIFFIEHELLMAN_HXX

#include "shared/util/SafeString.hxx"

#include "shared/crypto/OpenSslHelper.hxx"

#include <string>


/**
 * Interface for a DiffieHellman key exchange.
 *
 * It is run in two steps:
 * - exchange of the public keys. This could be done via the TEE protocol.
 *   The setPrivatePublicKey() and setPeerPublicKey() methods can be called in any order as long as they are called
 *   both before getSharedKey().
 * - compute the symmetric key by calling `getSymmetricKey`
 *
 */
class DiffieHellman
{
public:
    /**
     * Set the local public key, together with the associated private key.
     */
    void setPrivatePublicKey (const shared_EVP_PKEY& localPrivatePublicKey);

    /**
     * Set the peer's public key.
     */
    void setPeerPublicKey (const shared_EVP_PKEY& peerPublicKey);

    SafeString createSharedKey (std::optional<size_t> sharedKeyLength = {});

    /**
     * The HKDF does not really belong here, but there is also not a better place for it (yet).
     *
     * Control of the length of the resulting key is one of the features of the HKDF algorithm.
     * We will always use it for Aes256Gcm and for KeyId. Both have a length of 32 bytes.
     */
    static std::string hkdf (const SafeString& sharedKey, const std::string_view& derivationKey, size_t keyLength);

    /**
     * Delete the key pair.
     * Typically called as reaction to an error which requires safe deletion of all keys.
     */
    void clear (void);

private:
    shared_EVP_PKEY mKeyPair;
    shared_EVP_PKEY mPeerPublicKey;
};

#endif
