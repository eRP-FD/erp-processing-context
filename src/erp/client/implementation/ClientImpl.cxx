/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/client/implementation/ClientImpl.hxx"

#include "erp/client/request/ValidatedClientRequest.hxx"
#include "erp/client/request/ClientRequestWriter.hxx"
#include "erp/client/response/ClientResponse.hxx"
#include "erp/client/response/ClientResponseReader.hxx"
#include "erp/util/TLog.hxx"

#include "erp/util/Expect.hxx"

#include <iostream>


template<>
ClientImpl<SslStream>::ClientImpl (
    const std::string& hostname,
    const uint16_t port,
    const uint16_t connectionTimeoutSeconds,
    bool enforceServerAuthentication,
    const SafeString& caCertificates,
    const SafeString& clientCertificate,
    const SafeString& clientPrivateKey,
    const std::optional<std::string>& forcedCiphers)
    : mConnectionTimeoutSeconds(connectionTimeoutSeconds),
      mHostName(hostname),
      mSessionContainer(hostname, std::to_string(port), connectionTimeoutSeconds,
                      enforceServerAuthentication, caCertificates, clientCertificate, clientPrivateKey, forcedCiphers)
{
}


template<>
ClientImpl<TcpStream>::ClientImpl (
    const std::string& hostname,
    const uint16_t port,
    const uint16_t connectionTimeoutSeconds,
    bool,
    const SafeString&,
    const SafeString&,
    const SafeString&,
    const std::optional<std::string>&)
    : mConnectionTimeoutSeconds(connectionTimeoutSeconds),
      mHostName(hostname),
      mSessionContainer{hostname, std::to_string(port), connectionTimeoutSeconds}
{
}


template<class StreamClass>
ClientImpl<StreamClass>::~ClientImpl (void) = default;


template<class StreamClass>
ClientResponse ClientImpl<StreamClass>::send (const ClientRequest& clientRequest, const bool trustCn)
{
    TVLOG(3) << "HttpsClient at " << (void*)this << " sending request";

    try
    {
        mSessionContainer.establish(trustCn);

        mSessionContainer.getStream().expiresAfter(std::chrono::seconds(mConnectionTimeoutSeconds));
        // Send the request.
        ClientRequestWriter writer(ValidatedClientRequest{clientRequest});
        writer.write(mSessionContainer.getStream());

        // Read and return the response.
        ClientResponseReader reader;
        ClientResponse response = reader.read(mSessionContainer.getStream());

        if (!response.getHeader().keepAlive())
            mSessionContainer.teardown();

        return response;
    }
    catch(const boost::system::system_error& e)
    {
        TLOG(ERROR) << "caught exception in ClientImpl::send(): boost system_error " << e.code() << " " << e.what()
                   << " (" << mHostName << ")";
        throw;
    }
    catch(const std::exception& e)
    {
        TLOG(ERROR) << "caught exception in ClientImpl()::send(): " << e.what() << " (" << mHostName << ")";
        throw;
    }
}


template<class StreamClass>
bool ClientImpl<StreamClass>::hasLastTlsSessionBeenResumed () const
{
    return mSessionContainer.hasBeenResumed();
}


template<class StreamClass>
void ClientImpl<StreamClass>::inheritTlsSessionTicketFrom (const ClientImpl<StreamClass>& client)
{
    mSessionContainer.inheritTicketFrom(client.mSessionContainer);
}


template<class StreamClass>
void ClientImpl<StreamClass>::close (void)
{
    mSessionContainer.teardown();
}


template class ClientImpl<TcpStream>;
template class ClientImpl<SslStream>;
