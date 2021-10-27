/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_RESPONSE_SERVERRESPONSEWRITER_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_RESPONSE_SERVERRESPONSEWRITER_HXX

#include "erp/common/Constants.hxx"
#include "erp/server/response/ValidatedServerResponse.hxx"

#include <boost/beast/http/serializer.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>


class SslStream;
class ValidatedServerResponse;

class ServerResponseWriter
{
public:
    explicit ServerResponseWriter (void);

    void write (
        SslStream& stream,
        ValidatedServerResponse&& response);

    using Callback = std::function<void(bool success)>;
    void writeAsynchronously (
        SslStream& stream,
        ValidatedServerResponse&& response,
        Callback callback);

    std::string toString (ValidatedServerResponse&& response);
};


#endif
