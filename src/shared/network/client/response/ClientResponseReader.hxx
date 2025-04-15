/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_RESPONSE_CLIENTRESPONSEREADER_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_RESPONSE_CLIENTRESPONSEREADER_HXX


#include "shared/ErpConstants.hxx"
#include "shared/network/client/response/ClientResponse.hxx"
#include "shared/network/connection/SslStream.hxx"
#include "shared/network/message/Header.hxx"
#include "shared/util/ExceptionWrapper.hxx"
#include "shared/util/HeaderLog.hxx"

#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>


class ClientResponseReader
{
public:
    explicit ClientResponseReader (void);

    template<class stream_type>
    Header readHeader (stream_type& stream);

    template<class stream_type>
    ClientResponse read (stream_type& stream);

    ClientResponse read (const std::string& s);

    template<class stream_type>
    void closeConnection (stream_type& stream, bool expectError);

    bool isStreamClosed (void) const;

private:
    boost::beast::flat_static_buffer<ErpConstants::DefaultBufferSize> mBuffer;
    boost::beast::http::response_parser<boost::beast::http::string_body> mParser;
    bool mIsStreamClosed{false};

    void markStreamAsClosed (void);
    Header convertHeader (void);
};


template<class stream_type>
ClientResponse ClientResponseReader::read (stream_type& stream)
{
    auto header = readHeader(stream);
    boost::beast::error_code ec;
    boost::beast::http::read(stream, mBuffer, mParser, ec);
    if (ec.failed() && ec != boost::asio::ssl::error::stream_truncated)
    {
        HeaderLog::warning([&] {
            return std::ostringstream{} << "clientResponseReader::readSome: " << ec.message().c_str();
        });
    }

    ClientResponse response(std::move(header), mParser.get().body());

    return response;
}


template<class stream_type>
Header ClientResponseReader::readHeader (stream_type& stream)
{
    boost::beast::error_code ec;
    read_header(stream, mBuffer, mParser, ec);
    if (ec == boost::beast::http::error::end_of_stream || ec == boost::asio::ssl::error::stream_truncated)
    {
        HeaderLog::vlog(1, "TcpRequestReader : socket closed");
        HeaderLog::vlog(1, [&] {
            return std::ostringstream{} << "content of buffer is '"
                                        << std::string(reinterpret_cast<const char*>(mBuffer.data().data()),
                                                       mBuffer.data().size())
                                        << "'";
        });
        markStreamAsClosed();
        throw ExceptionWrapper<boost::beast::system_error>::create({__FILE__, __LINE__}, ec);
    }
    if (ec == boost::beast::error::timeout )
    {
        HeaderLog::vlog(1, "TcpRequestReader : socket read timeout");
        // the connection is automatically closed by boost, when a timeout happens
        markStreamAsClosed();
        throw ExceptionWrapper<boost::beast::system_error>::create({__FILE__, __LINE__}, ec);
    }
    if (ec)
    {
        HeaderLog::vlog(1, [&] {
            return std::ostringstream{} << "content of buffer is '"
                                        << std::string(reinterpret_cast<const char*>(mBuffer.data().data()),
                                                       mBuffer.data().size())
                                        << "'";
        });
        if (ec.failed() && ec != boost::asio::ssl::error::stream_truncated)
        {
            HeaderLog::error([&] {
                return std::ostringstream{} << "clientResponseReader::readHeader: " << ec.message().c_str();
            });
        }
        closeConnection(stream, false);
        throw ExceptionWrapper<boost::beast::system_error>::create({__FILE__, __LINE__}, ec);
    }

    return convertHeader();
}


template<class stream_type>
void ClientResponseReader::closeConnection (stream_type& stream, const bool expectError)
{
    markStreamAsClosed();

    try
    {
        stream.shutdown();
    }
    catch(const boost::system::system_error& e)
    {
        if (expectError)
        {
            // We are closing the connection hard. This is supposed the reaction to an earlier error where
            // either client or (this) server has made an error. Therefore some data may not have been read from
            // the stream and proper shutdown protocol may impossible to perform.
            //
            // As a result we expect an exception being thrown. And ignore it.
            HeaderLog::warning(e.what());
        }
        else
        {
            // Expected a clean shutdown. Forwarding the exception.
            throw;
        }
    }
}


#endif
