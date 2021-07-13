#ifndef ERP_PROCESSING_CONTEXT_CLIENT_TCPSTREAM_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_TCPSTREAM_HXX

#include "erp/util/AsyncStreamHelper.hxx"
#include "erp/util/GLog.hxx"
#include "erp/util/String.hxx"

#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/core/stream_traits.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <iterator>
#include <type_traits>
#include <utility>

// Use
// #define LOG_HTTP_STREAM_READS
// to activate logging of all read operations from plain HTTP streams.

// Use
// #define LOG_HTTP_STREAM_WRITES
// to activate logging of all write operations from plain HTTP streams.

namespace boost::asio
{
    class io_context;
    class mutable_buffer;
}

/**
 * This is a wrapper with the sole purpose of enabling logging for reads from and writes to the
 * wrapped tcp_stream.
 *
 * Turn either or both of LOG_HTTP_STREAM_{READS|WRITES} from #undef to #define to have reads and/or writes
 * logged to the console.
 */
class TcpStream
{
public:
    TcpStream(const std::string& hostname, const std::string& port, const uint16_t connectionTimeoutSeconds);

    void shutdown (void);

    template<class ConstBufferSequence>
    std::size_t write_some (
        ConstBufferSequence const& buffers,
        boost::beast::error_code& ec);

    template<class ConstBufferSequence, class WriteHandler>
    void async_write_some(
        ConstBufferSequence const& buffers,
        BOOST_ASIO_MOVE_ARG(WriteHandler) handler);

    std::size_t read_some(
        const boost::asio::mutable_buffer& buffer,
        boost::beast::error_code& ec);

private:
    std::unique_ptr<boost::asio::io_context> mIoContext;
    std::unique_ptr<boost::beast::tcp_stream> mTcpStream;

    template<class ConstBufferSequence>
    void logBuffers (const char* prefix, const ConstBufferSequence& buffers)
    {
        LOG(ERROR) << prefix << " : " << std::distance(buffers.begin(), buffers.end()) << " buffers";
        for (const auto& buffer : buffers)
        {
            LOG(ERROR) << "    " << prefix << " " << buffer.size() << " [" << String::quoteNewlines(
                std::string{reinterpret_cast<char*>(buffer.data()), buffer.size()}) << "]";
        }
    }
};


template<class ConstBufferSequence>
std::size_t TcpStream::write_some (
    ConstBufferSequence const& buffers,
    boost::beast::error_code& ec)
{
#ifdef LOG_SSL_STREAM_WRITES
    {
        VLOG(1) << "TcpStream::write_some() : starting to write";
        std::size_t size = 0;
        std::size_t count = 0;
        for (const auto& buffer : buffers)
        {
            size += buffer.size();
            ++count;
        }
        VLOG(1) << "    " << count << " buffers with " << size << " bytes";
    }
#endif

    const size_t count = AsyncStreamHelper::write_some(
        *mTcpStream,
        mIoContext.get(),
        buffers,
        ec);

#ifdef LOG_SSL_STREAM_WRITES
    VLOG(1) << "TcpStream::write_some() : wrote " << count << " bytes with result " << ec.value();
        #if LOG_SSL_STREAM_WRITES > 1
            writeBuffers("TcpStream::write_some()", buffers);
        #endif
        if (ec)
            VLOG(1) << "TcpStream::write_some() : error " << ec.message();
#endif

    return count;
}


template<class ConstBufferSequence, class WriteHandler>
void TcpStream::async_write_some(
    ConstBufferSequence const& buffers,
    BOOST_ASIO_MOVE_ARG(WriteHandler) handler)
{
#ifdef LOG_SSL_STREAM_WRITES
    std::size_t size = 0;
    {
        VLOG(1) << "TcpStream::async_write_some() : starting to write";
        std::size_t count = 0;
        for (const auto& buffer : buffers)
        {
            size += buffer.size();
            ++count;
        }
        VLOG(1) << "    " << count << " buffers with " << size << " bytes";
    }
#endif

    mTcpStream->async_write_some(buffers, std::forward<WriteHandler>(handler));

#ifdef LOG_SSL_STREAM_WRITES
    VLOG(1) << "TcpStream::async_write_some() : wrote " << size << " bytes";
        #if LOG_SSL_STREAM_WRITES > 1
            writeBuffers("TcpStream::async_write_some()", buffers);
        #endif
#endif
}


template<> struct boost::beast::is_sync_read_stream<TcpStream> : std::true_type {};
template<> struct boost::beast::is_sync_write_stream<TcpStream> : std::true_type {};
template<> struct boost::beast::is_async_write_stream<TcpStream> : std::true_type {};


#endif
