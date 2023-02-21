/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/crypto/AesGcm.hxx"

#include "erp/util/Expect.hxx"

#include <openssl/evp.h>
#include <openssl/err.h>


namespace {
    using unique_cipher_context = std::unique_ptr<EVP_CIPHER_CTX,void(*)(EVP_CIPHER_CTX*)>;

    unique_cipher_context createCipherContext (void)
    {
        auto context =
            std::unique_ptr<EVP_CIPHER_CTX, void (*)(EVP_CIPHER_CTX*)>(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
        Expect3(context!=nullptr, "can't create cipher context for AES-GCM", AesGcmException);
        return context;
    }


    template <size_t KeyLengthBits>
    class Encryptor
    {
    public:
        Encryptor (
            size_t plaintextLength,
            const SafeString& symmetricKey,
            const std::string_view& iv);

        void encrypt (const std::string_view& plaintext);

        EncryptionResult finalize (void);

    private:
        int mOffset;
        std::string mCiphertext;
        unique_cipher_context mCipherContext;
    };


    template <size_t KeyLengthBits>
    class Decryptor
    {
    public:
        Decryptor (
            size_t plaintextLength,
            const SafeString& symmetricKey,
            const std::string_view& iv,
            const std::string_view& authenticationTag);

        void decrypt (const std::string_view& data);

        // mPlaintext is moved out of the class.
        SafeString finalize (void);

    private:
        int mOffset;
        SafeString mPlaintext;
        unique_cipher_context mCipherContext;
    };
}


/**
 * The openSSL documentation does not make a statement about whether the pointer returned by EVP_aes_*_gcm has to be
 * freed. Therefore we assume that it does not.
 */
template<size_t KeyLengthBits>
const evp_cipher_st* encryptionCipher();

template<>
const evp_cipher_st* encryptionCipher<128>()
{
    static const auto* cipher = EVP_aes_128_gcm();
    return cipher;
}

template<>
const evp_cipher_st* encryptionCipher<256>()
{
    static const auto* cipher = EVP_aes_256_gcm();
    return cipher;
}

template<size_t KeyLengthBits>
const evp_cipher_st* encryptionCipher()
{
    Fail("encryptionCipher() not implemented for " + std::to_string(KeyLengthBits));
}


template <size_t KeyLengthBits>
Encryptor<KeyLengthBits>::Encryptor (
    size_t plaintextLength,
    const SafeString& symmetricKey,
    const std::string_view& iv)
    : mOffset(0),
      mCiphertext(plaintextLength, 'X'),
      mCipherContext(createCipherContext())
{
    Expect3(symmetricKey.size() == AesGcm<KeyLengthBits>::KeyLength,
            "invalid key length, actual: " + std::to_string(symmetricKey.size()) +
                ", expected: " + std::to_string(AesGcm<KeyLengthBits>::KeyLength),
            AesGcmException);
    Expect3(iv.size() == AesGcm<KeyLengthBits>::IvLength,
            "invalid IV length, actual: " + std::to_string(iv.size()) +
                ", expected: " + std::to_string(AesGcm<KeyLengthBits>::IvLength),
            AesGcmException);

    const int status = EVP_EncryptInit_ex(
        mCipherContext.get(),
        encryptionCipher<KeyLengthBits>(),
        nullptr,
        reinterpret_cast<const unsigned char*>(static_cast<const char*>(symmetricKey)),
        reinterpret_cast<const unsigned char*>(iv.data()));
    Expect3(status==1, "can't initialize AES-GCM encryption", AesGcmException);
}


template <size_t KeyLengthBits>
void Encryptor<KeyLengthBits>::encrypt (const std::string_view& plaintext)
{
    Expect3(plaintext.size() <= static_cast<size_t>(std::numeric_limits<int>::max()), "plain text too long",
            AesGcmException);

    mOffset = 0;
    const int status = EVP_EncryptUpdate(
        mCipherContext.get(),
        reinterpret_cast<unsigned char*>(mCiphertext.data()),
        &mOffset,
        reinterpret_cast<const unsigned char*>(plaintext.data()),
        static_cast<int>(plaintext.size()));

    Expect3(status==1, "can't encrypt data", AesGcmException);
    Expect3(mOffset >= 0, "encryption failed", AesGcmException);
    // Verify that openSSL obeyed the input parameters and did not write more bytes than we told it to.
    Expect3(static_cast<size_t>(mOffset) <= mCiphertext.size(), "wrote too many bytes on encryption", AesGcmException);
}

template <size_t KeyLengthBits>
EncryptionResult Encryptor<KeyLengthBits>::finalize (void)
{
    int count = 0;
    int status = EVP_EncryptFinal_ex(
        mCipherContext.get(),
        reinterpret_cast<unsigned char*>(mCiphertext.data()) + mOffset,
        &count);
    Expect3(status==1, "can't finalize AES-GCM encryption", AesGcmException);
    mOffset += count;
    Expect3(static_cast<size_t>(mOffset) == mCiphertext.size(), "ciphertext has invalid length", AesGcmException);

    std::string authenticationTag (AesGcm<KeyLengthBits>::AuthenticationTagLength, 'X');
    status = EVP_CIPHER_CTX_ctrl(mCipherContext.get(), EVP_CTRL_GCM_GET_TAG, static_cast<int>(authenticationTag.size()),
                                 authenticationTag.data());
    Expect3(status==1, "can't retrieve authentication tag", AesGcmException);

    return {
        mCiphertext,
        authenticationTag};
}


template <std::size_t KeyLengthBits>
Decryptor<KeyLengthBits>::Decryptor (
    size_t plaintextLength,
    const SafeString& symmetricKey,
    const std::string_view& iv,
    const std::string_view& authenticationTag)
    : mOffset(0),
      mPlaintext(std::string(plaintextLength, 'X')),
      mCipherContext(createCipherContext())
{
    Expect3(symmetricKey.size() == AesGcm<KeyLengthBits>::KeyLength,
            "invalid key length, actual: " + std::to_string(symmetricKey.size())
            + ", expected: " + std::to_string(AesGcm<KeyLengthBits>::KeyLength),
            AesGcmException);
    Expect3(iv.size() == AesGcm<KeyLengthBits>::IvLength, "invalid IV length", AesGcmException);
    Expect3(authenticationTag.size() == AesGcm<KeyLengthBits>::AuthenticationTagLength,
            "authentication tag has invalid length", AesGcmException);

    int status = EVP_DecryptInit_ex(
        mCipherContext.get(),
        encryptionCipher<KeyLengthBits>(),
        nullptr,
        reinterpret_cast<const unsigned char*>(static_cast<const char*>(symmetricKey)),
        reinterpret_cast<const unsigned char*>(iv.data()));
    Expect3(status==1, "can't initialize AES-GCM encryption", AesGcmException);

    auto tag = std::string(authenticationTag); // For some reason the tag is only accepted via non-const pointer.
                                               // So create a temporary copy which openSSL can safely modify.
    status = EVP_CIPHER_CTX_ctrl(
        mCipherContext.get(),
        EVP_CTRL_GCM_SET_TAG,
        static_cast<int>(tag.size()),
        tag.data());
    Expect3(status==1, "can't set authentication tag", AesGcmException);
}


template <size_t KeyLengthBits>
void Decryptor<KeyLengthBits>::decrypt (const std::string_view& data)
{
    Expect3(gsl::narrow<size_t>(mOffset) + data.size() <= mPlaintext.size(), "input data is too long",
            AesGcmException);

    const int status = EVP_DecryptUpdate(
        mCipherContext.get(),
        reinterpret_cast<unsigned char*>(static_cast<char*>(mPlaintext)),
        &mOffset,
        reinterpret_cast<const unsigned char*>(data.data()),
        static_cast<int>(data.size()));

    Expect3(status==1, "can't decrypt data", AesGcmException);
    // Verify that openSSL obeyed our parameters and did not write past of the plaintext string.
    Expect3(mOffset >= 0, "decryption failed", AesGcmException);
    Expect3(static_cast<size_t>(mOffset) <= mPlaintext.size(), "openSSl wrote too many bytes", AesGcmException);
}


template <std::size_t KeyLengthBits>
SafeString Decryptor<KeyLengthBits>::finalize (void)
{
    int count = 0;
    const int status = EVP_DecryptFinal_ex(
        mCipherContext.get(),
        reinterpret_cast<unsigned char*>(static_cast<char*>(mPlaintext)) + mOffset,
        &count);
    Expect3(status==1, "can't finalize AES-GCM decryption", AesGcmException);
    mOffset += count;
    Expect3(static_cast<size_t>(mOffset) == mPlaintext.size(), "wrote too many bytes on decryption", AesGcmException);

    return std::move(mPlaintext);
}


template <size_t KeyLengthBits>
EncryptionResult AesGcm<KeyLengthBits>::encrypt(const std::string_view& plaintext, const SafeString& key,
                                                const std::string_view& iv)
{
    Encryptor<KeyLengthBits> encryptor(plaintext.size(), key, iv);
    encryptor.encrypt(plaintext);
    return encryptor.finalize();
}


template <std::size_t KeyLengthBits>
SafeString AesGcm<KeyLengthBits>::decrypt(const std::string_view& ciphertext, const SafeString& key,
                                          const std::string_view& iv, const std::string_view& authenticationTag)
{
    Decryptor<KeyLengthBits> decryptor(ciphertext.size(), key, iv, authenticationTag);
    decryptor.decrypt(ciphertext);
    return decryptor.finalize();
}

template class AesGcm<128>;
template class AesGcm<256>;
