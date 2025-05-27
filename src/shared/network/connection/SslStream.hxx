/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_SSLSTREAM_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_SSLSTREAM_HXX

#include "shared/network/connection/AsyncStreamHelper.hxx"
#include "shared/util/HeaderLog.hxx"
#include "shared/util/String.hxx"


#include <boost/beast/core/detail/stream_traits.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/core/stream_traits.hpp>

#include <memory>
#include <cstdint>
#include <sstream>
#include <utility>

// Use
// #define LOG_SSL_STREAM_READS
// to activate logging of all read operations from TSL streams.

// Use
// #define LOG_SSL_STREAM_WRITES
// to activate logging of all write operations from TSL streams.

namespace boost::asio
{
    class io_context;

    namespace ssl
    {
        class context;
    }
}

/**
 * This is a wrapper with the sole purpose of enabling logging for reads from and writes to the
 * wrapped ssl_stream.
 *
 * Turn either or both of LOG_SSL_STREAM_{READS|WRITES} from #undef to #define to have reads and/or writes
 * logged to the console.
 */
class SslStream
{
public:
    static SslStream create (
        boost::asio::io_context& ioContext,
        boost::asio::ssl::context& sslContext);

    static SslStream create (
        boost::asio::ip::tcp::socket&& socket,
        boost::asio::ssl::context& sslContext);

    SslStream();

    void shutdown (void);

    boost::beast::lowest_layer_type<boost::beast::ssl_stream<boost::beast::tcp_stream>>& getLowestLayer (void);
    void handshake (boost::asio::ssl::stream_base::handshake_type type);

    ::SSL* getNativeHandle () const;

    template<class ConstBufferSequence>
    size_t write_some (
        ConstBufferSequence const& buffers,
        boost::beast::error_code& ec);

    template<class ConstBufferSequence, class WriteHandler>
    void async_write_some(
        ConstBufferSequence const& buffers,
        BOOST_ASIO_MOVE_ARG(WriteHandler) handler);

    size_t read_some(
        const boost::asio::mutable_buffer& buffer,
        boost::beast::error_code& ec);

    template<class MutableBufferSequence, class Handler>
    void async_read_some(
        MutableBufferSequence& buffers,
        BOOST_ASIO_MOVE_ARG(Handler) handler);

    boost::beast::ssl_stream<boost::beast::tcp_stream>& getSslStream (void);

    using executor_type = boost::beast::ssl_stream<boost::beast::tcp_stream>::executor_type;
    boost::beast::ssl_stream<boost::beast::tcp_stream>::executor_type get_executor (void) noexcept;

    void expiresAfter(const std::chrono::steady_clock::duration& duration);

    std::optional<boost::asio::ip::tcp::endpoint> currentEndpoint() const;

private:
    SslStream(boost::asio::io_context* ioContext);

    boost::asio::io_context* mIoContext;
    std::shared_ptr<boost::beast::ssl_stream<boost::beast::tcp_stream>> mSslStream;
};

template<class ConstBufferSequence>
    void logBuffers (const char* prefix, const ConstBufferSequence& buffers)
{
    HeaderLog::error([&] {
        return std::ostringstream{} << prefix << " : " << std::distance(buffers.begin(), buffers.end()) << " buffers";
    });
    for (const auto& buffer : buffers)
    {
        HeaderLog::error([&] {
            return std::ostringstream{} << "    " << prefix << " " << buffer.size() << " ["
                                        << String::quoteNewlines(std::string((char*) buffer.data(), buffer.size()))
                                        << "]";
        });
    }
}


template<class ConstBufferSequence>
size_t SslStream::write_some (
    ConstBufferSequence const& buffers,
    boost::beast::error_code& ec)
{
    #ifdef LOG_SSL_STREAM_WRITES
    {
        HeaderLog::vlog(1, "SslStream::write_some() : starting to write");
        size_t size = 0;
        size_t count = 0;
        for (const auto& buffer : buffers)
        {
            size += buffer.size();
            ++count;
        }
        HeaderLog::vlog(1, [&] {
            return std::ostringstream{} << "    " << count << " buffers with " << size << " bytes";
        });
    }
    #endif

    const size_t count = AsyncStreamHelper::write_some(
        *mSslStream,
        mIoContext,
        buffers,
        ec);

    #ifdef LOG_SSL_STREAM_WRITES
        TVLOG(1) << "SslStream::write_some() : wrote " << count << " bytes with result " << ec.value();
        #if LOG_SSL_STREAM_WRITES > 1
            writeBuffers("SslStream::write_some()", buffers);
        #endif
        if (ec)
            TVLOG(1) << "SslStream::write_some() : error " << ec.message();
    #endif

    return count;
}


template<class ConstBufferSequence, class WriteHandler>
//NOLINTNEXTLINE(misc-no-recursion)
void SslStream::async_write_some(
    ConstBufferSequence const& buffers,
    BOOST_ASIO_MOVE_ARG(WriteHandler) handler)
{
    #ifdef LOG_SSL_STREAM_WRITES
    size_t size = 0;
    {
        TVLOG(1) << "SslStream::async_write_some() : starting to write";
        size_t count = 0;
        for (const auto& buffer : buffers)
        {
            size += buffer.size();
            ++count;
        }
        TVLOG(1) << "    " << count << " buffers with " << size << " bytes";
    }
    #endif

    mSslStream->async_write_some(buffers, std::forward<WriteHandler>(handler));

    #ifdef LOG_SSL_STREAM_WRITES
        TVLOG(1) << "SslStream::async_write_some() : wrote " << size << " bytes";
        #if LOG_SSL_STREAM_WRITES > 1
            writeBuffers("SslStream::async_write_some()", buffers);
        #endif
    #endif
}


template<class MutableBufferSequence, class Handler>
//NOLINTNEXTLINE(misc-no-recursion)
void SslStream::async_read_some(
    MutableBufferSequence& buffers,
    BOOST_ASIO_MOVE_ARG(Handler) handler)
{
    mSslStream->async_read_some(buffers, std::forward<Handler>(handler));
}


template<> struct boost::beast::is_sync_read_stream<SslStream> : std::true_type {};
template<> struct boost::beast::is_sync_write_stream<SslStream> : std::true_type {};
template<> struct boost::beast::is_async_read_stream<SslStream> : std::true_type {};
template<> struct boost::beast::is_async_write_stream<SslStream> : std::true_type {};

#endif
