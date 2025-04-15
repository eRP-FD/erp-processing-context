/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_IMPLEMENTATION_CLIENTIMPL_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_IMPLEMENTATION_CLIENTIMPL_HXX

#include "shared/network/client/implementation/SessionContainer.hxx"
#include "shared/network/client/ConnectionParameters.hxx"

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http/buffer_body.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/serializer.hpp>
#include <chrono>
#include <memory>
#include <string>

class ClientRequest;
class ClientResponse;
class SafeString;

template <class StreamClass>
class ClientRequestTemplate;


template <class StreamClass>
class ClientImpl
{
public:
    ClientImpl(const ConnectionParameters& params);
    ClientImpl(const boost::asio::ip::tcp::endpoint& ep, const ConnectionParameters& params);
    ~ClientImpl (void);

    ClientResponse send (const ClientRequest& clientRequest);

    bool hasLastTlsSessionBeenResumed () const;

    void inheritTlsSessionTicketFrom (const ClientImpl<StreamClass>& client);

    void close (void);

private:
    std::chrono::milliseconds mConnectionTimeout;
    std::string mHostName;
    SessionContainer<StreamClass> mSessionContainer;
};

#endif
