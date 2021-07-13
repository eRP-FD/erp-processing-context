#include "erp/server/response/ServerResponseWriter.hxx"

#include "erp/beast/BoostBeastStringWriter.hxx"
#include "erp/server/response/ValidatedServerResponse.hxx"
#include "erp/server/SslStream.hxx"

#include "erp/common/BoostBeastHttpStatus.hxx"

#include <boost/beast/http/write.hpp>


ServerResponseWriter::ServerResponseWriter(
    const ValidatedServerResponse& response)
    : mResponse(response)
{
}


void ServerResponseWriter::write (SslStream& stream)
{
    boost::beast::http::response<boost::beast::http::string_body> response;
    boost::beast::http::response_serializer<boost::beast::http::string_body> serializer(response);

    serializer.limit(Constants::DefaultBufferSize);
    response.body() = mResponse.getBody();

    response.result(toBoostBeastStatus(mResponse.getHeader().status()));
    response.content_length(response.body().size());
    for (const auto& field : mResponse.getHeader().headers())
        response.set(field.first, field.second);

    boost::beast::http::write(stream, serializer);
}


std::string ServerResponseWriter::toString (void)
{
    return BoostBeastStringWriter::serializeResponse(mResponse.getHeader(), mResponse.getBody());
}
