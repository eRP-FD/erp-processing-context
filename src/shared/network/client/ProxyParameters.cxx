/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "shared/network/client/ProxyParameters.hxx"
#include "shared/util/UrlHelper.hxx"

#include <gsl/gsl-lite.hpp>

ProxyParameters ProxyParameters::fromUrl(std::string_view proxyUrl, ProxyMode mode)
{
    const auto parsedProxyUrl = UrlHelper::parseUrl(std::string{proxyUrl});
    return {.ip = parsedProxyUrl.mHost, .port = gsl::narrow<uint_least16_t>(parsedProxyUrl.mPort), .mode = mode};
}