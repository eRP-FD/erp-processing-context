/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/beast/BoostBeastStringWriter.hxx"

#include "erp/common/Header.hxx"
#include "erp/common/HttpStatus.hxx"

#include <gtest/gtest.h>


class BoostBeastStringWriterTest : public testing::Test
{
};


TEST_F(BoostBeastStringWriterTest, serializeRequest)
{
    // Given.
    const std::string body = "this is the body";
    Header header;
    header.setMethod(HttpMethod::GET);
    header.setTarget("/this/is/a/path");
    header.setVersion(Header::Version_1_0);
    header.addHeaderField("Content-Type", "some/type");
    // Note that the serialization may check the content length. So keep that realisitically.
    header.addHeaderField("Content-Length", std::to_string(body.size()));

    // When header and body are serialized to a string.
    const std::string serializedRequest = BoostBeastStringWriter::serializeRequest(header, body);

    // Then the result has this expected form.
    EXPECT_EQ(serializedRequest,
        "GET /this/is/a/path HTTP/1.0\r\n"
        "Content-Length: 16\r\n"
        "Content-Type: some/type\r\n"
        "\r\n"
        "this is the body");
}


TEST_F(BoostBeastStringWriterTest, serializeResponse)
{
    // Given.
    const std::string body = "this is the body";
    Header header;
    header.setMethod(HttpMethod::POST); // Note that the method is expected to be ignored for a response.
    header.setStatus(HttpStatus::InternalServerError);
    header.addHeaderField("Content-Type", "some/type");
    // Note that the serialization may check the content length. So keep that realistically.
    header.addHeaderField("Content-Length", std::to_string(body.size()));

    // When header and body are serialized to a string.
    const std::string serializedRequest = BoostBeastStringWriter::serializeResponse(header, body);

    // Then the result has this expected form.
    EXPECT_EQ(serializedRequest,
        "HTTP/1.1 500 Internal Server Error\r\n"
        "Content-Length: 16\r\n"
        "Content-Type: some/type\r\n"
        "\r\n"
        "this is the body");
}


TEST_F(BoostBeastStringWriterTest, serializeResponseNonStandardHttpStatus)
{
    // Given.
    const std::string body = "";
    Header header;
    header.setStatus(HttpStatus::BackendCallFailed);

    // When header and body are serialized to a string.
    const std::string serializedRequest = BoostBeastStringWriter::serializeResponse(header, body);

    // Then the result has this expected form.
    EXPECT_EQ(serializedRequest,
        "HTTP/1.1 512 <unknown-status>\r\n"
        "\r\n");
}
