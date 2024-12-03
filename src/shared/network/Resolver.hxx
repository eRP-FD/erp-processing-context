/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_UTIL_RESOLVER_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_RESOLVER_HXX

#include <boost/asio/ip/tcp.hpp>
#include <chrono>
#include <string>

/**
 * Resolver class that supports asynchronously resolving addresses, allowing
 * specifying a user-defined timeout. Requires getaddrinfo_a() availability
 */
class Resolver
{
public:
    static boost::asio::ip::basic_resolver_results<boost::asio::ip::tcp>
    resolve(const std::string& host, const std::string& service, std::chrono::milliseconds timeout);
};

#endif
