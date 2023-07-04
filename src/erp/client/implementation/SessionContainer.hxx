/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_IMPLEMENTATION_SESSIONCONTAINER_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_IMPLEMENTATION_SESSIONCONTAINER_HXX

#include "erp/client/implementation/TlsSession.hxx"
#include "erp/client/TcpStream.hxx"


// This "base" class only provides an empty class that is templated so that two specializations can be defined
// with the actual implementation. One for each type of socket stream, with and without support for TLS.
template <class StreamClass>
class SessionContainer;


// the specialization for SslStream and TlsSession ( https ) scenario
template <>
class SessionContainer<SslStream>
{
public:
    SessionContainer(
        const std::string& hostname,
        const std::string& port,
        const uint16_t connectionTimeoutSeconds,
        bool enforceServerAuthentication,
        const SafeString& caCertificates,
        const SafeString& clientCertificate,
        const SafeString& clientPrivateKey,
        const std::optional<std::string>& forcedCiphers)
        : mTlsSession(hostname, port, connectionTimeoutSeconds, enforceServerAuthentication,
                      caCertificates, clientCertificate, clientPrivateKey, forcedCiphers),
          mIsEstablished(false)
    {}

    SessionContainer(
        const boost::asio::ip::tcp::endpoint& ep,
        const std::string& hostname,
        const uint16_t connectionTimeoutSeconds,
        bool enforceServerAuthentication,
        const SafeString& caCertificates,
        const SafeString& clientCertificate,
        const SafeString& clientPrivateKey,
        const std::optional<std::string>& forcedCiphers)
        : mTlsSession(ep, hostname, connectionTimeoutSeconds, enforceServerAuthentication,
                      caCertificates, clientCertificate, clientPrivateKey, forcedCiphers),
          mIsEstablished(false)
    {}

    SslStream& getStream() { return mTlsSession.getSslStream(); }

    void establish(const bool trustCn)
    {
        if ( ! mIsEstablished)
        {
            mTlsSession.establish(trustCn);

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
    SessionContainer(const std::string& hostname, const std::string& port, const uint16_t connectionTimeoutSeconds)
        : mTcpStream(hostname, port, connectionTimeoutSeconds)
    {}

    SessionContainer(const boost::asio::ip::tcp::endpoint& ep, const std::string& hostname, const uint16_t connectionTimeoutSeconds)
        : mTcpStream(ep, hostname, connectionTimeoutSeconds)
    {}

    TcpStream& getStream() { return mTcpStream; };
    void establish(const bool) {};
    bool hasBeenResumed() const { return false; }
    void inheritTicketFrom (const SessionContainer<TcpStream>&)
    {
        Fail("The functionality is not supported.");
    }

    void teardown() { mTcpStream.shutdown(); }

private:
    TcpStream mTcpStream;
};


#endif
