/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/util/Hash.hxx"

#include "erp/util/Base64.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/OpenSsl.hxx"


std::vector<char> Hash::sha1 (gsl::span<const char> source)
{
    std::vector<char> result(SHA_DIGEST_LENGTH);
    SHA1(reinterpret_cast<const unsigned char*>(source.data()), source.size_bytes(),
         reinterpret_cast<unsigned char*>(result.data()));
    return result;
}


util::Buffer Hash::sha1 (const util::Buffer& data)
{
    util::Buffer result(SHA_DIGEST_LENGTH);
    SHA1(util::bufferToRaw(data), data.size(), util::bufferToRaw(result));

    return result;
}


std::vector<char> Hash::sha256 (gsl::span<const char> source)
{
    std::vector<char> result(SHA256_DIGEST_LENGTH);
    SHA256(reinterpret_cast<const unsigned char*>(source.data()), source.size_bytes(),
           reinterpret_cast<unsigned char*>(result.data()));
    return result;
}


std::string Hash::sha256 (const std::string& data)
{
    const auto hash = sha256(gsl::span<const char>(data));
    return {hash.data(), hash.size()};
}


util::Buffer Hash::sha256 (const util::Buffer& data)
{
    util::Buffer result(SHA256_DIGEST_LENGTH);
    SHA256(util::bufferToRaw(data), data.size(), util::bufferToRaw(result));

    return result;
}

util::Buffer Hash::hmacSha256(const util::Buffer& key, std::string_view data)
{
    auto dataView = std::string_view{reinterpret_cast<const char*>(key.data()), key.size()};
    return hmacSha256(dataView, data);
}

util::Buffer Hash::hmacSha256(std::string_view key, std::string_view data)
{
    util::Buffer result(SHA256_DIGEST_LENGTH);
    unsigned int resultSize = 0;

    unsigned char* hmacResult = HMAC(EVP_sha256(),
                                     key.data(), gsl::narrow_cast<int>(key.size()),
                                     reinterpret_cast<const unsigned char*>(data.data()), data.size(),
                                     reinterpret_cast<unsigned char*>(result.data()), &resultSize);
    OpenSslExpect(hmacResult == reinterpret_cast<const unsigned char*>(result.data()), "HMAC() failed");
    OpenSslExpect(resultSize == SHA256_DIGEST_LENGTH, "Unexpected digest length");

    return result;
}
