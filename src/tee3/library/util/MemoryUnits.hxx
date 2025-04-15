/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_MEMORYUNITS_HXX
#define EPA_LIBRARY_UTIL_MEMORYUNITS_HXX

#include <cstddef>
#include <string>

#include "fhirtools/util/Gsl.hxx"

namespace epa
{

constexpr std::size_t operator""_B(unsigned long long int count)
{
    return gsl::narrow_cast<std::size_t>(count);
}


/** Helper to convert from kibibytes to bytes. */
constexpr std::size_t operator""_KiB(unsigned long long int count)
{
    return gsl::narrow_cast<std::size_t>(count * 1024_B);
}


/** Helper to convert from mebibytes to bytes. */
constexpr std::size_t operator""_MiB(unsigned long long int count)
{
    return gsl::narrow_cast<std::size_t>(count * 1024_KiB);
}


class MemoryUnits
{
public:
    /**
     * Helper to convert a memory measurement to a string such as "0.12 MB".
     *
     * Unlike the KiB/MiB helpers above, this method returns base-1000 units because they are more
     * common in our existing log outputs.
     */
    static std::string sizeToString(std::size_t bytes);
};

} // namespace epa

#endif
