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

#include <openssl/core_names.h>
#include <openssl/param_build.h>


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
    auto privKeyBn = shared_BN::make();
    auto count = BN_hex2bn(privKeyBn.getP(), privateKeyHex.c_str());
    AssertOpenSsl(gsl::narrow<size_t>(count) == privateKeyHex.size())
        << "conversion of hex private key failed";

    // calculate the pubkey from the private key
    auto group = std::unique_ptr<EC_GROUP, decltype(&EC_GROUP_free)>{
        EC_GROUP_new_by_curve_name(NID_brainpoolP256r1), &EC_GROUP_free};
    auto pubkeyPoint = std::unique_ptr<EC_POINT, decltype(&EC_POINT_free)>{
        EC_POINT_new(group.get()), &EC_POINT_free};
    auto result =
        EC_POINT_mul(group.get(), pubkeyPoint.get(), privKeyBn, nullptr, nullptr, nullptr);
    AssertOpenSsl(result == 1) << "EC_POINT_mul failed";
    std::array<unsigned char, 1 + (2 * 32)> pubkeyUncompressed{};
    auto len = EC_POINT_point2oct(
        group.get(),
        pubkeyPoint.get(),
        POINT_CONVERSION_UNCOMPRESSED,
        pubkeyUncompressed.data(),
        pubkeyUncompressed.size(),
        nullptr);
    AssertOpenSsl(len > 0) << "EC_POINT_point2oct failed";

    // build the pkey from pubkey and private key
    const std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> pctx{
        EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr), &EVP_PKEY_CTX_free};
    AssertOpenSsl(EVP_PKEY_fromdata_init(pctx.get()) == 1) << "EVP_PKEY_fromdata_init failed";
    const std::unique_ptr<OSSL_PARAM_BLD, decltype(&OSSL_PARAM_BLD_free)> paramsBuild{
        OSSL_PARAM_BLD_new(), &OSSL_PARAM_BLD_free};
    OSSL_PARAM_BLD_push_utf8_string(
        paramsBuild.get(), OSSL_PKEY_PARAM_GROUP_NAME, OBJ_nid2sn(NID_brainpoolP256r1), 0);
    OSSL_PARAM_BLD_push_BN(paramsBuild.get(), OSSL_PKEY_PARAM_PRIV_KEY, privKeyBn);
    OSSL_PARAM_BLD_push_octet_string(
        paramsBuild.get(),
        OSSL_PKEY_PARAM_PUB_KEY,
        pubkeyUncompressed.data(),
        pubkeyUncompressed.size());

    const std::unique_ptr<OSSL_PARAM, decltype(&OSSL_PARAM_free)> params{
        OSSL_PARAM_BLD_to_param(paramsBuild.get()), &OSSL_PARAM_free};
    AssertOpenSsl(params != nullptr) << "Failed to generate OSSL_PARAM";

    EVP_PKEY* pkey = nullptr;
    result = EVP_PKEY_fromdata(pctx.get(), &pkey, EVP_PKEY_KEYPAIR, params.get());
    AssertOpenSsl(result == 1) << "EVP_PKEY_fromdata failed";
    mKeyPair = shared_EVP_PKEY::make(pkey);
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
    AssertOpenSsl(EVP_PKEY_is_a(peerPublicKey, "EC") == 1) << "Key is not an EC key";
    std::array<uint8_t, 256> pubkey{};
    size_t len = 0;
    const auto status = EVP_PKEY_get_octet_string_param(
        peerPublicKey, OSSL_PKEY_PARAM_PUB_KEY, pubkey.data(), pubkey.size(), &len);
    AssertOpenSsl(status == 1) << "unable to obtain public key";

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
