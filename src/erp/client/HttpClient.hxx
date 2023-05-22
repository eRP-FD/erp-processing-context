/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_HTTPCLIENT_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_HTTPCLIENT_HXX

#include "erp/client/ClientBase.hxx"

#include "erp/client/TcpStream.hxx"

#include <string>
#include <memory>
#include <cstddef>


class HttpClient
    : public ClientBase<TcpStream>
{
public:
    /// Creates a Http client using the given host name and port number.
    explicit HttpClient(const std::string& host, std::uint16_t port, const uint16_t connectionTimeoutSeconds);
};



#endif
