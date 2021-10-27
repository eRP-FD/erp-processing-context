/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "DiffieHellman.hxx"

#include "erp/crypto/OpenSslHelper.hxx"
#include "erp/util/Expect.hxx"

#include <openssl/kdf.h>



void DiffieHellman::setPrivatePublicKey (const shared_EVP_PKEY& localPrivatePublicKey)
{
    mKeyPair = localPrivatePublicKey;
}


void DiffieHellman::setPeerPublicKey (const shared_EVP_PKEY& peerPublicKey)
{
    Expect(peerPublicKey.isSet(), "got invalid peer's public key");

    EC_KEY* ecKey = EVP_PKEY_get0_EC_KEY(peerPublicKey.removeConst());
    Expect(ecKey != nullptr, "peer key has no public key");
    const EC_POINT* ecPoint = EC_KEY_get0_public_key(ecKey);
    Expect(ecPoint!=nullptr, "peer's public key not valid");

    mPeerPublicKey = peerPublicKey;
}


SafeString DiffieHellman::createSharedKey (const std::optional<size_t> keyLength)
{
    Expect(mKeyPair.isSet(), "local private and public key pair is not set");
    Expect(mPeerPublicKey.isSet(), "peer's public key is not set");

    /* Create the context for the shared secret derivation */
    auto sharedSecretContext = shared_EVP_PKEY_CTX::make(EVP_PKEY_CTX_new(mKeyPair, nullptr));
    Expect(sharedSecretContext.isSet(), "can not create context for shared key derivation");

    /* Initialise */
    int status = EVP_PKEY_derive_init(sharedSecretContext);
    Expect(status == 1, "can not initialize shared key derivation");

    /* Provide the peer public key */
    status = EVP_PKEY_derive_set_peer(sharedSecretContext, mPeerPublicKey);
    Expect(status == 1, "can not set peer's public key");

    /* Determine buffer length for shared secret */
    size_t secretLength = keyLength.value_or(0);
    if (secretLength == 0)
    {
        // No explict length provided. Determine the default size.
        status = EVP_PKEY_derive(sharedSecretContext, nullptr, &secretLength);
        Expect(status == 1, "can not determine length of shared secret");
    }

    /* Create the buffer */
    SafeString secret(secretLength);

    /* Derive the shared secret */
    status = EVP_PKEY_derive(sharedSecretContext, reinterpret_cast<unsigned char*>(static_cast<char*>(secret)), &secretLength);
    Expect(status == 1, "derivation of shared secret failed");
    Expect(secretLength == secret.size(), "derivation of shared secret failed");

    return secret;
}


std::string DiffieHellman::hkdf (const SafeString& sharedKey, const std::string_view& derivationKey, const size_t keyLength)
{
    Expect(keyLength > 0, "invalid requested key length");
    // Check key lengths for some semi-serious bounds. The checks are used for conversion from size_t to int, not to validate the
    // input data.
    Expect(keyLength < 1024, "invalid requested key length");
    Expect(sharedKey.size() < 1024, "invalid sharedKey");
    Expect(derivationKey.size() < 1024, "invalid derivationKey");

    auto context = shared_EVP_PKEY_CTX::make();

    int status = EVP_PKEY_derive_init(context);
    Expect(status==1, "can not initialize HKDF context");

    status = EVP_PKEY_CTX_set_hkdf_md(context, EVP_sha256());
    Expect(status==1, "can not set SHA256 as digest algorithm");

    // Salt is not used.

    status = EVP_PKEY_CTX_set1_hkdf_key(context, static_cast<const char*>(sharedKey), static_cast<int>(sharedKey.size()));
    Expect(status==1, "can not set HKDF shared key");
    
    status = EVP_PKEY_CTX_add1_hkdf_info(context, derivationKey.data(), static_cast<int>(derivationKey.size()));
    Expect(status==1, "can not set HKDF info");

    std::string derivedKey(keyLength, ' ');
    size_t actualLength = keyLength;
    status = EVP_PKEY_derive(context, reinterpret_cast<unsigned char*>(derivedKey.data()), &actualLength);
    Expect(status==1, "HKDF key derivation failed");
    Expect(actualLength == keyLength, "derived HKDF key does not have the requested size");

    return derivedKey;
}


void DiffieHellman::clear (void)
{
    mKeyPair.reset();
    mPeerPublicKey.reset();
}
