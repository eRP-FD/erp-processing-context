/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/Demangle.hxx"

#include <cxxabi.h>
#include <memory>

namespace util
{

std::string demangle(const std::string& mangledName)
{
    int status{};
    std::unique_ptr<char, decltype(&free)> demangled(nullptr, &free);
    size_t outLen{};
    demangled.reset(abi::__cxa_demangle(mangledName.c_str(), nullptr, &outLen, &status));
    if (status != 0)
    {
        return mangledName;
    }
    return std::string{demangled.get()};
}

} // namespace util
