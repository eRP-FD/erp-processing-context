/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/tee3/TeeHandshakeBase.hxx"
#include "library/crypto/EllipticCurve.hxx"
#include "library/crypto/Key.hxx"
#include "library/crypto/KeyDerivationUtils.hxx"
#include "library/crypto/tee3/Kyber768.hxx"
#include "library/stream/StreamHelper.hxx"
#include "library/util/Assert.hxx"
#include "library/util/ByteHelper.hxx"
#include "library/util/cbor/CborDeserializer.hxx"
#include "library/util/cbor/CborSerializer.hxx"

namespace epa
{

// GEMREQ-start A_24624-01#verifyVauKeys, A_24608#verifyVauKeys
void TeeHandshakeBase::verifyVauKeys(
    const Tee3Protocol::EcdhPublicKey& ecdhPublicKey,
    const BinaryBuffer& kyber768PublicKey)
{
    try
    {
        // A_24608: verify curve type
        Assert(ecdhPublicKey.curve == "P-256");

        // A_24608: x,y is a point on the curve
        Assert(ecdhPublicKey.x.size() == 32);
        Assert(ecdhPublicKey.y.size() == 32);
        const auto evpPkey = ecdhPublicKey.toEvpPkey();
        Assert(EllipticCurve::Prime256v1->isOnCurve(*evpPkey));

        // A_24608: verify public kyber768 key
        Assert(Kyber768::isValidPublicKey(kyber768PublicKey));
    }
    catch (const std::runtime_error&)
    {
        // Translate into TeeError.
        LOG(ERROR) << "TEE handshake: verification of public keys failed";
        throw TeeError(TeeError::Code::FailedPublicKeyVerification, "");
    }
}
// GEMREQ-end A_24624-01#verifyVauKeys, A_24608#verifyVauKeys


// GEMREQ-start A_24623#encapsulate, A_24608#encapsulate
Tee3Protocol::Encapsulation TeeHandshakeBase::encapsulate(
    const Tee3Protocol::EcdhPublicKey& ecdhPublicKey,
    const BinaryBuffer& kyber768PublicKey)
{
    verifyVauKeys(ecdhPublicKey, kyber768PublicKey);

    // Run the elliptic curve encapsulation in the form of a diffie hellman key exchange.
    // The local private/public key pair is created by the diffie hellman object.
    // Note that the public key of the temporary key pair, currently owned by the diffie hellman
    // object, is returned with the response to the client but that the private key is not used
    // at all after this point and therefore is not persisted.
    auto dh = EllipticCurve::Prime256v1->createDiffieHellman();
    const auto eccdhSharedSecret = dh.setPeerPublicKey(ecdhPublicKey.toEvpPkey()).getSharedKey();

    const auto [kyber768SharedSecret, kyber768CipherText] // Kyber768_ss, Kyber768_ct
        = Kyber768::encapsulate(kyber768PublicKey);

    auto ecdhCipherText = Tee3Protocol::EcdhPublicKey::fromEvpPkey(dh.getPublicKey());

    return Tee3Protocol::Encapsulation{
        .sharedKeys =
            Tee3Protocol::SharedKeys{
                .ecdhSharedSecret = SensitiveDataGuard(eccdhSharedSecret),
                .kyber768SharedSecret = kyber768SharedSecret},
        .cipherTexts = Tee3Protocol::CipherTexts{
            .ecdhCipherText = std::move(ecdhCipherText), .kyber768CipherText = kyber768CipherText}};
}
// GEMREQ-end A_24623#encapsulate, A_24608#encapsulate


// GEMREQ-start A_24623#decapsulate, A_24626#decapsulate
Tee3Protocol::SharedKeys TeeHandshakeBase::decapsulate(
    const Tee3Protocol::CipherTexts& ciphertexts,
    const Tee3Protocol::PrivateKeys& privateKeys)
{
    const auto eccPublicKeySender // ecc_public_key_sender
        = ciphertexts.ecdhCipherText.toEvpPkey();
    Assert(EllipticCurve::Prime256v1->isOnCurve(*eccPublicKeySender))
        << "peer public key is not on P256";

    shared_EVP_PKEY localPrivateKey;
    const auto rawPrivateKey = BinaryBuffer(*privateKeys.ecdhPrivateKey).toString();
    if (rawPrivateKey.starts_with("-----BEGIN"))
        localPrivateKey = Key::privateKeyFromPemString(rawPrivateKey);
    else
        localPrivateKey = Key::privateKeyFromDer(rawPrivateKey);

    Assert(localPrivateKey != nullptr);

    // Perform the decapsulation for the elliptic curve key with a diffie hellman key exchange.
    // It is based on the remote public key and a previously created local private/public key pair.
    const auto ecdhSharedSecret // ecdh_ss
        = EllipticCurve::Prime256v1->createDiffieHellman()
              .replaceKeyPairFromPrivateKey(localPrivateKey)
              .setPeerPublicKey(eccPublicKeySender)
              .getSharedKey();

    const auto kyber768SharedSecret // Kyber768_ss
        = Kyber768::decapsulate(privateKeys.kyber768PrivateKey, ciphertexts.kyber768CipherText);

    return Tee3Protocol::SharedKeys{
        .ecdhSharedSecret = SensitiveDataGuard(ecdhSharedSecret),
        .kyber768SharedSecret = kyber768SharedSecret};
}
// GEMREQ-end A_24623#decapsulate, A_24626#decapsulate


// GEMREQ-start A_24623#hkdf1, A_24608#hkdf1
Tee3Protocol::SymmetricKeys TeeHandshakeBase::hkdf(const Tee3Protocol::SharedKeys& kemResult1)
{
    Assert(! kemResult1.ecdhSharedSecret.empty());
    Assert(! kemResult1.kyber768SharedSecret.empty());

    // A_24608,A_24623: concatenation: ss_e = ss_e_ecdh || ss_e_kyber768.
    const auto sharedSecret = SensitiveDataGuard(
        BinaryBuffer::concatenate(*kemResult1.ecdhSharedSecret, *kemResult1.kyber768SharedSecret));

    // A_24608,A_24623: derive 64 bytes with empty info and no salt.
    const auto targetLength = 2 * 32; // 2 256-Bit-AES-Schlüssel
    const auto k1 = KeyDerivationUtils::performHkdfHmacSha256(sharedSecret, targetLength);
    Assert(k1.size() == targetLength);

    // A_24608,A_24623: split the derived 64 bytes into two 32 byte keys K1_c2s und K1_s2c.
    return Tee3Protocol::SymmetricKeys{
        .clientToServer = EncryptionKey(k1.data(), 32),     // K1_c2s
        .serverToClient = EncryptionKey(k1.data() + 32, 32) // K1_s2c
    };

    // tmp is of type SensitiveDataGuard and will safely delete its temporary content on exit.
}
// GEMREQ-end A_24623#hkdf1, A_24608#hkdf1


// GEMREQ-start A_24623#hkdf2, A_24626#hkdf2
TeeHandshakeBase::HkdfResult TeeHandshakeBase::hkdf(const SensitiveDataGuard& sharedSecret)
{
    // 4 * 256-Bit-AES-Keys + 1 * 256-Bit KeyID = 5 * 256 Bit = 5 * 32 Bytes
    const auto targetLength = 5 * 32;

    // Derive the 4 keys and 1 keyId with no salt or info value.
    const auto tmp = KeyDerivationUtils::performHkdfHmacSha256(sharedSecret, targetLength);
    Assert(tmp.size() == targetLength);

    return {
        .keyConfirmation{
            .clientToServer = EncryptionKey(tmp.data(), 32),     // item 0
            .serverToClient = EncryptionKey(tmp.data() + 64, 32) // item 2
        },
        .applicationData{
            .clientToServer = EncryptionKey(tmp.data() + 32, 32), // item 1
            .serverToClient = EncryptionKey(tmp.data() + 96, 32)  // item 3
        },
        .keyId = KeyId::fromBinaryView(toBinaryView(tmp.data() + 128, 32)) // item 4
    };
}
// GEMREQ-end A_24623#hkdf2, A_24626#hkdf2

// GEMREQ-start A_24623#aeadEncrypt, A_24608#aeadEncrypt, A_24626#aeadEncrypt
BinaryBuffer TeeHandshakeBase::aeadEncrypt(
    const SensitiveDataGuard& key,
    const BinaryBuffer& plaintext)
{
    Assert(key.size() == 32);
    Assert(! plaintext.empty());

    // A_249619: IV is created with a 96/8=12 random bytes.
    auto ivSupplier = AesGcmStreamCryptor::createRandomIV;

    auto encryptingStream = AesGcmStreamCryptor::makeEncryptingStream(
        [keyCopy = key]() { return keyCopy; },
        StreamFactory::makeReadableStream(plaintext.data(), plaintext.size()),
        ivSupplier);

    // A_24619: `to_string(encryptingStream)` returns the encrypted ciphertext prepended with the
    //           IV (96/8 = 12 bytes) and appended with the authentication tag (128/8 = 16 bytes).
    auto ciphertext = BinaryBuffer(to_string(encryptingStream));
    Assert(ciphertext.size() == 12 + plaintext.size() + 16);

    return ciphertext;
}
// GEMREQ-end A_24623#aeadEncrypt, A_24608#aeadEncrypt, A_24626#aeadEncrypt


// GEMREQ-start A_24623#aeadDecrypt, A_24623#aeadDecrypt
BinaryBuffer TeeHandshakeBase::aeadDecrypt(
    const SensitiveDataGuard& key,
    const BinaryBuffer& ciphertext,
    const std::string_view description)
{
    try
    {
        Assert(key.size() == 32);
        Assert(! ciphertext.empty());

        auto decryptingStream = AesGcmStreamCryptor::makeDecryptingStream(
            [keyCopy = key]() { return keyCopy; },
            StreamFactory::makeReadableStream(ciphertext.data(), ciphertext.size()));

        // A_24619: Decryption removes the leading IV (12 bytes) and the trailing authentication
        //          tag (16 bytes) and decrypts the remaining message.
        auto plaintext = BinaryBuffer(to_string(decryptingStream));
        Assert(plaintext.size() == ciphertext.size() - 12 - 16);

        return plaintext;
    }
    catch (const std::runtime_error&)
    {
        // among others A_24626: A decryption failure is translated into an error that terminates the handshake.
        LOG(ERROR) << "TEE handshake: " << description << " failed";
        throw TeeError(TeeError::Code::GcmDecryptionFailure, std::string(description));
    }
}
// GEMREQ-end A_24623#aeadDecrypt, A_24623#aeadDecrypt


void TeeHandshakeBase::report(std::string_view, const BinaryBuffer&)
{
    // Do nothing. Any logging has to be done by derived classes.
}


void TeeHandshakeBase::report(std::string_view, const std::string_view)
{
    // Do nothing. Any logging has to be done by derived classes.
}

} // namespace epa
