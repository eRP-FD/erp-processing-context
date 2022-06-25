/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/Buffer.hxx"


namespace util
{
    Buffer stringToBuffer(std::string_view string)
    {
        const auto* begin = reinterpret_cast<const Buffer::value_type *>(string.data());
        const auto* end = begin + string.length();
        return Buffer(begin, end);
    }


    std::string bufferToString(const Buffer& buffer)
    {
        return std::string(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    }


    Buffer rawToBuffer(const std::uint8_t* raw, std::size_t size)
    {
        return Buffer(raw, raw + size);
    }


    const std::uint8_t* bufferToRaw(const Buffer& buffer)
    {
        return buffer.data();
    }


    std::uint8_t* bufferToRaw(Buffer& buffer)
    {
        return buffer.data();
    }
}
