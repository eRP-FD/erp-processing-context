/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/crypto/Pbkdf2Hmac.hxx"
#include "erp/util/Expect.hxx"

#include <openssl/evp.h>


std::vector<uint8_t> Pbkdf2Hmac::deriveKey(std::string_view input, const SafeString& salt)
{
    std::vector<uint8_t> key(32);
    const auto result = PKCS5_PBKDF2_HMAC(input.data(), gsl::narrow<int>(input.size()),
                                          reinterpret_cast<const unsigned char*>(static_cast<const char*>(salt)),
                                          gsl::narrow<int>(salt.size()), numberOfIterations, EVP_sha256(),
                                          gsl::narrow<int>(key.size()), reinterpret_cast<unsigned char*>(&key[0]));
    OpenSslExpect(result == 1, "PKCS5_PBKDF2_HMAC failed");
    return key;
}
