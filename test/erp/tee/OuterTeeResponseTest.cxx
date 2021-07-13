#include <erp/crypto/AesGcm.hxx>
#include <erp/tee/OuterTeeResponse.hxx>
#include <gtest/gtest.h>

#include "erp/hsm/HsmPool.hxx"
#include "mock/hsm/HsmMockFactory.hxx"

class OuterTeeResponseTest : public testing::Test
{
};


TEST_F(OuterTeeResponseTest, convert)
{
    const auto key = SafeString("0123456789abcdef");
    OuterTeeResponse outerResponse ("abc", key);

    ServerResponse response;
    outerResponse.convert(response);

    EXPECT_EQ(response.getHeader(Header::ContentType), MimeType::binary);
    EXPECT_EQ(response.getHeader().status(), HttpStatus::OK);

    // Decrypt the body and verify that it is identical to the original "abc".
    const auto body = response.getBody();
    const auto iv = body.substr(0,12);
    const auto authenticationTag = body.substr(body.size()-16, 16);
    const auto clearText = AesGcm128::decrypt(body.substr(12, body.size()-12-16), key, iv, authenticationTag);
    const std::string_view clearTextString = clearText;
    EXPECT_EQ(clearTextString, "abc");
}
