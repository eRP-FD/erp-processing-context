// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#ifndef FHIR_TOOLS_SEVERITY_HXX
#define FHIR_TOOLS_SEVERITY_HXX

#include <compare>
#include <type_traits>

namespace fhirtools
{
enum class Severity
{
    error = 4,
    unslicedWarning = 3,
    warning = 2,
    info = 1,
    debug = 0,
    MIN = debug,
    MAX = error,
};

inline auto operator <=> (Severity lhs, Severity rhs)
{
    using UT = std::underlying_type_t<Severity>;
    return static_cast<UT>(lhs) <=> static_cast<UT>(rhs);
}

}

#endif// FHIR_TOOLS_SEVERITY_HXX
