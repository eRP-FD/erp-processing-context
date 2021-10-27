/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/beast/BoostBeastStringReader.hxx"
#include "erp/util/ErpException.hxx"

#include <gtest/gtest.h>


class BoostBeastStringReaderTest : public testing::Test
{
public:
};



TEST_F(BoostBeastStringReaderTest, parseRequest)
{
    const std::string request =
        "GET /this/is/a/path HTTP/1.1\r\n"
        "Content-Type: text\r\n"
        "Content-Length: 16\r\n"
        "\r\n"
        "this is the body";
    auto [header, body] = BoostBeastStringReader::parseRequest(request);

    EXPECT_EQ(header.method(), HttpMethod::GET);
    EXPECT_EQ(header.target(), "/this/is/a/path");
    EXPECT_EQ(header.version(), Header::Version_1_1);
    EXPECT_TRUE(header.hasHeader(Header::ContentType));
    EXPECT_EQ(header.header(Header::ContentType).value(), "text");
    EXPECT_TRUE(header.hasHeader(Header::ContentLength));
    EXPECT_EQ(header.header(Header::ContentLength).value(), "16");

    EXPECT_EQ(body, "this is the body");
}


TEST_F(BoostBeastStringReaderTest, parseRequestWrongContentLength)
{
    const std::string request =
        "GET /this/is/a/path HTTP/1.1\r\n"
        "Content-Type: text\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "this is the body";
    ASSERT_THROW(BoostBeastStringReader::parseRequest(request), ErpException);
}



TEST_F(BoostBeastStringReaderTest, parseResponse)
{
    const std::string request =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text\r\n"
        "Content-Length: 22\r\n"
        "\r\n"
        "did not find your file";
    auto [header, body] = BoostBeastStringReader::parseResponse(request);

    EXPECT_EQ(header.status(), HttpStatus::NotFound);
    EXPECT_TRUE(header.hasHeader(Header::ContentType));
    EXPECT_EQ(header.header(Header::ContentType).value(), "text");
    EXPECT_TRUE(header.hasHeader(Header::ContentLength));
    EXPECT_EQ(header.header(Header::ContentLength).value(), "22");

    EXPECT_EQ(body, "did not find your file");
}



TEST_F(BoostBeastStringReaderTest, parseRequest_successWithLargeHeaderAndBody)
{
    // Header max size is currently configured as 8kB.
    std::string request =
        "GET /this/is/a/path HTTP/1.1\r\n"
        "Content-Type: text\r\n"
        "Content-Length: ##LENGTH##\r\n"
        "something-large: ##HEADER-VALUE##\r\n"
        "\r\n"
        "##BODY##";

    // Make the header only a little smaller than 8kB.
    const std::string largeHeaderValue = std::string(8000, 'X');
    request = String::replaceAll(request, "##HEADER-VALUE##", largeHeaderValue);

    // Also use a large body.
    const std::string largeBody = std::string(1024*1024, 'Y');
    request = String::replaceAll(request, "##BODY##", largeBody);
    request = String::replaceAll(request, "##LENGTH##", std::to_string(largeBody.size()));

    // Parse the request.
    auto [header, body] = BoostBeastStringReader::parseRequest(request);

    EXPECT_EQ(header.method(), HttpMethod::GET);
    EXPECT_EQ(header.target(), "/this/is/a/path");
    EXPECT_EQ(header.version(), Header::Version_1_1);
    EXPECT_TRUE(header.hasHeader(Header::ContentType));
    EXPECT_EQ(header.header(Header::ContentType).value(), "text");
    EXPECT_TRUE(header.hasHeader("something-large"));
    EXPECT_EQ(header.header("something-large").value(), largeHeaderValue);

    EXPECT_EQ(body.size(), largeBody.size());
    EXPECT_EQ(body, largeBody);
}


TEST_F(BoostBeastStringReaderTest, parseRequest_failForOversizedHeader)
{
    // Header max size is currently configured as 8kB.
    std::string request =
        "GET /this/is/a/path HTTP/1.1\r\n"
        "Content-Type: text\r\n"
        "Content-Length: 8\r\n"
        "something-large: ##HEADER-VALUE##\r\n"
        "\r\n"
        "##BODY##";

    // Make the header larger than 8kB (together with the already existing header).
    const std::string largeHeaderValue = std::string(8192, 'X');
    request = String::replaceAll(request, "##HEADER-VALUE##", largeHeaderValue);

    // Parse the request.
    ASSERT_ANY_THROW(
        BoostBeastStringReader::parseRequest(request));
}
