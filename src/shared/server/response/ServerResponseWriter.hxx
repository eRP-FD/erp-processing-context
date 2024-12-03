/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_RESPONSE_SERVERRESPONSEWRITER_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_RESPONSE_SERVERRESPONSEWRITER_HXX

#include "shared/common/Constants.hxx"
#include "shared/server/response/ValidatedServerResponse.hxx"

#include <boost/beast/http/serializer.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>


class AccessLog;
class SslStream;
class ValidatedServerResponse;

class ServerResponseWriter
{
public:
    explicit ServerResponseWriter (void) = default;

    void write (
        SslStream& stream,
        ValidatedServerResponse&& response, AccessLog* accessLog = nullptr);

    using Callback = std::function<void(bool success)>;
    void writeAsynchronously (
        SslStream& stream,
        ValidatedServerResponse&& response,
        Callback callback, AccessLog* accessLog = nullptr);

    std::string toString (ValidatedServerResponse&& response);
};


#endif
