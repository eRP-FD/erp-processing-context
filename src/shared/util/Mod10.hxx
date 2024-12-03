/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_UTIL_MOD10_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_MOD10_HXX

#include <string_view>

namespace checksum
{

/**
 * Implementation of the Luhn/mod10 checksum. It expects a strictly numeric
 * string and returns the checksum as an ascii character.
 * @param str Checksum to be calculated for. Is expected to be a numeric string
 * @tparam evenWeight weight for even indices of the strings, counted from the right, so last char, last-2, ..  char, etc
 * @tparam oddWeight weight for odd indices of the string, counted from the right
 * @tparam wrapDigits wrap the result of after weighting each digit
 */
template<size_t evenWeight = 1, size_t oddWeight = 2, bool wrapDigits = true>
char mod10(std::string_view str)
{
    size_t sum = 0;
    // since the indices are counted from the right, determine an even/odd parity flag
    const auto parity = str.length() % 2;
    for (size_t i = 0; i < str.length(); ++i)
    {
        auto weight = ((parity + i) % 2) == 0 ? evenWeight : oddWeight;
        auto val = static_cast<size_t>(str[i] - '0') * weight;
        if (wrapDigits)
            sum += (val > 9 ? (val - 9) : val);
        else
            sum += val;
    }
    return static_cast<char>(sum % 10 + '0');
}

} // namespace checksum

#endif
