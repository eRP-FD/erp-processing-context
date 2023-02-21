/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/tee/InnerTeeRequest.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/JwtException.hxx"
#include "test/util/ErpMacros.hxx"

#include <gtest/gtest.h>


class InnerTeeRequestTest : public testing::Test
{
public:
    static std::string uint8ArrayToString (const uint8_t* data, const size_t length)
    {
        return std::string(reinterpret_cast<const char*>(data), length);
    }

    static std::string toHex (const std::string& s)
    {
        return ByteHelper::toHex(s);
    }
};


TEST_F(InnerTeeRequestTest, create_success)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string serializedJwt = JWT( Base64::encode(R"({"alg":"BP256R1"})") + "." + Base64::encode("{}") + "." + Base64::encode("three") ).serialize();
    InnerTeeRequest message (SafeString("1 "
                             + serializedJwt + " "
                             + toHex("request-id") + " "
                             + toHex("aes-key") + " "
                             + "GET /endpoint HTTP/1.1\r\nContent-Type: application/json\r\nContent-Length: 5\r\n\r\nhello"));
    message.parseHeaderAndBody();

    ASSERT_EQ(message.version(), "1");
    ASSERT_EQ(message.authenticationToken().serialize(), serializedJwt);
    ASSERT_EQ(message.requestId(), "request-id");
    ASSERT_EQ(std::string{message.aesKey()}, "aes-key");
    ASSERT_TRUE(message.header().hasHeader(Header::ContentType));
    ASSERT_EQ(message.header().header(Header::ContentType).value(), "application/json");
    ASSERT_TRUE(message.header().hasHeader(Header::ContentLength));
    ASSERT_EQ(message.header().header(Header::ContentLength).value(), "5");
    ASSERT_EQ(message.body(), "hello");
}


TEST_F(InnerTeeRequestTest, create_successWithoutBody)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string serializedJwt = JWT( Base64::encode(R"({"alg":"BP256R1"})") + "." + Base64::encode("{}") + "." + Base64::encode("three") ).serialize();
    InnerTeeRequest message (SafeString("1 "
                             + serializedJwt + " "
                             + toHex("request-id") + " "
                             + toHex("aes-key") + " "
                             + "POST / HTTP/1.0\r\nContent-Type: application/json\r\nContent-Length: 0\r\n\r\n"));
    message.parseHeaderAndBody();

    ASSERT_EQ(message.version(), "1");
    ASSERT_EQ(message.authenticationToken().serialize(), serializedJwt);
    ASSERT_EQ(message.requestId(), "request-id");
    ASSERT_EQ(std::string{message.aesKey()}, "aes-key");
    ASSERT_TRUE(message.header().hasHeader(Header::ContentType));
    ASSERT_EQ(message.header().header(Header::ContentType).value(), "application/json");
    ASSERT_TRUE(message.header().hasHeader(Header::ContentLength));
    ASSERT_EQ(message.header().header(Header::ContentLength).value(), "0");
    ASSERT_EQ(message.body(), "");
}


TEST_F(InnerTeeRequestTest, create_failForMissingHeaderEnd)
{
    const std::string serializedJwt = JWT( Base64::encode(R"({"alg":"BP256R1"})") + "." + Base64::encode("{}") + "." + Base64::encode("three") ).serialize();
    auto innerTeeRequest= InnerTeeRequest (SafeString("1 "
                         + serializedJwt + " "
                         + toHex("request-id") + " "
                         + toHex("aes-key") + " "
                         + "Content-Type: application/json\r\nContent-Length: 5\r\n"));
        // \r\n missing                                                            ^ here
    EXPECT_ERP_EXCEPTION(innerTeeRequest.parseHeaderAndBody(), HttpStatus::BadRequest);
}

TEST_F(InnerTeeRequestTest, create_failInvalidHexToken)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string serializedJwt =
        JWT(Base64::encode(R"({"alg":"BP256R1"})") + "." + Base64::encode("{}") + "." + Base64::encode("three"))
            .serialize();
    EXPECT_ERP_EXCEPTION(
        InnerTeeRequest{
            SafeString("1 " + serializedJwt + " " + toHex("request-id") + "a " + toHex("aes-key") + " " +
                       "GET /endpoint HTTP/1.1\r\nContent-Type: application/json\r\nContent-Length: 5\r\n\r\nhello")},
        HttpStatus::BadRequest);

    EXPECT_ERP_EXCEPTION(
        InnerTeeRequest{
            SafeString("1 " + serializedJwt + " " + toHex("request-id") + " " + "testnonhex " +
                       "GET /endpoint HTTP/1.1\r\nContent-Type: application/json\r\nContent-Length: 5\r\n\r\nhello")},
        HttpStatus::BadRequest);
}

TEST_F(InnerTeeRequestTest, create_failForMissingAccessToken)
{
    // This is structurally wrong, because of the double <<space>>
    EXPECT_ERP_EXCEPTION(InnerTeeRequest(SafeString("1 "
        + std::string(" ")
        + toHex("request-id") + " "
        + toHex("aes-key") + " "
        + "POST / HTTP/1.0\r\nContent-Type: application/json\r\nContent-Length: 0\r\n\r\n")), HttpStatus::BadRequest);
}

TEST_F(InnerTeeRequestTest, create_failForMissingAccessToken1)
{
    // This is structurally right (kind of), but JWT is missing.
    ASSERT_THROW(InnerTeeRequest(SafeString("1 "
        + toHex("request-id") + " "
        + toHex("aes-key") + " "
        + "POST / HTTP/1.0\r\nContent-Type: application/json\r\nContent-Length: 0\r\n\r\n")), JwtInvalidFormatException);
}

TEST_F(InnerTeeRequestTest, create_failForMissingAccessToken2)
{

    EXPECT_ERP_EXCEPTION(InnerTeeRequest(SafeString("1 "
        + toHex("request-id") + " "
        + toHex("aes-key"))), HttpStatus::BadRequest);
}


TEST_F(InnerTeeRequestTest, create_failForInvalidSeparators)//NOLINT(readability-function-cognitive-complexity)
{
    EXPECT_ERP_EXCEPTION(
        InnerTeeRequest(SafeString(" 1 {}.two.three request-id aes-key Content-Type: application/json\r\nContent-Size: 5\r\n\r\nhello"))
    //                              ^
    , HttpStatus::BadRequest);

    EXPECT_ERP_EXCEPTION(
        InnerTeeRequest(SafeString("1  {}.two.three request-id aes-key Content-Type: application/json\r\nContent-Size: 5\r\n\r\nhello"))
    //                                ^
    , HttpStatus::BadRequest);

    EXPECT_ERP_EXCEPTION(
        InnerTeeRequest(SafeString("1 {}.two.three  request-id aes-key Content-Type: application/json\r\nContent-Size: 5\r\n\r\nhello"))
    //                                             ^
    , HttpStatus::BadRequest);

    EXPECT_ERP_EXCEPTION(
        InnerTeeRequest(SafeString("1 {}.two.three request-id  aes-key Content-Type: application/json\r\nContent-Size: 5\r\n\r\nhello"))
    //                                                        ^
    , HttpStatus::BadRequest);

    EXPECT_THROW(
        InnerTeeRequest(SafeString("1 {}.two.three request-id aes-key  Content-Type: application/json\r\nContent-Size: 5\r\n\r\nhello"))
    //                                                                ^
    , JwtInvalidFormatException);
}
