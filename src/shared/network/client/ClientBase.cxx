/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/client/ClientBase.hxx"

#include "shared/ErpRequirements.hxx"
#include "shared/network/client/request/ClientRequest.hxx"
#include "shared/network/client/response/ClientResponse.hxx"


#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>

#include <limits>


namespace beast = boost::beast;         // from <boost/beast.hpp>
using tcp = boost::asio::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
namespace ssl = boost::asio::ssl;
namespace http = boost::beast::http;    // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>

template<class StreamClass>
ClientBase<StreamClass>::ClientBase (
    const ConnectionParameters& params)
    : mImplementation(std::make_unique<ClientImpl<StreamClass>>(params))
{
}

template<class StreamClass>
ClientBase<StreamClass>::ClientBase (
    const boost::asio::ip::tcp::endpoint& ep,
    const ConnectionParameters& params)
    : mImplementation(std::make_unique<ClientImpl<StreamClass>>(ep, params))
{
}


template<class StreamClass>
ClientResponse ClientBase<StreamClass>::send (const ClientRequest& clientRequest)
{
    return mImplementation->send(clientRequest);
}


template<class StreamClass>
bool ClientBase<StreamClass>::hasLastTlsSessionBeenResumed () const
{
    return mImplementation->hasLastTlsSessionBeenResumed();
}


template<class StreamClass>
void ClientBase<StreamClass>::inheritTlsSessionTicketFrom (const ClientBase& client)
{
    mImplementation->inheritTlsSessionTicketFrom(*client.mImplementation);
}


template<class StreamClass>
void ClientBase<StreamClass>::close (void)
{
    mImplementation->close();
}

template<typename StreamClass>
std::optional<boost::asio::ip::tcp::endpoint> ClientBase<StreamClass>::currentEndpoint() const
{
    return mImplementation ? mImplementation->currentEndpoint() : std::nullopt;
}


template class ClientBase<SslStream>;
template class ClientBase<TcpStream>;
