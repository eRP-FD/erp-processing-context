/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_UTIL_REQUIREMENT_HXX
#define E_LIBRARY_UTIL_REQUIREMENT_HXX

/**
 * This implementation is intended to provide IDE navigation.
 * It is not supposed to generate code.
 */
class Requirement
{
public:
    enum State
    {
        Partial,
        Complete
    };

    /**
     * Define requirements as static variables in erp/Requirements.hxx.
     */
    constexpr explicit Requirement (const char* title, State state = Partial);

    /**
     * Mark the start of (a part of) a gematik requirement.
     */
    constexpr bool start (const char* description);

    /**
     * Mark the start of (a part of) a gematik requirement that is currently disabled.
     */
    constexpr void startDisabled (const char* description);

    /**
     * Mark the end of (a part of) a gematik requirement.
     */
    constexpr bool finish (void);

    /**
     * Mark a test a gematik requirement.
     */
    constexpr void test (const char* description);
};


constexpr Requirement::Requirement (const char*, State)
{
}


constexpr bool Requirement::start (const char*)
{
    return true;
}


constexpr void Requirement::startDisabled (const char*)
{
}


constexpr bool Requirement::finish (void)
{
    return false;
}


constexpr void Requirement::test (const char*)
{
}

#endif
