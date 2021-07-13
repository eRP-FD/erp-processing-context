#ifndef ERP_PROCESSING_CONTEXT_CLIENT_REQUEST_CLIENTREQUESTWRITER_HXX
#define ERP_PROCESSING_CONTEXT_CLIENT_REQUEST_CLIENTREQUESTWRITER_HXX

#include "erp/common/Constants.hxx"

#include <boost/beast/http/serializer.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/http/write.hpp>


class SslStream;
class ValidatedClientRequest;

class ClientRequestWriter
{
public:
    explicit ClientRequestWriter (const ValidatedClientRequest& request);

    template<class stream_type>
    void write (stream_type& stream);

    std::string toString (void);

private:
    const ValidatedClientRequest& mRequest;
    boost::beast::http::request<boost::beast::http::string_body> mBeastRequest;
    boost::beast::http::request_serializer<boost::beast::http::string_body> mSerializer;
};


template<class stream_type>
void ClientRequestWriter::write (stream_type& stream)
{
    boost::beast::http::write(stream, mSerializer);
}


#endif
