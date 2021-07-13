#ifndef ERP_PROCESSING_CONTEXT_UTIL_BYTEHELPER_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_BYTEHELPER_HXX

#include "erp/util/Gsl.hxx"

#include <cstdint>
#include <string>
#include <type_traits>


class ByteHelper
{
public:
    static std::string toHex (gsl::span<const char> buffer);
    static std::string fromHex (std::string_view hexString);

    static char nibbleToChar (int value);
    static int charToNibble (char value);
};

#endif
