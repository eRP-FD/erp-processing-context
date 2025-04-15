/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/client/HttpsClient.hxx"


HttpsClient::HttpsClient(const ConnectionParameters& params)
    : ClientBase<SslStream>(params)
{
}

HttpsClient::HttpsClient(const boost::asio::ip::tcp::endpoint& ep, const ConnectionParameters& params)
    : ClientBase<SslStream>(ep, params)
{
}


HttpsClient::HttpsClient (ClientBase<SslStream>&& other)
    : ClientBase<SslStream>(std::move(other))
{
}
