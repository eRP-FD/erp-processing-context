/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/server/SslStream.hxx"

#include <boost/beast/core/error.hpp>
#include <boost/asio/buffer.hpp>


SslStream SslStream::create (
    boost::asio::io_context& ioContext,
    boost::asio::ssl::context& sslContext)
{
    SslStream sslStream(&ioContext);
    sslStream.mSslStream = std::make_shared<boost::beast::ssl_stream<boost::beast::tcp_stream>>(ioContext, sslContext);
    return sslStream;
}


SslStream SslStream::create (
    boost::asio::ip::tcp::socket&& socket,
    boost::asio::ssl::context& sslContext)
{
    SslStream sslStream;
    sslStream.mSslStream = std::make_shared<boost::beast::ssl_stream<boost::beast::tcp_stream>>(std::move(socket), sslContext);
    return sslStream;
}


SslStream::SslStream()
    : mIoContext(nullptr)
    , mSslStream()
{
}


SslStream::SslStream(boost::asio::io_context* ioContext)
    : mIoContext(ioContext)
    , mSslStream()
{
}


boost::beast::lowest_layer_type<boost::beast::ssl_stream<boost::beast::tcp_stream>>& SslStream::getLowestLayer (void)
{
    return boost::beast::get_lowest_layer(*mSslStream);
}


void SslStream::shutdown (void)
{
    mSslStream->shutdown();
}


void SslStream::handshake (boost::asio::ssl::stream_base::handshake_type type)
{
    mSslStream->handshake(type);
}


::SSL* SslStream::getNativeHandle () const
{
    return mSslStream->native_handle();
}


size_t SslStream::read_some(
    const boost::asio::mutable_buffer& buffer,
    boost::beast::error_code& ec)
{
    #ifdef LOG_SSL_STREAM_READS
        TVLOG(1) << "SslStream::read_some() : starting to read";
    #endif

    const size_t count = AsyncStreamHelper::read_some(
        *mSslStream,
        mIoContext,
        buffer,
        ec);

    #ifdef LOG_SSL_STREAM_READS
        TVLOG(1) << "SslStream::read_some() : read " << count << " bytes";
        #if LOG_SSL_STREAM_READS > 1
            TLOG(ERROR) << "SslStream::read_some() : ["
                       << String::quoteNewlines(std::string((char*)buffer.data(), count))
                       << ']';
        #endif
    #endif

    return count;
}


boost::beast::ssl_stream<boost::beast::tcp_stream>& SslStream::getSslStream (void)
{
    return *mSslStream;
}


boost::beast::ssl_stream<boost::beast::tcp_stream>::executor_type SslStream::get_executor (void) noexcept
{
    return mSslStream->get_executor();
}

void SslStream::expiresAfter(const std::chrono::steady_clock::duration& duration)
{
    mSslStream->next_layer().expires_after(duration);
}
