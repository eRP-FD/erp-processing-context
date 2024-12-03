/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_HTTPSCLIENT_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_HTTPSCLIENT_HXX

#include "shared/network/client/ClientBase.hxx"
#include "shared/network/client/ConnectionParameters.hxx"
#include "shared/util/SafeString.hxx"

#include <boost/asio/ip/tcp.hpp>
#include <cstdint>
#include <string>


class SslStream;


class HttpsClient
    : public ClientBase<SslStream>
{
public:
    /// Use this constructor to create a Https client using the given
    /// host name and port number with (optional) the given certificates.
    explicit HttpsClient(const ConnectionParameters& params);

    /// Use this constructor to create a Https client using a
    /// specific endpoint
    explicit HttpsClient (
        const boost::asio::ip::tcp::endpoint& ep, const ConnectionParameters& params);

    /// Use this constructor to convert from e.g. a TeeClient.
    explicit HttpsClient(ClientBase<SslStream>&& other);
};


#endif
