#include "erp/beast/BoostBeastStringWriter.hxx"

#include "erp/common/BoostBeastMethod.hxx"
#include "erp/common/BoostBeastHttpStatus.hxx"
#include "erp/common/Header.hxx"
#include "erp/util/Expect.hxx"

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http.hpp>


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
            message.result(toBoostBeastStatus(header.status()));
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
                s.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
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
