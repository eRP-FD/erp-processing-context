/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/server/request/ServerRequest.hxx"

#include <gtest/gtest.h>
#include <vector>


TEST(ServerRequestTest, PathParameters) // NOLINT
{
    std::vector<std::string> keys = { "key1", "key2", "key3", "key4", "key5", "key6", "key7" };
    std::vector<std::string> values = { "val1", "val2", "val3", "val4", "val5", "val6", "val7" };

    Header header;
    ServerRequest request(std::move(header));
    request.setPathParameters(keys, values);
    ASSERT_EQ(request.getPathParameterCount(), keys.size());
    ASSERT_EQ(request.getPathParameter("unknown"), std::optional<std::string>());

    for(unsigned int i = 1; i <= keys.size(); ++i)
    {
        std::string key = "key" + std::to_string(i);
        std::string expectedVal = "val" + std::to_string(i);
        ASSERT_EQ(request.getPathParameter(key), expectedVal);
    }

    // error cases:
    keys.emplace_back("key8");
    ASSERT_ANY_THROW(request.setPathParameters(keys, values));
    values.emplace_back("val8");
    values.emplace_back("val9");
    ASSERT_ANY_THROW(request.setPathParameters(keys, values));
}

TEST(ServerRequestTest, ContentLenghtTest) // NOLINT
{
    Header header;
    ServerRequest request(std::move(header));
    request.header().setMethod(HttpMethod::GET);

    EXPECT_NO_THROW(request.setBody("body"));
    // should throw, because body was set but method is GET
    EXPECT_ANY_THROW(request.header().checkInvariants());

    // no throw, no content-length field
    request.setBody("");
    EXPECT_NO_THROW(request.header().checkInvariants());
    EXPECT_FALSE(request.header().hasHeader(Header::ContentLength));

    // no throw, content-length field with value 0
    request.setMethod(HttpMethod::POST);
    EXPECT_NO_THROW(request.header().checkInvariants());
    EXPECT_TRUE(request.header().hasHeader(Header::ContentLength));
    EXPECT_EQ(request.header().contentLength(), 0);

    request.setBody("body");
    // no throw, content-length field with value 4
    request.setMethod(HttpMethod::POST);
    EXPECT_NO_THROW(request.header().checkInvariants());
    EXPECT_TRUE(request.header().hasHeader(Header::ContentLength));
    EXPECT_EQ(request.header().contentLength(), 4);
}
