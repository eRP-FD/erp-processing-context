/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_BYTEHELPER_HXX
#define EPA_LIBRARY_UTIL_BYTEHELPER_HXX

#include "library/util/BinaryBuffer.hxx"

#include <boost/endian/conversion.hpp>
#include <string>
#include <string_view>

#include "fhirtools/util/Gsl.hxx"

namespace epa
{

class ByteHelper
{
public:
    static std::string toHex(std::string_view buffer);
    static std::string toHex(BinaryView buffer);
    static std::string fromHex(std::string_view hexString);
    static BinaryBuffer binaryBufferFromHex(std::string_view hexString);

    /**
     * Convert the given integral `value` and write it as big-endian
     * (most significant) bytes first to `buffer`.
     * I.e. the hex number 0x12345 will be written as bytes 0x01, 0x23, 0x45.
     */
    template<typename T>
    static void intToBigEndianBytes(const T value, void* buffer);
    /**
     * Convert the given integral `value` and write it as big-endian
     * (most significant) bytes first to `buffer`.
     * I.e. the hex number 0x12345 will be written as bytes 0x01, 0x23, 0x45.
     * The resulting byte representation is returned as unencoded (i.e. no base64 etc.)
     * std::string object.
     */
    template<typename T>
    static std::string intToBigEndianBytes(const T value);

    /**
     * Convert the given big-endian stream of bytes to an integer value.
     * I.e. the bytes 0x01, 0x23, 0x45 will be converted to integer 0x12345.
     */
    template<typename T>
    static T intFromBigEndianBytes(const void* buffer);
    /**
     * Convert the given big-endian stream of bytes to an integer value.
     * I.e. the bytes 0x01, 0x23, 0x45 will be converted to integer 0x12345.
     */
    template<typename T>
    static T intFromBigEndianBytes(const std::string& data);

private:
    static unsigned char charToNibble(char value);
};


template<typename T>
void ByteHelper::intToBigEndianBytes(const T value, void* buffer)
{
    boost::endian::endian_store<T, sizeof(T), boost::endian::order::big>(
        reinterpret_cast<unsigned char*>(buffer), value);
}


template<typename T>
std::string ByteHelper::intToBigEndianBytes(const T value)
{
    std::string s(sizeof(value), ' ');
    intToBigEndianBytes(value, s.data());
    return s;
}


template<typename T>
T ByteHelper::intFromBigEndianBytes(const void* buffer)
{
    return boost::endian::endian_load<T, sizeof(T), boost::endian::order::big>(
        reinterpret_cast<const unsigned char*>(buffer));
}


template<typename T>
T ByteHelper::intFromBigEndianBytes(const std::string& data)
{
    Expects(data.size() == sizeof(T));
    return intFromBigEndianBytes<T>(data.data());
}

} // namespace epa

#endif
