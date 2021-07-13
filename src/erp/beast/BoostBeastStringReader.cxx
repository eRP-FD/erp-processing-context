#include "erp/beast/BoostBeastStringReader.hxx"

#include "erp/beast/BoostBeastHeader.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/UrlHelper.hxx"

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
                      "Parser is already done, maybe the Content-Length field was not correct.");
            boost::beast::error_code ec;
            const size_t count = parser.put(boost::asio::buffer(s.substr(offset, remaining)), ec);
            // In general it is not an error if count is 0. It means we would need more data, beyond the given `s`, so
            // that the last bytes of `s` can be parsed as well. In this case, however, we can't supply more data.
            TVLOG(1) << ec.message();
            ErpExpect(ec.value() == 0, HttpStatus::BadRequest, "parsing the HTTP header failed");
            ErpExpect(count > 0, HttpStatus::BadRequest, "not enough data");

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
    Expect(parser.is_header_done(), "parser has not finished to parse the request");
    Expect(parser.is_done(), "parser has not finished to parse the request");

    auto header = BoostBeastHeader::fromBeastRequestParser(parser);
    auto& body = parser.get().body();

    // The boost beast parser only works if there are no spaces in the request URL. That means that unescaping these
    // spaces, and other escaped characters, could not be done earlier.
    header.setTarget(UrlHelper::unescapeUrl(header.target()));

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


