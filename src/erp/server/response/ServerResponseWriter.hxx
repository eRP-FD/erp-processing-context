#ifndef ERP_PROCESSING_CONTEXT_SERVER_RESPONSE_SERVERRESPONSEWRITER_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_RESPONSE_SERVERRESPONSEWRITER_HXX

#include "erp/common/Constants.hxx"

#include <boost/beast/http/serializer.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>


class SslStream;
class ValidatedServerResponse;

class ServerResponseWriter
{
public:
    explicit ServerResponseWriter (const ValidatedServerResponse& response);

    void write (SslStream& stream);

    std::string toString (void);

private:
    const ValidatedServerResponse& mResponse;
};


#endif
