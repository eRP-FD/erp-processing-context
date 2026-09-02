/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/network/client/response/ClientResponse.hxx"

#include <gtest/gtest.h>
#include <stdexcept>

class ClientResponseTest : public testing::Test
{
};

TEST_F(ClientResponseTest, test1)
{
    EXPECT_NO_THROW(ClientResponse(Header{}, ""));

    EXPECT_NO_THROW(
        ClientResponse(Header{HttpMethod::UNKNOWN, "/", 1, {{Header::ContentLength, "5"}}, HttpStatus::OK}, "Hello"));

    EXPECT_NO_THROW(
        ClientResponse(Header{HttpMethod::UNKNOWN, "/", 1, {{Header::ContentLength, "50"}}, HttpStatus::OK}, ""));

    EXPECT_THROW(
        ClientResponse(Header{HttpMethod::UNKNOWN, "/", 1, {{Header::ContentLength, "5"}}, HttpStatus::OK}, "Hello!"),
        std::runtime_error);

    EXPECT_THROW(
        ClientResponse(Header{HttpMethod::GET, "/", 1, {{Header::ContentLength, "5"}}, HttpStatus::OK}, "Hello"),
        std::logic_error);

    EXPECT_THROW(
        ClientResponse(Header{HttpMethod::UNKNOWN, "/", 1, {{Header::ContentLength, "-"}}, HttpStatus::OK}, "Hello"),
        std::invalid_argument);

    EXPECT_THROW(
        ClientResponse(Header{HttpMethod::UNKNOWN, "/", 1, {{Header::ContentLength, "0"}}, HttpStatus::OK}, "Hello"),
        std::runtime_error);
}
