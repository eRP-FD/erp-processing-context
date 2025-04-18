/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_REQUEST_SERVERREQUESTREADER_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_REQUEST_SERVERREQUESTREADER_HXX

#include "shared/ErpConstants.hxx"

#include "shared/network/connection/SslStream.hxx"
#include "shared/network/message/Header.hxx"
#include "shared/server/request/ServerRequest.hxx"

#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>


class ServerRequestReader
{
public:
    using RequestConsumer = std::function<void(std::optional<ServerRequest>&&, std::exception_ptr)>;  // expected to be effectively noexcept

    explicit ServerRequestReader (SslStream& stream);

    ServerRequest read (void);
    /**
     * Read request data, header and body, asynchronously and pass the result to the given consumer.
     * In case of an error, an exception pointer is passed to the consumer instead of header and body.
     * Note that it is the responsibility to keep the ServerRequestReader object alive until the consumer
     * is called.
     */
    void readAsynchronously (RequestConsumer&& requestConsumer);
    void closeConnection (bool expectError);

    bool isStreamClosed (void) const;

private:
    void on_async_read(const RequestConsumer& requestConsumer, const std::optional<std::string>& logContext,
                       const boost::beast::error_code& ec, const size_t count);

    SslStream& mStream;
    boost::beast::flat_static_buffer<ErpConstants::DefaultBufferSize> mBuffer;
    boost::beast::http::request_parser<boost::beast::http::string_body> mParser;
    bool mIsStreamClosed{false};

    Header readHeader (void);
    void markStreamAsClosed (void);
};


#endif
