/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_UTIL_BASE64_HXX
#define E_LIBRARY_UTIL_BASE64_HXX

#include "shared/util/Buffer.hxx"

#include <ranges>
#include <string>


/**
 * This class accepts Base64, either in its classic form using the + and / special characters, or in
 * the "base64url" variant (RFC 4648), where - and _ are used instead.
 * It always outputs Base64 with the correct padding, but does not require padding when parsing.
 *
 * "Be conservative in what you send, be liberal in what you accept"
 */
class Base64
{
public:
    /**
     * Encodes a binary buffer into a Base64 string.
     */
    static std::string encode(std::string_view data);

    template<std::ranges::contiguous_range RangeT>
    static std::string encode(const RangeT& data);

    // for c-string constants it is unclear if the trailing \0 needs to be encoded, therfore we delete this
    // consider using `string_view_literals`
    template<size_t n>
    static std::string encode(const char (&)[n]) = delete;

    /**
     * Given a Base64 string, it returns the base64url version
     */
    static std::string toBase64Url(const std::string& encodedData);

    /**
     * Decodes a Base64 string into a binary buffer.
     */
    static util::Buffer decode(const std::string_view& base64, bool skipWhiteSpace = false);

    template<std::ranges::contiguous_range RangeT>
    static util::Buffer decode(const RangeT& base64, bool skipWhiteSpace = false);

    static std::string decodeToString(const std::string_view& base64, bool skipWhiteSpace = false);

    /**
     * Return the size of a document that it will have after being Base64 encoded.
     */
    static size_t adjustSizeForEncoding (size_t sizeBeforeEncoding);
    /**
     * Return the size of a document that it will have after being Base64 decoded.
     * @param dataSize All characters a-z, A-Z, 0-9, + and / in a base64 encoded message.
     * @oaram paddinSize Only the = padding characters at the end of a message.
     */
    static size_t adjustSizeForDecoding (size_t dataSize, size_t paddingSize);

    /**
     * According to https://en.wikipedia.org/wiki/Base64 there are many different versions of Base64 encoding.
     * Some, like RFC 4648 do not allow spaces and newlines, others do. In reality, white space is present in many
     * applications, like PEM certificates.
     * This function removes any characters (not only white space) that does not conform to the base64 characters
     * and returns a string that can be passed into one of the decode... functions.
     */
    static std::string cleanupForDecoding (std::string data);
};


template<std::ranges::contiguous_range RangeT>
std::string Base64::encode(const RangeT& data)
{
    static_assert(sizeof(*std::ranges::data(data)) == sizeof(const char));
    return encode(std::string_view{reinterpret_cast<const char*>(std::ranges::data(data)), std::ranges::size(data)});
}

template<std::ranges::contiguous_range RangeT>
util::Buffer Base64::decode(const RangeT& base64, bool skipWhiteSpace)
{
    static_assert(sizeof(*std::ranges::data(base64)) == sizeof(const char));
    return decode(std::string_view{reinterpret_cast<const char*>(std::ranges::data(base64)), std::ranges::size(base64)},
                  skipWhiteSpace);
}

#endif
