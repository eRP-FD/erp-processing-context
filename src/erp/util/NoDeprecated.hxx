/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef FHIRTOOLS_NO_DEPRECATED_HXX
#define FHIRTOOLS_NO_DEPRECATED_HXX


#if defined(__GNUC__) || defined(__clang__)
#define BEGIN_NO_DEPRECATED_WARNINGS                                                                                   \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")

#define END_NO_DEPRECATED_WARNINGS _Pragma("GCC diagnostic pop")
#elif defined(_MSC_VER)
#define BEGIN_NO_DEPRECATED_WARNINGS _Pragma("warning(push)") _Pragma("warning(disable: 4995)")

#define END_NO_DEPRECATED_WARNINGS _Pragma("warning(pop)")

#endif


#endif// FHIRTOOLS_NO_DEPRECATED_HXX
