/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/client/HttpClient.hxx"


HttpClient::HttpClient(const std::string& host, const uint16_t port, const uint16_t connectionTimeoutSeconds,
                       std::chrono::milliseconds resolveTimeout)
    : ClientBase<TcpStream>(host, port, connectionTimeoutSeconds, resolveTimeout, false, {}, {}, {}, std::nullopt)
{
}

HttpClient::HttpClient(const boost::asio::ip::tcp::endpoint& ep, const std::string& host,
                       const uint16_t connectionTimeoutSeconds)
    : ClientBase<TcpStream>(ep, host, connectionTimeoutSeconds, false, {}, {}, {}, std::nullopt)
{
}
