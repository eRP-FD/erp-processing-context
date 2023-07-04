/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_CLIENT_RESPONSE_CLIENTRESPONSEREADER_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_RESPONSE_CLIENTRESPONSEREADER_HXX

#include "erp/ErpConstants.hxx"
#include "erp/client/response/ClientResponse.hxx"
#include "erp/util/TLog.hxx"

#include "erp/server/SslStream.hxx"
#include "erp/common/Header.hxx"
#include "erp/server/ErrorHandler.hxx"

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
    bool mIsStreamClosed;

    void markStreamAsClosed (void);
    Header convertHeader (void);
};


template<class stream_type>
ClientResponse ClientResponseReader::read (stream_type& stream)
{
    auto header = readHeader(stream);
    boost::beast::error_code ec;
    boost::beast::http::read(stream, mBuffer, mParser, ec);
    ErrorHandler(ec).throwOnServerError("clientResponseReader::readSome", {__FILE__, __LINE__});

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
        TVLOG(1) << "TcpRequestReader : socket closed";
        TVLOG(1) << "content of buffer is '"
                 << std::string(reinterpret_cast<const char*>(mBuffer.data().data()), mBuffer.data().size()) << "'";
        markStreamAsClosed();
        Fail2("response was only partially received", std::runtime_error);
    }
    else if (ec)
    {
        TVLOG(1) << "content of buffer is '"
                 << std::string(reinterpret_cast<const char*>(mBuffer.data().data()), mBuffer.data().size()) << "'";
        ErrorHandler(ec).reportServerError("clientResponseReader::readHeader");
        closeConnection(stream, false);
        Fail2("read_header() failed", std::runtime_error);
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
            TLOG(ERROR) << e.what();
        }
        else
        {
            // Expected a clean shutdown. Forwarding the exception.
            throw;
        }
    }
}


#endif
