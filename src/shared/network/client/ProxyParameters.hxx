/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include <cstdint>
#include <string>

enum class ProxyMode
{
    HTTP,
    SNI,
};

struct ProxyParameters {
    std::string ip;
    std::uint_least16_t port;
    ProxyMode mode;
    static ProxyParameters fromUrl(std::string_view proxyUrl, ProxyMode mode);
};
