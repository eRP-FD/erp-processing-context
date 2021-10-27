/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef E_LIBRARY_COMMON_CONSTANTS_HXX
#define E_LIBRARY_COMMON_CONSTANTS_HXX

#include "erp/util/Buffer.hxx"
#include <cstdio>

class Constants
{
public:
    static constexpr size_t DefaultBufferSize = 8192;

    // Timeout in seconds by http connection
    static constexpr int httpTimeoutInSeconds = 30;
};

#endif
