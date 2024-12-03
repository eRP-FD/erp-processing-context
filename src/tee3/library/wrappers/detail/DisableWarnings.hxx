/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_WRAPPERS_DETAIL_DISABLEWARNINGS_HXX
#define EPA_LIBRARY_WRAPPERS_DETAIL_DISABLEWARNINGS_HXX

#include <boost/predef/compiler.h>

#if BOOST_COMP_CLANG
#define DISABLE_WARNINGS_BEGIN                                                                     \
    _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Weverything\"")
#define DISABLE_WARNINGS_END _Pragma("clang diagnostic pop")
#elif BOOST_COMP_GNUC
#if BOOST_COMP_GNUC >= BOOST_VERSION_NUMBER(8, 0, 0)
#define DISABLE_WARNING_GCC8_CLASS_MEMACCESS _Pragma("GCC diagnostic ignored \"-Wclass-memaccess\"")
#else
#define DISABLE_WARNING_GCC8_CLASS_MEMACCESS
#endif

#define DISABLE_WARNINGS_BEGIN                                                                     \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wformat\"")                  \
        _Pragma("GCC diagnostic ignored \"-Wmisleading-indentation\"")                             \
            DISABLE_WARNING_GCC8_CLASS_MEMACCESS
#define DISABLE_WARNINGS_END _Pragma("GCC diagnostic pop")
#else
#define DISABLE_WARNINGS_BEGIN
#define DISABLE_WARNINGS_END
#endif

#endif
