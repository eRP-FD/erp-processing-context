/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_UTIL_HASH_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_HASH_HXX

#include "erp/util/Buffer.hxx"
#include "fhirtools/util/Gsl.hxx"

#include <string>
#include <string_view>
#include <vector>


class Hash
{
public:
    static std::vector<char> sha1 (gsl::span<const char> source);
    static std::vector<char> sha256 (gsl::span<const char> source);

    /**
     * Return the binary SHA1 hash of the given data.
     */
    static util::Buffer sha1 (const util::Buffer& data);

    /**
     * Return the binary SHA 256 hash of the given data.
     * Note that the returned string still contains binary data, not a hex or base64 representation.
     */
    static std::string sha256 (const std::string& data);

    /**
     * Return the binary SHA 256 hash of the given data.
     */
    static util::Buffer sha256 (const util::Buffer& data);

    /**
     * Return the binary Hmac Cash based on SHA 256 algorithm.
     *
     * @param key - the key to use for Hmac signature
     * @param data - the data to sign
     * @return Hmac Cash based on SHA 256 algorithm
     */
    static util::Buffer hmacSha256(const util::Buffer& key, std::string_view data);
    static util::Buffer hmacSha256(std::string_view key, std::string_view data);
};


#endif
