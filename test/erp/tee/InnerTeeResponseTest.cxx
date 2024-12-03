/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/message/Header.hxx"
#include "erp/tee/InnerTeeResponse.hxx"
#include "shared/util/ByteHelper.hxx"

#include <gtest/gtest.h>


class InnerTeeResponseTest : public testing::Test
{
};


TEST_F(InnerTeeResponseTest, getA)
{
    Header header;
    header.setStatus(HttpStatus::OK);
    InnerTeeResponse response ("the-request-id", header, "the-body");

    ASSERT_EQ(response.getA(), "1 "
                               + ByteHelper::toHex(std::string_view("the-request-id"))
                               + " HTTP/1.1 200 OK\r\nContent-Length: "
                               + std::to_string(std::string_view("the-body").size())
                               + "\r\n\r\nthe-body");
}


TEST_F(InnerTeeResponseTest, failForInvalidContentLength)
{
    Header header;
    header.setStatus(HttpStatus::NoContent);
    ASSERT_THROW(InnerTeeResponse("the-request-id", header, "the-body"), std::logic_error);
}