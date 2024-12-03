/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/client/implementation/ClientImpl.hxx"

#include "shared/network/client/request/ValidatedClientRequest.hxx"
#include "shared/network/client/request/ClientRequestWriter.hxx"
#include "shared/network/client/response/ClientResponse.hxx"
#include "shared/network/client/response/ClientResponseReader.hxx"
#include "shared/util/TLog.hxx"

#include "shared/util/Expect.hxx"

#include <iostream>


template<>
ClientImpl<SslStream>::ClientImpl(const ConnectionParameters& params)
    : mConnectionTimeoutSeconds(params.connectionTimeoutSeconds)
    , mHostName(params.hostname)
    , mSessionContainer(params)
{
}

template<>
ClientImpl<SslStream>::ClientImpl(const boost::asio::ip::tcp::endpoint& ep, const ConnectionParameters& params)
    : mConnectionTimeoutSeconds(params.connectionTimeoutSeconds)
    , mHostName(params.hostname)
    , mSessionContainer(ep, params)
{
}


template<>
ClientImpl<TcpStream>::ClientImpl(const ConnectionParameters& params)
    : mConnectionTimeoutSeconds(params.connectionTimeoutSeconds),
      mHostName(params.hostname),
      mSessionContainer{params}
{
}

template<>
ClientImpl<TcpStream>::ClientImpl(const boost::asio::ip::tcp::endpoint& ep, const ConnectionParameters& params)
    : mConnectionTimeoutSeconds(params.connectionTimeoutSeconds)
    , mHostName(params.hostname)
    , mSessionContainer{ep, params}
{
}

template<class StreamClass>
ClientImpl<StreamClass>::~ClientImpl (void) = default;


template<class StreamClass>
ClientResponse ClientImpl<StreamClass>::send (const ClientRequest& clientRequest)
{
    TVLOG(3) << "HttpsClient at " << (void*)this << " sending request";

    try
    {
        mSessionContainer.establish();

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
