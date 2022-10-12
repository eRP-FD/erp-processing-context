/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "Utf8Helper.hxx"

namespace fhirtools
{
size_t Utf8Helper::utf8Length(const std::string_view& s)
{
    size_t codePoints = 0;
    for (char ch : s)
    {
        // Count everything except 10xx'xxxx bytes, which are just continuations of previously
        // started UTF-8 sequences.
        // Ref: https://en.wikipedia.org/wiki/UTF-8#Description
        if ((static_cast<std::uint8_t>(ch) & 0b1100'0000) != 0b1000'0000)
            codePoints++;
    }
    return codePoints;
}
std::string Utf8Helper::truncateUtf8(const std::string& s, std::size_t maxLength)
{
    // This could be optimized by taking into account that one UTF-8 code point never requires more
    // than 4 bytes, but for the tiny strings that we handle, the extra code branches are probably
    // not worth it.

    size_t codePoints = 0;
    for (auto it = s.begin(), end = s.end(); it != end; ++it)
    {
        // Count everything except 10xx'xxxx bytes, which are just continuations of previously
        // started UTF-8 sequences.
        // Ref: https://en.wikipedia.org/wiki/UTF-8#Description
        if ((static_cast<std::uint8_t>(*it) & 0b1100'0000) != 0b1000'0000)
        {
            if (codePoints >= maxLength)
            {
                // We have enough code points _and_ we skipped over all continuation bytes -> done.
                return std::string{s.begin(), it};
            }
            codePoints++;
        }
    }

    // If we got here, the string didn't need truncation in the first place.
    return s;
}
}
