/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_UTIL_DEMANGLE_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_DEMANGLE_HXX

#include <string>
#include <string_view>

namespace util
{

std::string demangle(const std::string& mangledName);

} // namespace util

#endif