/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/beast/BoostBeastStringWriter.hxx"
#include "shared/beast/BoostBeastMethod.hxx"
#include "shared/network/message/Header.hxx"
#include "shared/util/Expect.hxx"
#include "fhirtools/util/Gsl.hxx"

#include <boost/beast/http.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>


namespace {
    template<class Message, bool isRequest>
    void toBeastMessage (Message& message, const Header& header, const std::string_view& body)
    {
        // Convert header values that differ between request and response.
        if constexpr (isRequest)
        {
            message.method(toBoostBeastVerb(header.method()));
            message.target(header.target());
        }
        else
        {
            message.result(static_cast<unsigned int>(header.status()));
        }

        // Convert header values that are the same for request and response.
        message.version(header.version());
        for (const auto& field : header.headers())
            message.set(field.first, field.second);

        // Convert the body.
        message.body() = body;
    }

    template<class Serializer>
    std::string serialize (Serializer& serializer)
    {
        boost::beast::error_code ec;
        std::ostringstream s;
        auto visitor = [&s, &serializer] (auto&, auto buffers)
        {
            for (const auto&& buffer : buffers)
                s.write(reinterpret_cast<const char*>(buffer.data()), gsl::narrow<std::streamsize>(buffer.size()));
            serializer.consume(boost::beast::buffer_bytes(buffers));
        };
        while (!serializer.is_done())
        {
            serializer.next(ec, visitor);
            Expect(ec.value() == 0, "serializing request failed");
        }
        return s.str();
    }
}


std::string BoostBeastStringWriter::serializeRequest (const Header& header, const std::string_view& body)
{
    boost::beast::http::request<boost::beast::http::string_body> request;

    toBeastMessage<decltype(request), true>(request, header, body);

    boost::beast::http::request_serializer<boost::beast::http::string_body> serializer (request);
    return serialize(serializer);
}


std::string BoostBeastStringWriter::serializeResponse (const Header& header, const std::string_view& body)
{
    boost::beast::http::response<boost::beast::http::string_body> response;
    toBeastMessage<decltype(response), false>(response, header, body);

    boost::beast::http::response_serializer<boost::beast::http::string_body> serializer (response);
    return serialize(serializer);
}
