/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CRYPTO_AESGCM128_HXX
#define ERP_PROCESSING_CONTEXT_CRYPTO_AESGCM128_HXX

#include "erp/util/SafeString.hxx"

#include <string>
#include <openssl/ossl_typ.h>
#include <memory>


class AesGcmException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};


struct EncryptionResult
{
    std::string ciphertext;
    std::string authenticationTag;
};

/**
 * Collection of Encryptor and Decryptor classes, some constants and convenience functions encrypt() and decrypt()
 * for the AES GCM algorithm with 128/256 bit keys.
 */
template <size_t KeyLengthBits>
class AesGcm
{
public:
    // Some frequently used lengths for AES-GCM 128
    static constexpr std::size_t KeyLength = KeyLengthBits / 8;     // 128 bits = 16 bytes, 256 bits = 32 bytes
    static constexpr std::size_t IvLength = 96 / 8;                 //  96 bits = 12 bytes, not dependent on key length
    static constexpr std::size_t AuthenticationTagLength = 128 / 8; // 128 bits = 16 bytes, not dependent on key length



    static EncryptionResult encrypt (
        const std::string_view& plaintext,
        const SafeString& key,
        const std::string_view& iv);

    static SafeString decrypt (
        const std::string_view& ciphertext,
        const SafeString& key,
        const std::string_view& iv,
        const std::string_view& authenticationTag);
};

extern template class AesGcm<128>;
extern template class AesGcm<256>;

using AesGcm128 = AesGcm<128>;
using AesGcm256 = AesGcm<256>;


#endif
