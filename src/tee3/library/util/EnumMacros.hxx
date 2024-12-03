/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_ENUMMACROS_HXX
#define EPA_LIBRARY_UTIL_ENUMMACROS_HXX

/**
 * Defines an enum class, and an additional variable "raw<ENUM>Names" that can be used to initialize
 * an EnumTable for enum<->string conversions.
 */
#define EPA_ENUM_CLASS(ENUM, ...)                                                                  \
    [[maybe_unused]] constexpr const char raw##ENUM##Names[] = #__VA_ARGS__;                       \
    enum class ENUM                                                                                \
    {                                                                                              \
        __VA_ARGS__                                                                                \
    }


/**
 * Defines an enum class inside another class, and an additional static variable "raw<ENUM>Names"
 * that can be used to initialize an EnumTable for enum<->string conversions.
 */
#define EPA_NESTED_ENUM_CLASS(ENUM, ...)                                                           \
    static constexpr const char raw##ENUM##Names[] = #__VA_ARGS__;                                 \
    enum class ENUM                                                                                \
    {                                                                                              \
        __VA_ARGS__                                                                                \
    }

/**
 * Defines a nested "enum Value" inside an enum wrapper class, and an additional variable "rawNames"
 * that will be used by EnumBase::getTable() for enum<->string conversions.
 */
#define EPA_VALUES_ENUM(...)                                                                       \
    static constexpr const char rawNames[] = #__VA_ARGS__;                                         \
    enum Value                                                                                     \
    {                                                                                              \
        __VA_ARGS__                                                                                \
    }


#endif
