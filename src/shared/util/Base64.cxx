/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/util/Gsl.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Expect.hxx"

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <stdexcept>


namespace
{
    const char* base64Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    enum SpecialCharacter : char
    {
        INVALID_CHARACTER = -1,
        XML_WHITESPACE = -2
    };

    template<std::size_t BinaryAlphabetSizeN = std::numeric_limits<std::uint8_t>::max() + 1,
             std::uint8_t Base64AlphabetSizeN = 64>
    constexpr std::array<std::int8_t, BinaryAlphabetSizeN> getBinaryAlphabet()
    {
        static_assert(BinaryAlphabetSizeN >= Base64AlphabetSizeN);

        std::array<std::int8_t, BinaryAlphabetSizeN> binaryAlphabetResult{};

        // Set every element in the lookup table to -1 by default - we can use this magic number to
        // detect invalid Base64 during parsing.
        binaryAlphabetResult.fill(INVALID_CHARACTER);

        for (std::uint8_t itr = 0; itr < Base64AlphabetSizeN; ++itr)
        {
            //NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            binaryAlphabetResult[static_cast<unsigned char>(base64Alphabet[itr])] = static_cast<char>(itr);
        }

        // Map -_ to the same values as +/ for base64url encoding (RFC 4648)
        // See also https://en.wikipedia.org/wiki/Base64#URL_applications
        binaryAlphabetResult['-'] = binaryAlphabetResult['+'];
        binaryAlphabetResult['_'] = binaryAlphabetResult['/'];

        // XML whitespace characters:
        // https://www.w3.org/TR/xml/#sec-common-syn
        binaryAlphabetResult[' '] = XML_WHITESPACE;
        binaryAlphabetResult['\r'] = XML_WHITESPACE;
        binaryAlphabetResult['\t'] = XML_WHITESPACE;
        binaryAlphabetResult['\n'] = XML_WHITESPACE;

        return binaryAlphabetResult;
    }
}


std::string Base64::encode (std::string_view data)
{
    // Every triplet of bytes will be encoded to four bytes of Base64.
    // => The length of Base64-encoded data is ceil(size / 3) * 4.
    const size_t encodedSize = adjustSizeForEncoding(data.size());
    auto encodedData = std::make_unique<char[]>(encodedSize);

    std::int32_t value{};
    std::int8_t valueShift{-6};
    char* p = encodedData.get();

    // Note the implicit cast from char to uint8_t - this is well-defined even when char is signed.
    for (auto c : data)
    {
        auto byte = static_cast<uint8_t>(c);
        value = (value << 8) + byte;
        valueShift = static_cast<std::int8_t>(valueShift + 8);

        while (valueShift >= 0)
        {
            *p++ = base64Alphabet[(value >> valueShift) & 0x3F];
            valueShift -= 6;
        }
    }

    if (valueShift > -6)
    {
        *p++ = base64Alphabet[((value << 8) >> (valueShift + 8)) & 0x3F];
    }

    // pad with '=' until the length becomes a multiple of 4
    //
    for (const char* end = encodedData.get()+encodedSize; p!=end; )
        *p++ = '=';

    return std::string(encodedData.get(), encodedSize);
}

std::string Base64::toBase64Url(const std::string& encodedData)
{
    std::string result = encodedData;
    std::replace(result.begin(), result.end(), '+', '-');
    std::replace(result.begin(), result.end(), '/', '_');
    // remove padding
    while (result.back() == '=' && result.size() > 2)
        result.pop_back();
    return result;
}


util::Buffer Base64::decode(const std::string_view& base64, bool skipWhiteSpace)
{
    static const auto binaryAlphabet = getBinaryAlphabet();

    const size_t decodedMaxSize = base64.length() * 3 / 4 + 3;
    auto decodedData = std::make_unique<char[]>(decodedMaxSize);

    std::int32_t value{};
    std::int8_t valueShift{-8};
    char* p = decodedData.get();
    unsigned paddingBytes = 0;
    for (char itr : base64)
    {
        if (itr == '=')
        {
            paddingBytes += 1;
            if (paddingBytes > 3)
                Fail2("Excessive Base64 padding: " + std::string(base64), std::invalid_argument);
            continue;
        }

        // If we have already started seeing padding, we must not see other characters again.
        if (paddingBytes > 0)
            Fail2("Invalid Base64 padding: " + std::string(base64), std::invalid_argument);

        //NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        std::int8_t binaryDigit = binaryAlphabet[static_cast<std::uint8_t>(itr)];
        if (INVALID_CHARACTER == binaryDigit)
        {
            Fail2("Invalid Base64 string: " + std::string(base64), std::invalid_argument);
        }

        if (XML_WHITESPACE == binaryDigit)
        {
            Expect3(skipWhiteSpace, "Unexpected whitespace in Base64 string: " + std::string{base64},
                    std::invalid_argument);
            continue;
        }

        value = (value << 6) + binaryDigit;
        valueShift = static_cast<std::int8_t>(valueShift + 6);

        if (valueShift >= 0)
        {
            *p++ = static_cast<char>((value >> valueShift) & 0xFF);

            valueShift = static_cast<std::int8_t>(valueShift - 8);
        }
    }

    const size_t decodedSize = gsl::narrow<size_t>(std::distance(decodedData.get(), p));
    return util::rawToBuffer(reinterpret_cast<uint8_t*>(decodedData.get()), decodedSize);
}


std::string Base64::decodeToString (const std::string_view& base64)
{
    auto result = decode(base64);
    return std::string(reinterpret_cast<const char*>(result.data()), result.size());
}


size_t Base64::adjustSizeForEncoding (size_t sizeBeforeEncoding)
{
    return (sizeBeforeEncoding + 2) / 3*4;
}


size_t Base64::adjustSizeForDecoding (const size_t dataSize, const size_t paddingSize)
{
    return (dataSize + paddingSize) * 3/4 - paddingSize;
}


std::string Base64::cleanupForDecoding (std::string data)
{
    const auto alphabet = getBinaryAlphabet();

    auto readIterator = data.begin();
    auto writeIterator = data.begin();
    auto endIterator = data.end();

    while (readIterator != endIterator)
    {
        const auto c = static_cast<uint8_t>(*readIterator++);
        //NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,clang-diagnostic-sign-conversion)
        if (alphabet[c] >= 0)
            *writeIterator++ =  static_cast<char>(c);
    }

    return data.substr(0, gsl::narrow<size_t>(std::distance(data.begin(), writeIterator)));
}
