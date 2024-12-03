/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_TEE3_TEEHANDSHAKEBASE_HXX
#define EPA_LIBRARY_CRYPTO_TEE3_TEEHANDSHAKEBASE_HXX

#include "library/crypto/tee/KeyId.hxx"
#include "library/crypto/tee3/Tee3Protocol.hxx"

namespace epa
{

class TeeHandshakeBase
{
public:
    virtual ~TeeHandshakeBase() = default;

    struct HkdfResult
    {
        Tee3Protocol::SymmetricKeys keyConfirmation;
        Tee3Protocol::SymmetricKeys applicationData;
        KeyId keyId;
    };

    /**
     * Perform the checks that are mandated by A_24608.
     */
    static void verifyVauKeys(
        const Tee3Protocol::EcdhPublicKey& ecdhPublicKey,
        const BinaryBuffer& kyber768PublicKey);
    static Tee3Protocol::Encapsulation encapsulate(
        const Tee3Protocol::EcdhPublicKey& ecdhPublicKey,
        const BinaryBuffer& kyber768PublicKey);
    static Tee3Protocol::SharedKeys decapsulate(
        const Tee3Protocol::CipherTexts& ciphertexts,
        const Tee3Protocol::PrivateKeys& privateKeys);
    static Tee3Protocol::SymmetricKeys hkdf(const Tee3Protocol::SharedKeys& kemResult1);
    static HkdfResult hkdf(const SensitiveDataGuard& sharedSecret);

    /**
     * Aka AEAD_enc.
     * A_24619: Encryption with AES/GCM with 96 bit, random IV and 128 bit authentication tag.
     * @param key AES key
     * @param plaintext  the bytes that are to be encrypted
     * @returns (A_24619) concatenation of IV, ciphertext, authentication tag.
     */
    static BinaryBuffer aeadEncrypt(const SensitiveDataGuard& key, const BinaryBuffer& plaintext);

    /**
     * Aka AEAD_dec.
     * A_24619: Decryption with AES/GCM.
     * @param key AES key
     * @param ciphertext according to (A_24619) the concatenation of IV, the actual ciphertext,
     * authentication tag.
     * @param description to include in error messages
     */
    static BinaryBuffer aeadDecrypt(
        const SensitiveDataGuard& key,
        const BinaryBuffer& ciphertext,
        std::string_view description);

protected:
    /**
     * The default behavior that is implemented here is to do nothing.
     */
    virtual void report(std::string_view name, const BinaryBuffer& value);

    /**
     * The default behavior that is implemented here is to do nothing.
     */
    virtual void report(std::string_view name, std::string_view value);
};

} // namespace epa

#endif
