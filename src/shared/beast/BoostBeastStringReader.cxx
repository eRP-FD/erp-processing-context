/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/beast/BoostBeastStringReader.hxx"

#include "shared/beast/BoostBeastHeader.hxx"
#include "shared/util/TLog.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/UrlHelper.hxx"

#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>


namespace {
    /**
     * Feed a complete string to the given parser.
     */
    template<class Parser>
    void putString (Parser& parser, const std::string_view& s)
    {
        size_t offset = 0;
        size_t remaining = s.size();
        while (remaining > 0)
        {
            ErpExpect(! parser.is_done(), HttpStatus::BadRequest,
                "HTTP parser finished before end of message, Content-Length field is missing or the value is too low.");
            boost::beast::error_code ec;
            const size_t count = parser.put(boost::asio::buffer(s.substr(offset, remaining)), ec);
            // In general it is not an error if count is 0. It means we would need more data, beyond the given `s`, so
            // that the last bytes of `s` can be parsed as well. In this case, however, we can't supply more data.
            ErpExpect(ec.value() == 0, HttpStatus::BadRequest, "Parsing the HTTP message header failed.");
            ErpExpect(count > 0, HttpStatus::BadRequest, "HTTP message parser: not enough data");

            offset += count;
            remaining -= count;
        }
    }
}



std::tuple<Header,std::string> BoostBeastStringReader::parseRequest (const std::string_view& headerAndBody)
{
    boost::beast::http::request_parser<boost::beast::http::string_body> parser;
    // Consume input buffers across semantic boundaries
    parser.eager(true);

    putString(parser, headerAndBody);
    ErpExpect(parser.is_header_done(), HttpStatus::BadRequest,
              "HTTP Parser did not finish correctly to parse the request header");
    ErpExpect(parser.is_done(), HttpStatus::BadRequest,
              "HTTP parser did not finish correctly, Content-Length field is too large.");

    auto header = BoostBeastHeader::fromBeastRequestParser(parser);
    auto& body = parser.get().body();

    try
    {
        // The boost beast parser only works if there are no spaces in the request URL. That means that unescaping these
        // spaces, and other escaped characters, could not be done earlier.
        header.setTarget(UrlHelper::unescapeUrl(header.target()));
    }
    catch (const std::runtime_error& re)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Failed to unescape URL",
                               "what(): " + std::string{re.what()} + " URL=" + header.target());
    }

    TVLOG(1) << "User-Agent: " << header.header("User-Agent").value_or("<not set>");

    return {header, body};
}


std::tuple<Header,std::string> BoostBeastStringReader::parseResponse (const std::string_view& headerAndBody)
{
    boost::beast::http::response_parser<boost::beast::http::string_body> parser;
    // Consume input buffers across semantic boundaries
    parser.eager(true);

    putString(parser, headerAndBody);
    Expect(parser.is_header_done(), "parser has not finished to parse the request");
    Expect(parser.is_done(), "parser has not finished to parse the request");

    auto header = BoostBeastHeader::fromBeastResponseParser(parser);
    auto& body = parser.get().body();

    return {header, body};
}
