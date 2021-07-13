#ifndef ERP_PROCESSING_CONTEXT_SERVER_REQUEST_SERVERREQUESTREADER_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_REQUEST_SERVERREQUESTREADER_HXX

#include "erp/ErpConstants.hxx"

#include "erp/server/request/ServerRequest.hxx"
#include "erp/server/SslStream.hxx"
#include "erp/common/Header.hxx"

#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>


class ServerRequestReader
{
public:
    explicit ServerRequestReader (SslStream& stream);

    ServerRequest read (void);
    void closeConnection (bool expectError);

    bool isStreamClosed (void) const;

private:
    SslStream& mStream;
    boost::beast::flat_static_buffer<ErpConstants::DefaultBufferSize> mBuffer;
    boost::beast::http::request_parser<boost::beast::http::string_body> mParser;
    bool mIsStreamClosed;

    Header readHeader (void);
    void markStreamAsClosed (void);
};


#endif
