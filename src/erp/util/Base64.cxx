#include "erp/util/Base64.hxx"

#include "erp/util/Expect.hxx"

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <stdexcept>


namespace
{
    const char* base64Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


    template<std::uint8_t BinaryAlphabetSizeN = std::numeric_limits<std::uint8_t>::max(),
        std::uint8_t Base64AlphabetSizeN = 64>
    constexpr std::array<std::int8_t, BinaryAlphabetSizeN> getBinaryAlphabet ()
    {
        static_assert(BinaryAlphabetSizeN >= Base64AlphabetSizeN);

        std::array<std::int8_t, BinaryAlphabetSizeN> binaryAlphabetResult{};

        // Set every element in the lookup table to -1 by default - we can use this magic number to
        // detect invalid Base64 during parsing.
        binaryAlphabetResult.fill(-1);

        for (std::uint8_t itr = 0; itr < Base64AlphabetSizeN; ++itr)
        {
            binaryAlphabetResult[base64Alphabet[itr]] = itr;
        }

        // Map -_ to the same values as +/ for base64url encoding (RFC 4648)
        // See also https://en.wikipedia.org/wiki/Base64#URL_applications
        binaryAlphabetResult['-'] = binaryAlphabetResult['+'];
        binaryAlphabetResult['_'] = binaryAlphabetResult['/'];

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
    for (std::uint8_t byte : data)
    {
        value = (value << 8) + byte;
        valueShift = valueShift + 8;

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


std::string Base64::encode (const util::Buffer& data)
{
    return Base64::encode(
        std::string_view(reinterpret_cast<const char*>(data.data()), data.size()));
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


util::Buffer Base64::decode (const std::string_view& base64)
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

        std::int8_t binaryDigit = binaryAlphabet[static_cast<std::uint8_t>(itr)];
        if (-1 == binaryDigit)
        {
            Fail2("Invalid Base64 string: " + std::string(base64), std::invalid_argument);
        }

        value = (value << 6) + binaryDigit;
        valueShift = valueShift + 6;

        if (valueShift >= 0)
        {
            *p++ = static_cast<std::uint8_t>((value >> valueShift) & 0xFF);

            valueShift = valueShift - 8;
        }
    }

    const size_t decodedSize = std::distance(decodedData.get(), p);
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
        const char c = *readIterator++;
        if (alphabet[c] >= 0)
            *writeIterator++ = c;
    }

    return data.substr(0, std::distance(data.begin(), writeIterator));
}
