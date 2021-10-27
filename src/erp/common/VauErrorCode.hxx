/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_VAUERRORCODE_HXX
#define ERP_PROCESSING_CONTEXT_VAUERRORCODE_HXX

#include <magic_enum.hpp>

// Values for the Http-Header field VAU-Error-Code
enum class VauErrorCode
{
    brute_force,
    invalid_prescription
};

constexpr std::string_view vauErrorCodeStr(VauErrorCode vauErrorCode)
{
    return magic_enum::enum_name(vauErrorCode);
}

#endif//ERP_PROCESSING_CONTEXT_VAUERRORCODE_HXX
