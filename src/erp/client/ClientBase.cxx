/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/client/ClientBase.hxx"

#include "erp/ErpRequirements.hxx"
#include "erp/client/request/ClientRequest.hxx"
#include "erp/client/response/ClientResponse.hxx"


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
    const std::string& host,
    const std::uint16_t port,
    const uint16_t connectionTimeoutSeconds,
    bool enforceServerAuthentication,
    const SafeString& caCertificates,
    const SafeString& clientCertificate,
    const SafeString& clientPrivateKey,
    const std::optional<std::string>& forcedCiphers)
    : mImplementation(std::make_unique<ClientImpl<StreamClass>>(
        host, port, connectionTimeoutSeconds, enforceServerAuthentication,
        caCertificates, clientCertificate, clientPrivateKey, forcedCiphers))
{
}


template<class StreamClass>
ClientResponse ClientBase<StreamClass>::send (const ClientRequest& clientRequest, const bool trustCn)
{
    return mImplementation->send(clientRequest, trustCn);
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


template class ClientBase<SslStream>;
template class ClientBase<TcpStream>;
