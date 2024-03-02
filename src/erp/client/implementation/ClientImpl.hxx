/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_IMPLEMENTATION_CLIENTIMPL_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_IMPLEMENTATION_CLIENTIMPL_HXX

#include "erp/client/implementation/SessionContainer.hxx"
#include "erp/client/request/ClientRequest.hxx"
#include "erp/client/response/ClientResponse.hxx"

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/serializer.hpp>
#include <boost/beast/http/buffer_body.hpp>

#include <memory>
#include <string>
#include <boost/beast/http/empty_body.hpp>


template <class StreamClass>
class ClientRequestTemplate;


template <class StreamClass>
class ClientImpl
{
public:
    ClientImpl (
        const std::string& hostname,
        uint16_t port,
        const uint16_t connectionTimeoutSeconds,
        std::chrono::milliseconds resolveTimeout,
        bool enforceServerAuthentication,
        const SafeString& caCertificates,
        const SafeString& clientCertificate,
        const SafeString& clientPrivateKey,
        const std::optional<std::string>& forcedCiphers);
    ClientImpl(const boost::asio::ip::tcp::endpoint& ep, const std::string& hostname,
               const uint16_t connectionTimeoutSeconds, bool enforceServerAuthentication,
               const SafeString& caCertificates, const SafeString& clientCertificate,
               const SafeString& clientPrivateKey, const std::optional<std::string>& forcedCiphers);
    ~ClientImpl (void);

    ClientResponse send (const ClientRequest& clientRequest, const bool trustCn);

    bool hasLastTlsSessionBeenResumed () const;

    void inheritTlsSessionTicketFrom (const ClientImpl<StreamClass>& client);

    void close (void);

private:
    const uint16_t mConnectionTimeoutSeconds;
    std::string mHostName;
    SessionContainer<StreamClass> mSessionContainer;
};

#endif
