#include "erp/client/request/ValidatedClientRequest.hxx"

#include <gtest/gtest.h>


TEST(ClientRequestTest, Construct)
{
    const std::string body = "This is a test body string";
    {
        ClientRequest request(
            Header(HttpMethod::POST, "/target/path", Header::Version_1_1,
                   { {Header::TransferEncoding, "chunked"} },
                   HttpStatus::Processing,  // code will be ignored because of http method;
                   false),
            body);
        ASSERT_TRUE(request.getHeader().hasHeader(Header::ContentLength));
        ASSERT_EQ(request.getHeader().header(Header::ContentLength).value(), std::to_string(body.size()));
        ASSERT_ANY_THROW(ValidatedClientRequest{request});
    }
    {
        ClientRequest request(
            Header(HttpMethod::POST, "/target/path", Header::Version_1_1, {}, HttpStatus::Unknown, false),
            body);
        ASSERT_NO_THROW(ValidatedClientRequest{request});
        request.setHeader(Header::TransferEncoding, "compress");
        ASSERT_TRUE(request.getHeader().hasHeader(Header::ContentLength));
        ASSERT_EQ(request.getHeader().header(Header::ContentLength).value(), std::to_string(body.size()));
        ASSERT_ANY_THROW(ValidatedClientRequest{request});
    }
}
