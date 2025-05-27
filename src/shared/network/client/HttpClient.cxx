/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/client/HttpClient.hxx"


HttpClient::HttpClient(const std::string& host, const uint16_t port, std::chrono::milliseconds connectionTimeout,
                       std::chrono::milliseconds resolveTimeout)
    : ClientBase<TcpStream>(ConnectionParameters{.hostname = host,
                                                 .port = std::to_string(port),
                                                 .connectionTimeout = connectionTimeout,
                                                 .resolveTimeout = resolveTimeout,
                                                 .tlsParameters = {}})
{
}

HttpClient::HttpClient(const boost::asio::ip::tcp::endpoint& ep, const std::string& host,
                       std::chrono::milliseconds connectionTimeout)
    : ClientBase<TcpStream>(ep, ConnectionParameters{.hostname = host,
                                                     .port = "",// ignored when the endpoint is set
                                                     .connectionTimeout = connectionTimeout,
                                                     .resolveTimeout = std::chrono::seconds{2},
                                                     .tlsParameters = std::nullopt})
{
}
