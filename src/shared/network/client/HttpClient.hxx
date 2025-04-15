/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_HTTPCLIENT_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_HTTPCLIENT_HXX

#include "shared/network/client/ClientBase.hxx"
#include "shared/network/connection/TcpStream.hxx"

#include <boost/asio/ip/tcp.hpp>
#include <string>
#include <memory>
#include <cstddef>


class HttpClient
    : public ClientBase<TcpStream>
{
public:
    /// Creates a Http client using the given host name and port number.
    explicit HttpClient(const std::string& host, std::uint16_t port, const uint16_t connectionTimeoutSeconds,
                        std::chrono::milliseconds resolveTimeout);
    explicit HttpClient(const boost::asio::ip::tcp::endpoint& ep, const std::string& host, const uint16_t connectionTimeoutSeconds);
};



#endif
