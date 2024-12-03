/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_UTIL_ASYNCSTREAMHELPER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_UTIL_ASYNCSTREAMHELPER_HXX

#include "shared/util/HeaderLog.hxx"

#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <memory>
#include <cstdint>
#include <utility>

/**
 * The intention of this implementation is to allow to use asynchronous boost beast API
 * in synchronous mode ( without having additional context thread ).
 * E.g. map
 *      connect() to async_connect()
 *      write_some() to async_write_some()
 *      read_some() to async_read_some()
 *
 * The motivation is to use the support of time outs support by asynchronous boost beast API.
 */
class AsyncStreamHelper
{
public:
    /**
     * Allows to create connection using asynchronous boost beast API.
     *
     * @tparam Stream           the type of the stream to be connected
     *
     * @param stream            the stream to be connected
     * @param ioContext         context to execute the connection without having context thread,
     *                          it has to be the same as the one used by stream
     * @param resolverResults   the resolver results to use for connection
     *
     * @return                  boost beast error code
     */
    template<class Stream>
    static boost::system::error_code connect(
        Stream& stream,
        boost::asio::io_context& ioContext,
        const boost::asio::ip::basic_resolver_results<boost::asio::ip::tcp>& resolverResults);


    /**
     * Allows to write some bytes using asynchronous boost beast API.
     * If no context is provided, the synchronous API is used.
     *
     * @tparam Stream               the type of the stream to write to
     * @tparam ConstBufferSequence  the type of the buffer with data to write
     *
     * @param stream                the stream to write to
     * @param ioContext             optional pointer to context to execute the connection without having context thread,
     *                              when ioContext is present, it has to be the same as the one used by stream
     * @param buffers               the buffer with data to write
     * @param ec                    boost beast error code
     *
     * @return                      count of written bytes
     */
    template<class Stream, class ConstBufferSequence>
    static size_t write_some (
        Stream& stream,
        boost::asio::io_context* ioContext,
        ConstBufferSequence const& buffers,
        boost::beast::error_code& ec);


    /**
     * Allows to read some bytes using asynchronous boost beast API.
     * If no context is provided, the synchronous API is used.
     *
     * @tparam Stream               the type of the stream to read from
     *
     * @param stream                the stream to be read from
     * @param ioContext             optional pointer to context to execute the connection without having context thread,
     *                              when ioContext is present, it has to be the same as the one used by stream
     * @param buffer                the buffer to be filled
     * @param ec                    boost beast error code
     *
     * @return                      count of read bytes
     */
    template<class Stream>
    static size_t read_some(
        Stream& stream,
        boost::asio::io_context* ioContext,
        const boost::asio::mutable_buffer& buffer,
        boost::beast::error_code& ec);

private:

    static void waitForOperation(
        boost::asio::io_context& ioContext,
        boost::beast::error_code& ec)
    {
        if (ioContext.stopped())
        {
            ioContext.restart();
        }

        while(ioContext.run_one() > 0 && !ec) {}
    }
};



template<class Stream>
boost::system::error_code AsyncStreamHelper::connect(
    Stream& stream,
    boost::asio::io_context& ioContext,
    const boost::asio::ip::basic_resolver_results<boost::asio::ip::tcp>& resolverResults)
{
    boost::system::error_code ec;
    stream.async_connect(resolverResults,
                         [&ec](const boost::system::error_code& error, const boost::asio::ip::tcp::endpoint& endpoint) {
                             HeaderLog::vlog(2, [&] {
                                 return std::ostringstream{} << "connected to: " << endpoint;
                             });
                             ec = error;
                         });

    waitForOperation(ioContext, ec);

    return ec;
}


template<class Stream, class ConstBufferSequence>
size_t AsyncStreamHelper::write_some (
    Stream& stream,
    boost::asio::io_context* ioContext,
    ConstBufferSequence const& buffers,
    boost::beast::error_code& ec)
{
    std::atomic<size_t> count = 0;
    if (ioContext)
    {
        stream.async_write_some(
            buffers,
            [&ec, &count](
                const boost::system::error_code& error,
                std::size_t transferred) -> void
            {
              ec = error;
              count = transferred;
            });

        waitForOperation(*ioContext, ec);
    }
    else
    {
        count = stream.write_some(buffers, ec);
    }

    return count;
}


template<class Stream>
size_t AsyncStreamHelper::read_some(
    Stream& stream,
    boost::asio::io_context* ioContext,
    const boost::asio::mutable_buffer& buffer,
    boost::beast::error_code& ec)
{
    std::atomic<size_t> count = 0;
    if (ioContext)
    {
        stream.async_read_some(
            buffer,
            [&ec, &count](
                const boost::system::error_code& error,
                std::size_t transferred) -> void
            {
              ec = error;
              count = transferred;
            });

        waitForOperation(*ioContext, ec);
    }
    else
    {
        count = stream.read_some(buffer, ec);
    }

    return count;
}

#endif
