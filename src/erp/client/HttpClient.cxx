/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/client/HttpClient.hxx"


HttpClient::HttpClient (const std::string& host, const uint16_t port, const uint16_t connectionTimeoutSeconds)
    : ClientBase<TcpStream>(host, port, connectionTimeoutSeconds, false, {}, {}, {}, std::nullopt)
{
}
