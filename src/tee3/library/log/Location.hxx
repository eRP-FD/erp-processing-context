/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_LOG_LOCATION_HXX
#define EPA_LIBRARY_LOG_LOCATION_HXX

#include <source_location>
#include <string>

namespace epa
{
/**
 * A simple replacement for C++20's `source_location`.
 */
class Location
{
public:
    const char* file() const;
    int line() const;

    /**
     * Return <filename-with-path>:<line-number>.
     */
    std::string toString() const;
    /**
     * Like `toString` but without path of the file name.
     * Returns \<filename>:\<line-number>.
     */
    std::string toShortString() const;

    /**
     * Calculate and return the first 31 bits of the SHA-256 hash of toShortString(). Note that
     * scripts/decode_program_location.pl can find the program location to such a code.
     * The returned value is never negative.
     */
    int32_t calculateProgramLocationCode() const;

protected:
    /**
     * The pointer must be valid for the lifetime of the Location object (and all its copies).
     */
    Location(const char* file, int line);

private:
    const char* mFile;
    /**
     * Store as int rather than size_t because the value is supplied as int and it is used
     * as int.
     */
    int mLine;
};


/**
 * A Location class that makes the constructor public. Only to be used by the here() macro and by
 * tests. This way we want to make sure to prevent lifetime errors with non-static string pointers.
 */
class LocationFromValidRemainingPointer : public Location
{
public:
    LocationFromValidRemainingPointer(const char* file, int line)
      : Location(file, line)
    {
    }
};

inline LocationFromValidRemainingPointer here(
    const std::source_location& loc = std::source_location::current())
{
    return LocationFromValidRemainingPointer(loc.file_name(), static_cast<int>(loc.line()));
}


} // namespace epa
#endif
