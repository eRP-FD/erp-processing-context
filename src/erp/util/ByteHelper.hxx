/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_UTIL_BYTEHELPER_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_BYTEHELPER_HXX

#include "fhirtools/util/Gsl.hxx"
#include "erp/util/Buffer.hxx"

#include <cstdint>
#include <string>
#include <type_traits>


class ByteHelper
{
public:
    static std::string toHex (gsl::span<const char> buffer);
    static std::string toHex (const util::Buffer& buffer);
    static std::string fromHex (std::string_view hexString);

    static char nibbleToChar (int value);
    static int charToNibble (char value);
};

#endif
