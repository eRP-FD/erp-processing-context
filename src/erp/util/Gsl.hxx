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
