/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_UTIL_REQUIREMENT_HXX
#define E_LIBRARY_UTIL_REQUIREMENT_HXX
// NOLINTBEGIN(readability-convert-member-functions-to-static)

/**
 * This implementation is intended to provide IDE navigation.
 * It is not supposed to generate code.
 */
class Requirement
{
public:
    /**
     * Define requirements as static variables in erp/Requirements.hxx.
     */
    constexpr explicit Requirement (const char* title);

    /**
     * Mark the start of (a part of) a gematik requirement.
     */
    constexpr void start(const char* description) const;

    constexpr bool implements(const char* description) const;

    /**
     * Mark the end of (a part of) a gematik requirement.
     */
    constexpr void finish () const;

    /**
     * Mark a test a gematik requirement.
     */
    constexpr void test (const char* description) const;
};


constexpr Requirement::Requirement (const char*)
{
}


constexpr void Requirement::start(const char*) const
{
}

constexpr bool Requirement::implements(const char*) const
{
    return true;
}

constexpr void Requirement::finish () const
{
}


constexpr void Requirement::test (const char*) const
{
}

// NOLINTEND(readability-convert-member-functions-to-static)
#endif
