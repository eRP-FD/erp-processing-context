/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_CLIENTBASE_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_CLIENTBASE_HXX

#include "shared/network/client/ClientInterface.hxx"
#include "shared/network/client/ConnectionParameters.hxx"
#include "shared/network/client/implementation/ClientImpl.hxx"
#include "shared/network/client/request/ClientRequest.hxx"
#include "shared/network/client/response/ClientResponse.hxx"

#include <boost/asio/ip/tcp.hpp>
#include <cstdint>
#include <memory>
#include <string>


class SslStream;

template <class StreamClass>
class ClientBase : public ClientInterface
{
public:
    /**
     * Send the given request and return the received response.
     * Tee encryption is performed as defined at construction time of the request object,
     * i.e. via the ClientRequest::Builder.
     */
    ClientResponse send (const ClientRequest& clientRequest) override;

    bool hasLastTlsSessionBeenResumed () const;

    void inheritTlsSessionTicketFrom (const ClientBase<StreamClass>& client);

    void close (void);

    std::optional<boost::asio::ip::tcp::endpoint> currentEndpoint() const;

protected:
    ClientBase(const ConnectionParameters& params);

    ClientBase(const boost::asio::ip::tcp::endpoint& ep, const ConnectionParameters& params);

private:
    std::unique_ptr<ClientImpl<StreamClass>> mImplementation;
};

using HttpsClientBase = ClientBase<SslStream>;

#endif
