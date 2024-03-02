/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_UTIL_GSL_HXX
#define E_LIBRARY_UTIL_GSL_HXX

// let gsl_assert throw exceptions in case of failed assert
// (This must match the configuration in our Conanfile, but since iOS does not use Conan, we define
// it here as well)
#ifndef gsl_CONFIG_CONTRACT_VIOLATION_THROWS
#define gsl_CONFIG_CONTRACT_VIOLATION_THROWS
#endif

#include <gsl/gsl-lite.hpp>

#endif
