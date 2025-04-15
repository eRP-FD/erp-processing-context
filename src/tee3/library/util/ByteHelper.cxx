/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/util/ByteHelper.hxx"

#include <stdexcept>

namespace epa
{

std::string ByteHelper::toHex(std::string_view buffer)
{
    return toHex(toBinaryView(buffer));
}

//NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
std::string ByteHelper::toHex(BinaryView buffer)
{
    constexpr const char hexChars[16 + 1] = "0123456789abcdef";

    std::string hex(buffer.size() * 2, '\0');
    char* iterator = hex.data();
    for (const std::uint8_t c : buffer)
    {
        *iterator = hexChars[(c >> 4) & 0xf];
        ++iterator;
        *iterator = hexChars[c & 0xf];
        ++iterator;
    }
    return hex;
}
//NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)


std::string ByteHelper::fromHex(std::string_view hexString)
{
    if ((hexString.size() % 2) == 1)
        throw std::logic_error("hex string should have an even number of characters");
    std::string s(hexString.size() / 2, '\0');
    const char* iterator = hexString.data();
    for (char& ch : s)
    {
        const unsigned char firstDigit = charToNibble(*iterator);
        ++iterator;
        const unsigned char secondDigit = charToNibble(*iterator);
        ++iterator;
        reinterpret_cast<unsigned char&>(ch) =
            static_cast<unsigned char>(firstDigit << 4 | secondDigit);
    }
    return s;
}


BinaryBuffer ByteHelper::binaryBufferFromHex(std::string_view hexString)
{
    return stringToBinaryBuffer(fromHex(hexString));
}


unsigned char ByteHelper::charToNibble(const char value)
{
    switch (value)
    {
        case '0':
            return 0;
        case '1':
            return 1;
        case '2':
            return 2;
        case '3':
            return 3;
        case '4':
            return 4;
        case '5':
            return 5;
        case '6':
            return 6;
        case '7':
            return 7;
        case '8':
            return 8;
        case '9':
            return 9;

        case 'a':
        case 'A':
            return 10;

        case 'b':
        case 'B':
            return 11;

        case 'c':
        case 'C':
            return 12;

        case 'd':
        case 'D':
            return 13;

        case 'e':
        case 'E':
            return 14;

        case 'f':
        case 'F':
            return 15;

        default:
            throw std::runtime_error("invalid nibble value");
    }
}

} // namespace epa
