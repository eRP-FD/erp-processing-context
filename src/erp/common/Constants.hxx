/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_COMMON_CONSTANTS_HXX
#define E_LIBRARY_COMMON_CONSTANTS_HXX

#include "erp/util/Buffer.hxx"
#include <cstdio>

#include <chrono>

class Constants
{
public:
    static constexpr size_t DefaultBufferSize = 8192;

    // Timeout in seconds by http connection
    static constexpr int httpTimeoutInSeconds = 30;
    static constexpr auto resolveTimeout = std::chrono::milliseconds{2000};
};

#endif
