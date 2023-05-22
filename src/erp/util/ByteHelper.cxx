/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/util/ByteHelper.hxx"
#include "erp/util/Expect.hxx"

#include <stdexcept>


std::string ByteHelper::toHex (gsl::span<const char> buffer)
{
    std::string hex;
    hex.reserve(buffer.size() * 2);
    for (const char ch : buffer)
    {
        const auto c = static_cast<unsigned char>(ch);
        hex.push_back(nibbleToChar(c >> 4));
        hex.push_back(nibbleToChar(c & 0xf));
    }
    return hex;
}

std::string ByteHelper::toHex(const util::Buffer& buffer)
{
    return toHex(gsl::span(reinterpret_cast<const char*>(buffer.data()), buffer.size()));
}


std::string ByteHelper::fromHex (const std::string_view hexString)
{
    if ((hexString.size()%2) == 1)
    {
        Fail2("hex string should have an even number of characters", std::runtime_error);
    }
    std::string s;
    s.reserve(hexString.size()/2);
    for (std::size_t index=0; index<hexString.size(); index+=2)
        s.push_back(char(charToNibble(hexString[index])<<4 | charToNibble(hexString[index+1])));
    return s;
}


char ByteHelper::nibbleToChar (const int value)
{
    switch (value)
    {
        case  0: return '0';
        case  1: return '1';
        case  2: return '2';
        case  3: return '3';
        case  4: return '4';
        case  5: return '5';
        case  6: return '6';
        case  7: return '7';
        case  8: return '8';
        case  9: return '9';
        case 10: return 'a';
        case 11: return 'b';
        case 12: return 'c';
        case 13: return 'd';
        case 14: return 'e';
        case 15: return 'f';

        default:
            Fail2("invalid nibble value", std::runtime_error);
    }
}


int ByteHelper::charToNibble (const char value)
{
    switch (value)
    {
        case '0': return 0;
        case '1': return 1;
        case '2': return 2;
        case '3': return 3;
        case '4': return 4;
        case '5': return 5;
        case '6': return 6;
        case '7': return 7;
        case '8': return 8;
        case '9': return 9;

        case 'a':
        case 'A': return 10;

        case 'b':
        case 'B': return 11;

        case 'c':
        case 'C': return 12;

        case 'd':
        case 'D': return 13;

        case 'e':
        case 'E': return 14;

        case 'f':
        case 'F': return 15;

        default:
            Fail2("invalid nibble value", std::runtime_error);
    }
}
