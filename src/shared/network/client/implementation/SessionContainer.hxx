/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_IMPLEMENTATION_SESSIONCONTAINER_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_IMPLEMENTATION_SESSIONCONTAINER_HXX

#include "shared/network/client/implementation/TlsSession.hxx"
#include "shared/network/connection/TcpStream.hxx"
#include "shared/util/ExceptionWrapper.hxx"


// This "base" class only provides an empty class that is templated so that two specializations can be defined
// with the actual implementation. One for each type of socket stream, with and without support for TLS.
template <class StreamClass>
class SessionContainer;


// the specialization for SslStream and TlsSession ( https ) scenario
template <>
class SessionContainer<SslStream>
{
public:
    SessionContainer(const ConnectionParameters& params)
        : mTlsSession(params),
          mIsEstablished(false)
    {}

    SessionContainer(
        const boost::asio::ip::tcp::endpoint& ep, const ConnectionParameters& params)
        : mTlsSession(ep, params),
          mIsEstablished(false)
    {}

    SslStream& getStream() { return mTlsSession.getSslStream(); }

    void establish()
    {
        if ( ! mIsEstablished)
        {
            mTlsSession.establish();

            mIsEstablished = true;
        }
    };

    bool hasBeenResumed() const { return mTlsSession.hasBeenResumed(); }

    void inheritTicketFrom (const SessionContainer<SslStream>& container)
    {
        mTlsSession.inheritTicketFrom(container.mTlsSession);
    }

    void teardown()
    {
        if (mIsEstablished)
        {
            mIsEstablished = false;
            mTlsSession.teardown();
        }
    }

private:
    TlsSession mTlsSession;
    bool mIsEstablished;
};


// the specialization for TcpStream ( http ) scenario
template <>
class SessionContainer<TcpStream>
{
public:
    SessionContainer(const ConnectionParameters& params)
        : mTcpStream(params.hostname, params.port, params.connectionTimeoutSeconds, params.resolveTimeout)
    {}

    SessionContainer(const boost::asio::ip::tcp::endpoint& ep, const ConnectionParameters& params)
        : mTcpStream(ep, params.hostname, params.connectionTimeoutSeconds)
    {}

    TcpStream& getStream() { return mTcpStream; };
    void establish() {};
    bool hasBeenResumed() const { return false; }
    void inheritTicketFrom (const SessionContainer<TcpStream>&)
    {
        throw ExceptionWrapper<std::runtime_error>::create({__FILE__, __LINE__}, "The functionality is not supported.");
    }

    void teardown() { mTcpStream.shutdown(); }

private:
    TcpStream mTcpStream;
};


#endif
