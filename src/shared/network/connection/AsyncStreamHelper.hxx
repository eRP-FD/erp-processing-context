/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_UTIL_ASYNCSTREAMHELPER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_UTIL_ASYNCSTREAMHELPER_HXX

#include "shared/util/ExceptionWrapper.hxx"
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
        std::atomic_bool& completed)
    {
        auto timeoutAt = std::chrono::steady_clock::now() + std::chrono::minutes{5};
        while(!ioContext.stopped() && !completed.load()) {
            ioContext.run_one_for(std::chrono::milliseconds{100});
            if (std::chrono::steady_clock::now() >= timeoutAt)
            {
                HeaderLog::error("Operation aborted due to emergency timeout");
                throw ExceptionWrapper<boost::system::system_error>::create(
                    {__FILE__, __LINE__}, make_error_code(boost::system::errc::timed_out));
            }
        }
    }
};



template<class Stream>
boost::system::error_code AsyncStreamHelper::connect(
    Stream& stream,
    boost::asio::io_context& ioContext,
    const boost::asio::ip::basic_resolver_results<boost::asio::ip::tcp>& resolverResults)
{
    if (! ioContext.stopped())
    {
        auto ec = make_error_code(boost::system::errc::operation_canceled);
        std::atomic_bool completed{false};
        stream.async_connect(resolverResults, [&ec, &completed](const boost::system::error_code& error,
                                                                const boost::asio::ip::tcp::endpoint& endpoint) {
            if (! error)
            {
                HeaderLog::vlog(2, [&] {
                    return std::ostringstream{} << "connected to: " << endpoint;
                });
            }
            ec = error;
            completed.store(true);
        });

        waitForOperation(ioContext, completed);

        return ec;
    }
    boost::system::error_code ec;
    stream.connect(resolverResults, ec);
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
    if (ioContext && !ioContext->stopped())
    {
        std::atomic_bool completed{false};
        stream.async_write_some(
            buffers,
            [&ec, &count, &completed](
                const boost::system::error_code& error,
                std::size_t transferred) -> void
            {
              ec = error;
              count = transferred;
              completed.store(true);
            });

        waitForOperation(*ioContext, completed);
        if (!completed)
        {
            ec = make_error_code(boost::system::errc::operation_canceled);
        }
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
    if (ioContext && !ioContext->stopped())
    {
        std::atomic_bool completed{false};
        stream.async_read_some(
            buffer,
            [&ec, &count, &completed](
                const boost::system::error_code& error,
                std::size_t transferred) -> void
            {
              ec = error;
              count = transferred;
              completed.store(true);
            });

        waitForOperation(*ioContext, completed);
        if (!completed)
        {
            ec = make_error_code(boost::system::errc::operation_canceled);
        }
    }
    else
    {
        count = stream.read_some(buffer, ec);
    }

    return count;
}

#endif
