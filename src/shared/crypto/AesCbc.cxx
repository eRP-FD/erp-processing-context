/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "shared/crypto/AesCbc.hxx"
#include "shared/crypto/OpenSslHelper.hxx"
#include "shared/util/Expect.hxx"

namespace
{

EvpCipherCtxPtr createCipherContext()
{
    auto context = EvpCipherCtxPtr(EVP_CIPHER_CTX_new());
    OpenSslExpect(context != nullptr, "can't create cipher context for AES-CBC");
    EVP_CIPHER_CTX_set_padding(context.get(), 0);
    return context;
}

/**
 * The openSSL documentation does not make a statement about whether the pointer returned by EVP_aes_*_CBC has to be
 * freed. Therefore we assume that it does not.
 */
template<size_t KeyLengthBits>
const EVP_CIPHER* encryptionCipher();

template<>
const EVP_CIPHER* encryptionCipher<128>()
{
    return EVP_aes_128_cbc();
}

template<>
const EVP_CIPHER* encryptionCipher<256>()
{
    return EVP_aes_256_cbc();
}

template<size_t KeyLengthBits>
const EVP_CIPHER* encryptionCipher()
{
    Fail("encryptionCipher() not implemented for " + std::to_string(KeyLengthBits));
}

}// namespace

template<std::size_t KeyLengthBits>
std::string AesCbc<KeyLengthBits>::encrypt(std::string_view plaintext, const SafeString& key, std::string_view iv)
{
    Expect3(key.size() == KeyLength,
            "invalid key length, actual: " + std::to_string(key.size()) + ", expected: " + std::to_string(KeyLength),
            AesCbcException);
    Expect3(iv.size() == IvLength,
            "invalid IV length, actual: " + std::to_string(iv.size()) + ", expected: " + std::to_string(IvLength),
            AesCbcException);
    Expect3(plaintext.size() % BlockSize == 0, "plaintext must be multiple of keylength", AesCbcException);
    auto cipherContext = createCipherContext();
    int status = EVP_EncryptInit_ex(cipherContext.get(), encryptionCipher<KeyLengthBits>(), nullptr,
                                    static_cast<const unsigned char*>(key),
                                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                                    reinterpret_cast<const unsigned char*>(iv.data()));
    OpenSslExpect(status == 1, "can't initialize AES-CBC encryption");

    Expect3(plaintext.size() <= static_cast<size_t>(std::numeric_limits<int>::max()), "plain text too long",
            AesCbcException);

    std::string cipherText(plaintext.size(), 'X');
    int offset = 0;
    const int updateStatus =
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        EVP_EncryptUpdate(cipherContext.get(), reinterpret_cast<unsigned char*>(cipherText.data()), &offset,
                          // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                          reinterpret_cast<const unsigned char*>(plaintext.data()),
                          static_cast<int>(plaintext.size()));// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    Expect3(updateStatus == 1, "can't encrypt data", AesCbcException);
    Expect3(offset >= 0, "encryption failed", AesCbcException);
    // Verify that openSSL obeyed the input parameters and did not write more bytes than we told it to.
    Expect3(static_cast<size_t>(offset) <= cipherText.size(), "wrote too many bytes on encryption", AesCbcException);

    int count = 0;
    status =
        EVP_EncryptFinal_ex(cipherContext.get(), reinterpret_cast<unsigned char*>(cipherText.data()) + offset,// NOLINT
                            &count);
    Expect3(status == 1, "can't finalize AES-CBC encryption", AesCbcException);
    offset += count;
    Expect3(static_cast<size_t>(offset) == cipherText.size(), "ciphertext has invalid length", AesCbcException);

    return cipherText;
}

template<std::size_t KeyLengthBits>
SafeString AesCbc<KeyLengthBits>::decrypt(std::string_view ciphertext, const SafeString& key, std::string_view iv)
{
    Expect3(key.size() == KeyLength,
            "invalid key length, actual: " + std::to_string(key.size()) + ", expected: " + std::to_string(KeyLength),
            AesCbcException);
    Expect3(iv.size() == IvLength, "invalid IV length", AesCbcException);

    auto cipherContext = createCipherContext();
    int status = EVP_DecryptInit_ex(cipherContext.get(), encryptionCipher<KeyLengthBits>(), nullptr,
                                    static_cast<const unsigned char*>(key),
                                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                                    reinterpret_cast<const unsigned char*>(iv.data()));
    OpenSslExpect(status == 1, "can't initialize AES-CBC decryption");

    SafeString plaintext{ciphertext.size()};
    int offset = 0;
    status = EVP_DecryptUpdate(cipherContext.get(),
                               // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                               reinterpret_cast<unsigned char*>(static_cast<char*>(plaintext)), &offset,
                               // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                               reinterpret_cast<const unsigned char*>(ciphertext.data()),
                               static_cast<int>(ciphertext.size()));

    Expect3(status == 1, "can't decrypt data", AesCbcException);
    // Verify that openSSL obeyed our parameters and did not write past of the plaintext string.
    Expect3(offset >= 0, "decryption failed", AesCbcException);
    Expect3(static_cast<size_t>(offset) <= plaintext.size(), "openSSl wrote too many bytes", AesCbcException);

    int count = 0;
    status = EVP_DecryptFinal_ex(
        cipherContext.get(), reinterpret_cast<unsigned char*>(static_cast<char*>(plaintext)) + offset, &count);// NOLINT
    Expect3(status == 1, "can't finalize AES-CBC decryption", AesCbcException);
    offset += count;
    Expect3(static_cast<size_t>(offset) == plaintext.size(), "wrote too many bytes on decryption", AesCbcException);
    return plaintext;
}

template class AesCbc<128>;
template class AesCbc<256>;
