/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/server/response/ValidatedServerResponse.hxx"

#include <gtest/gtest.h>


namespace
{
    void checkInvalidStatusErrors(ServerResponse&& response) // NOLINT
    {
        {
            ServerResponse copy (response);
            copy.setStatus(HttpStatus::Continue);
            ASSERT_ANY_THROW(ValidatedServerResponse{std::move(copy)});
        }
        {
            ServerResponse copy (response);
            copy.setStatus(HttpStatus::SwitchingProtocols);
            ASSERT_ANY_THROW(ValidatedServerResponse{std::move(copy)});
        }
        {
            ServerResponse copy (response);
            copy.setStatus(HttpStatus::Processing);
            ASSERT_ANY_THROW(ValidatedServerResponse{std::move(copy)});
        }
        {
            ServerResponse copy (response);
            copy.setStatus(HttpStatus::NoContent);
            ASSERT_ANY_THROW(ValidatedServerResponse{std::move(copy)});
        }
    }
}

TEST(ServerResponseTest, Construct) // NOLINT
{
    const std::string body = "This is a test body string";
    {
        ServerResponse response;
        response.setStatus(HttpStatus::OK);
        response.setBody(body);
        ASSERT_TRUE(response.getHeader().hasHeader(Header::ContentLength));
        ASSERT_EQ(response.getHeader().header(Header::ContentLength).value(), std::to_string(body.size()));

        ASSERT_NO_THROW(ValidatedServerResponse{ServerResponse(response)});

        response.setStatus(HttpStatus::Continue);
        ASSERT_ANY_THROW(ValidatedServerResponse{ServerResponse(response)});
    }
    {
        ServerResponse response(Header(HttpStatus::OK), body);
        response.setHeader(Header::TransferEncoding, "compress");
        ASSERT_ANY_THROW(ValidatedServerResponse{ServerResponse(response)});
    }
    {
        ServerResponse response;
        response.setStatus(HttpStatus::OK);
        response.setBody(body);
        checkInvalidStatusErrors(std::move(response));
    }
    {
        ServerResponse response;
        response.setHeader(Header::TransferEncoding, "compress");
        checkInvalidStatusErrors(std::move(response));
    }
}

TEST(ServerResponseTest, ContentLength) // NOLINT
{
    ServerResponse response;

    // no body allowed for NoContent
    response.setStatus(HttpStatus::NoContent);
    EXPECT_NO_THROW(response.setBody("body"));
    EXPECT_ANY_THROW(response.getHeader().checkInvariants());
    EXPECT_NO_THROW(response.setBody(""));
    EXPECT_FALSE(response.getHeader().hasHeader(Header::ContentLength));

    // no body allowed for Status < 200
    response.setStatus(HttpStatus::Processing);
    EXPECT_NO_THROW(response.setBody("body"));
    EXPECT_ANY_THROW(response.getHeader().checkInvariants());
    EXPECT_NO_THROW(response.setBody(""));
    EXPECT_FALSE(response.getHeader().hasHeader(Header::ContentLength));

    response.setStatus(HttpStatus::OK);
    EXPECT_NO_THROW(response.setBody("body"));
    EXPECT_NO_THROW(response.getHeader().checkInvariants());
    EXPECT_TRUE(response.getHeader().hasHeader(Header::ContentLength));
    EXPECT_EQ(response.getHeader().contentLength(), 4);

    EXPECT_NO_THROW(response.setBody(""));
    EXPECT_NO_THROW(response.getHeader().checkInvariants());
    EXPECT_TRUE(response.getHeader().hasHeader(Header::ContentLength));
    EXPECT_EQ(response.getHeader().contentLength(), 0);
}