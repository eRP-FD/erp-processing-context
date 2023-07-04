/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_CLIENTBASE_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_CLIENTBASE_HXX

#include "erp/client/request/ClientRequest.hxx"
#include "erp/client/response/ClientResponse.hxx"
#include "erp/client/implementation/ClientImpl.hxx"
#include "erp/client/ClientInterface.hxx"

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
    ClientResponse send (const ClientRequest& clientRequest, const bool trustCn = false) override;

    bool hasLastTlsSessionBeenResumed () const;

    void inheritTlsSessionTicketFrom (const ClientBase<StreamClass>& client);

    void close (void);

protected:
    ClientBase (
        const std::string& host,
        std::uint16_t port,
        const uint16_t connectionTimeoutSeconds,
        bool enforceServerAuthentication,
        const SafeString& caCertificates,
        const SafeString& clientCertificate,
        const SafeString& clientPrivateKey,
        const std::optional<std::string>& forcedCiphers);

    ClientBase(const boost::asio::ip::tcp::endpoint& ep, const std::string& host,
               const uint16_t connectionTimeoutSeconds, bool enforceServerAuthentication,
               const SafeString& caCertificates, const SafeString& clientCertificate,
               const SafeString& clientPrivateKey, const std::optional<std::string>& forcedCiphers);

private:
    std::unique_ptr<ClientImpl<StreamClass>> mImplementation;
};

using HttpsClientBase = ClientBase<SslStream>;

#endif
