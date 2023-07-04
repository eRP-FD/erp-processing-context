/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/server/response/ServerResponseWriter.hxx"

#include "erp/beast/BoostBeastStringWriter.hxx"
#include "erp/server/AccessLog.hxx"
#include "erp/server/response/ValidatedServerResponse.hxx"
#include "erp/server/SslStream.hxx"

#include <boost/beast/http/write.hpp>

namespace {
    auto convertResponse (ValidatedServerResponse&& input, AccessLog* accessLog)
    {
        auto response = std::make_shared<boost::beast::http::response<boost::beast::http::string_body>>();

        response->body() = input.getBody();
        response->result(static_cast<unsigned int>(input.getHeader().status()));
        if (input.getHeader().hasHeader(Header::ContentLength))
        {
            auto contentLength = input.getHeader().contentLength();
            Expect3(contentLength == response->body().size(),
                    "content length " + std::to_string(contentLength) +
                        " doesn't match body size: " + std::to_string(response->body().size()),
                    std::logic_error);
            response->content_length(input.getHeader().contentLength());
        }
        for (const auto& field : input.getHeader().headers())
            response->set(field.first, field.second);

        if (accessLog)
        {
            accessLog->keyValue("response-body-length", response->body().size());
        }

        return response;
    }
}


void ServerResponseWriter::write (
    SslStream& stream,
    ValidatedServerResponse&& validatedResponse,
    AccessLog* accessLog)
{
    auto response = convertResponse(std::move(validatedResponse), accessLog);

    boost::beast::http::response_serializer<boost::beast::http::string_body> serializer(*response);
    serializer.limit(Constants::DefaultBufferSize);

    boost::beast::http::write(stream, serializer);
}


void ServerResponseWriter::writeAsynchronously (
    SslStream& stream,
    ValidatedServerResponse&& validatedResponse,
    Callback callback,
    AccessLog* accessLog)
{
    // Set up a boost::beast response object.
    auto response = convertResponse(std::move(validatedResponse), accessLog);

    // Set up a serializer for the response.
    auto serializer = std::make_shared<boost::beast::http::response_serializer<boost::beast::http::string_body>>(*response);
    serializer->limit(Constants::DefaultBufferSize);

    auto logContext = erp::server::Worker::tlogContext;
    boost::beast::http::async_write(
        stream,
        *serializer,
        [response, serializer, callback = std::move(callback), logContext]
        (const boost::system::error_code ec, const size_t count)
        {
            ScopedLogContext scopeLog(logContext);
            if (ec)
            {
                TLOG(ERROR) << "writing body asynchronously failed after " << count << " bytes: " << ec.message();
            }
            else
            {
                TVLOG(2) << "ServerResponseWriter::writeAsynchronously: write completed - bytes written: " << count;
            }

            callback(!ec);
        });
}


std::string ServerResponseWriter::toString (ValidatedServerResponse&& response)
{
    return BoostBeastStringWriter::serializeResponse(response.getHeader(), response.getBody());
}
