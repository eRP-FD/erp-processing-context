/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/tee/OuterTeeRequest.hxx"

#include <gtest/gtest.h>


class OuterTeeRequestTest : public testing::Test
{
public:
    std::string uint8ArrayToString (const uint8_t* data, const size_t length)
    {
        return std::string(reinterpret_cast<const char*>(data), length);
    }
};


TEST_F(OuterTeeRequestTest, assemble)
{
    OuterTeeRequest message;
    message.version = 1;
    memcpy(message.xComponent, "x component of the ephemeral key", 32);
    memcpy(message.yComponent, "y component of the ephemeral key", 32);
    memcpy(message.iv, "here  the iv", 12);
    message.ciphertext = "this is the original request, usually encrypted";
    memcpy(message.authenticationTag, "here the authtag", 16);

    const std::string outerBody = message.assemble();

    ASSERT_EQ(outerBody, "\1x component of the ephemeral keyy component of the ephemeral key"
                         "here  the ivthis is the original request, usually encryptedhere the authtag");
}


TEST_F(OuterTeeRequestTest, disassemble)
{
    const OuterTeeRequest message = OuterTeeRequest::disassemble(
        "\1x component of the ephemeral keyy component of the ephemeral key"
        "here  the ivthis is the original request, usually encryptedhere the authtag");

    ASSERT_EQ(message.version, 1u);
    ASSERT_EQ(uint8ArrayToString(message.xComponent, 32), "x component of the ephemeral key");
    ASSERT_EQ(uint8ArrayToString(message.yComponent, 32), "y component of the ephemeral key");
    ASSERT_EQ(uint8ArrayToString(message.iv, 12), "here  the iv");
    ASSERT_EQ(message.ciphertext, "this is the original request, usually encrypted");
    ASSERT_EQ(uint8ArrayToString(message.authenticationTag, 16), "here the authtag");
}

