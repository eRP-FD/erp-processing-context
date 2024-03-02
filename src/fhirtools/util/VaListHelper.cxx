/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/util/VaListHelper.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/util/Gsl.hxx"

namespace fhirtools
{
std::string VaListHelper::vaListToString(const char* msg, va_list args)
{
    FPExpect(msg != nullptr, "message is missing");
    std::string message(256, '\0');
    ssize_t len = vsnprintf(message.data(), message.size(), msg, args);
    Expect3(len >= 0, "Failed call to vsnprintf.", std::logic_error);
    auto size = gsl::narrow<size_t>(len);
    if (size >= message.size())
    {
        message.resize(size + 1, '\0');
        len = vsnprintf(message.data(), message.size(), msg, args);
        Expect3(len == static_cast<ssize_t>(size), "message length changed.", std::logic_error);
        message.resize(size);
    }
    else
    {
        message.resize(size);
    }
    return message;
}
}
