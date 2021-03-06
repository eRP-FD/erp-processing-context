/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/client/TcpStream.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/TLog.hxx"

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/error.hpp>

#include <atomic>

#ifdef _WIN32
#pragma warning( disable : 4459 )
#endif

TcpStream::TcpStream (
    const std::string& hostname, const std::string& port, const uint16_t connectionTimeoutSeconds)
    : mIoContext(std::make_unique<boost::asio::io_context>())
    , mTcpStream(std::make_unique<boost::beast::tcp_stream>(*mIoContext))
{
    Expect(connectionTimeoutSeconds > 0, "Connection timeout must be greater than 0.");
    boost::asio::ip::tcp::resolver resolver(*mIoContext);
    boost::asio::ip::basic_resolver_results<boost::asio::ip::tcp> resolverResults;
    try
    {
        resolverResults = resolver.resolve(hostname, port);
    }
    catch (const boost::system::system_error& e)
    {
        // We observed some instability when we try to resolve especially OCSP hosts which should
        // not happen normally. We catch the observed boost exception and try it again at least once.
        TLOG(ERROR)  << "resolving hostname failed '" << hostname << ":" << port << "', try again";
        resolverResults = resolver.resolve(hostname, port);
    }

    mTcpStream->expires_after(std::chrono::seconds(connectionTimeoutSeconds));

    boost::system::error_code errorCode = AsyncStreamHelper::connect(*mTcpStream, *mIoContext, resolverResults);
    if(errorCode)
        throw boost::system::system_error(errorCode);
}

void TcpStream::shutdown (void)
{
   mTcpStream->close();
}

std::size_t TcpStream::read_some(
    const boost::asio::mutable_buffer& buffer,
    boost::beast::error_code& ec)
{
#ifdef LOG_SSL_STREAM_READS
    VLOG(1) << "TcpStream::read_some() : starting to read";
#endif

    const size_t count = AsyncStreamHelper::read_some(
        *mTcpStream,
        mIoContext.get(),
        buffer,
        ec);

#ifdef LOG_SSL_STREAM_READS
    VLOG(1) << "TcpStream::read_some() : read " << count << " bytes";
        #if LOG_SSL_STREAM_READS > 1
            LOG(ERROR) << "TcpStream::read_some() : ["
                       << String::quoteNewlines(std::string((char*)buffer.data(), count))
                       << ']';
        #endif
#endif

    return count;
}

void TcpStream::expiresAfter(const std::chrono::steady_clock::duration& duration)
{
    mTcpStream->expires_after(duration);
}
