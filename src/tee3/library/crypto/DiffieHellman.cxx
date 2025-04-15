/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/DiffieHellman.hxx"
#include "library/crypto/EllipticCurve.hxx"
#include "library/util/Assert.hxx"
#include "library/wrappers/OpenSsl.hxx"

#include "fhirtools/util/Gsl.hxx"
#include "shared/crypto/OpenSslHelper.hxx"

namespace epa
{

#define throwIfNot(expression, message) AssertOpenSsl(expression) << message


// GEMREQ-start GS-A_4368#DiffieHellman
DiffieHellman::DiffieHellman(const int publicKeyAlgorithm, const int curveNID)
  : mKeyPair(),
    mPeerPublicKey()
{
    // Based on an example at https://wiki.openssl.org/index.php/Elliptic_Curve_Diffie_Hellman

    /* Create the context for parameter generation */
    auto parameterContext =
        shared_EVP_PKEY_CTX::make(EVP_PKEY_CTX_new_id(publicKeyAlgorithm, nullptr));
    throwIfNot(parameterContext.isSet(), "can not create parameter context");

    /* Initialise the parameter generation */
    int status = EVP_PKEY_paramgen_init(parameterContext);
    throwIfNot(status == 1, "can not initialize parameter context");

    status = EVP_PKEY_CTX_set_ec_paramgen_curve_nid(parameterContext, curveNID);
    throwIfNot(status == 1, "can not set elliptic curve at parameter context");

    /* Create the parameter object params */
    shared_EVP_PKEY parameters;
    status = EVP_PKEY_paramgen(parameterContext, parameters.getP());
    throwIfNot(status == 1, "can not create parameter object for key pair generation");

    /* Create the context for the key generation */
    auto keyContext = shared_EVP_PKEY_CTX::make(EVP_PKEY_CTX_new(parameters, nullptr));
    throwIfNot(keyContext.isSet(), "can not create context for key pair generation");

    /* Generate the key */
    status = EVP_PKEY_keygen_init(keyContext);
    throwIfNot(status == 1, "can not initialize key pair generation");
    status = EVP_PKEY_keygen(keyContext, mKeyPair.getP());
    throwIfNot(status == 1, "can not generate key pair");
}
// GEMREQ-end GS-A_4368#DiffieHellman

DiffieHellman::DiffieHellman(const shared_EVP_PKEY& keyPair)
  : mKeyPair(keyPair),
    mPeerPublicKey()
{
}


/**
 * Unlike in OpenSSL 1.1.x, we cannot simply overwrite the private and public keys of mKeyPair
 * with the given private key. The relevant manpage section says:
 *
 * > Note that if an EVP_PKEY was not constructed using one of the deprecated functions such as
 * > EVP_PKEY_set1_RSA(), EVP_PKEY_set1_DSA(), EVP_PKEY_set1_DH() or EVP_PKEY_set1_EC_KEY() […],
 * > then the internal key will be managed by a provider (see provider(7)). In that case the key
 * > returned by […] EVP_PKEY_get1_EC_KEY() […] or EVP_PKEY_get0_EC_KEY() will be a cached copy
 * > of the provider's key. Subsequent updates to the provider's key will not be reflected back
 * > in the cached copy, and updates made by an application to the returned key will not be
 * > reflected back in the provider's key.
 *
 * Instead, we have to create a new key pair using EVP_PKEY_new and then "construct" it using
 * the EVP_PKEY_set1_EC_KEY function.
 *
 * The clean way in OpenSSL 3+ would be to use EVP_PKEY_fromdata (see example in manpage).
 * EVP_PKEY_new_raw_private_key_ex would be even easier, but unfortunately doesn't support our
 * curve.
 */
// GEMREQ-start A_21888
void DiffieHellman::replaceKeyPairFromPrivateKey(const std::string& privateKeyHex)
{
    auto ecKey = shared_EC_KEY::make(EC_KEY_new_by_curve_name(NID_brainpoolP256r1));
    throwIfNot(ecKey != nullptr, "cannot create new brainpoolP256R1 EC_KEY");

    // Set private key.
    auto privateKey = shared_BN::make();
    throwIfNot(BN_hex2bn(privateKey.getP(), privateKeyHex.c_str()) > 0, "cannot parse private key");
    throwIfNot(EC_KEY_set_private_key(ecKey, privateKey) == 1, "cannot set private key");

    // Calculate public key from private key within given group.
    const EC_GROUP* group = EC_KEY_get0_group(ecKey);
    auto publicKey = shared_EC_POINT::make(EC_POINT_new(group));
    throwIfNot(
        EC_POINT_mul(group, publicKey, privateKey, nullptr, nullptr, nullptr) == 1,
        "cannot multiply group with private key to retrieve public key");
    throwIfNot(EC_KEY_set_public_key(ecKey, publicKey) == 1, "cannot set public key");
    mKeyPair = shared_EVP_PKEY::make();
    throwIfNot(mKeyPair != nullptr, "cannot create new EVP_PKEY");
    EVP_PKEY_set1_EC_KEY(mKeyPair, ecKey);
}
// GEMREQ-end A_21888


DiffieHellman& DiffieHellman::replaceKeyPairFromPrivateKey(const shared_EVP_PKEY& privateKey)
{
    mKeyPair = privateKey;

    Assert(EllipticCurve::Prime256v1->isOnCurve(*mKeyPair));

    return *this;
}


shared_EVP_PKEY DiffieHellman::getPublicKey()
{
    return mKeyPair;
}


shared_EVP_PKEY DiffieHellman::getPrivateKey()
{
    return mKeyPair;
}


DiffieHellman& DiffieHellman::setPeerPublicKey(const shared_EVP_PKEY& peerPublicKey)
{
    throwIfNot(peerPublicKey.isSet(), "got invalid peer's public key");
    const EC_KEY* ecKey = EVP_PKEY_get0_EC_KEY(peerPublicKey);
    throwIfNot(ecKey != nullptr, "peer key has no public key");
    const EC_POINT* ecPoint = EC_KEY_get0_public_key(ecKey);
    throwIfNot(ecPoint != nullptr, "peer's public key not valid");

    mPeerPublicKey = peerPublicKey;

    return *this;
}


std::string DiffieHellman::getSharedKey()
{
    return getSharedKey(mPeerPublicKey);
}


std::string DiffieHellman::getSharedKey(shared_EVP_PKEY eckey)
{
    /* Create the context for the shared secret derivation */
    auto sharedSecretContext = shared_EVP_PKEY_CTX::make(EVP_PKEY_CTX_new(mKeyPair, nullptr));
    throwIfNot(sharedSecretContext.isSet(), "can not create context for shared key derivation");

    /* Initialise */
    int status = EVP_PKEY_derive_init(sharedSecretContext);
    throwIfNot(status == 1, "can not initialize shared key derivation");

    /* Provide the peer public key */
    status = EVP_PKEY_derive_set_peer(sharedSecretContext, eckey);
    throwIfNot(status == 1, "can not set peer's public key");

    /* Determine buffer length for shared secret */
    size_t secretLen = 0;
    status = EVP_PKEY_derive(sharedSecretContext, nullptr, &secretLen);
    throwIfNot(status == 1, "can not determine length of shared secret");

    /* Create the buffer */
    auto secret = std::string(secretLen, ' ');

    /* Derive the shared secret */
    status = EVP_PKEY_derive(
        sharedSecretContext, reinterpret_cast<unsigned char*>(secret.data()), &secretLen);
    throwIfNot(status == 1, "derivation of shared secret failed");
    throwIfNot(secretLen == secret.size(), "derivation of shared secret failed, length mismatch");

    return secret;
}


// GEMREQ-start A_16943-01#hkdf
EncryptionKey DiffieHellman::hkdf(
    const std::string& sharedKey,
    const std::string& derivationKey,
    const size_t keyLength)
{
    Expects(keyLength > 0);

    auto context = shared_EVP_PKEY_CTX::make();

    int status = EVP_PKEY_derive_init(context);
    throwIfNot(status == 1, "can not initialize HKDF context");

    status = EVP_PKEY_CTX_set_hkdf_md(context, EVP_sha256());
    throwIfNot(status == 1, "can not set SHA256 as digest algorithm");

    // Salt is not used.

    status = EVP_PKEY_CTX_set1_hkdf_key(
        context,
        reinterpret_cast<const unsigned char*>(sharedKey.data()),
        gsl::narrow<int>(sharedKey.size()));
    throwIfNot(status == 1, "can not set HKDF shared key");

    status = EVP_PKEY_CTX_add1_hkdf_info(
        context,
        reinterpret_cast<const unsigned char*>(derivationKey.data()),
        gsl::narrow<int>(derivationKey.size()));
    throwIfNot(status == 1, "can not set HKDF info");

    BinaryBuffer derivedKey(std::string(keyLength, ' '));
    size_t actualLength = keyLength;
    status = EVP_PKEY_derive(context, derivedKey.data(), &actualLength);
    throwIfNot(status == 1, "HKDF key derivation failed");
    throwIfNot(actualLength == keyLength, "derived HKDF key does not have the requested size");

    return derivedKey;
}
// GEMREQ-end A_16943-01#hkdf


void DiffieHellman::clear()
{
    mKeyPair.reset();
    mPeerPublicKey.reset();
}

} // namespace epa
