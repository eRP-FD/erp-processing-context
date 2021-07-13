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
                      caCertificates, clientCertificate, clientPrivateKey, forcedCiphers)
    {}

    SslStream& getStream() { return mTlsSession.getSslStream(); }

    void establish(const bool trustCn)
    {
        // "ePA frontend of the insurant: TLS connection establishment as required"
        // GEMREQ-start A_15300
        mTlsSession.establish(trustCn);
        // GEMREQ-end A_15300
    };

    bool hasBeenResumed() const { return mTlsSession.hasBeenResumed(); }

    void inheritTicketFrom (const SessionContainer<SslStream>& container)
    {
        mTlsSession.inheritTicketFrom(container.mTlsSession);
    }

    void teardown() { mTlsSession.teardown(); }

private:
    TlsSession mTlsSession;
};


// the specialization for TcpStream ( http ) scenario
template <>
class SessionContainer<TcpStream>
{
public:
    SessionContainer(const std::string& hostname, const std::string& port, const uint16_t connectionTimeoutSeconds)
        : mTcpStream(hostname, port, connectionTimeoutSeconds)
    {}

    TcpStream& getStream() { return mTcpStream; };
    void establish(const bool) {};
    bool hasBeenResumed() const { return false; }
    void inheritTicketFrom (const SessionContainer<TcpStream>&)
    {
        throw std::runtime_error("The functionality is not supported.");
    }

    void teardown() { mTcpStream.shutdown(); }

private:
    TcpStream mTcpStream;
};

#endif
