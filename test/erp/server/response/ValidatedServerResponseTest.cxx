#include "shared/server/response/ValidatedServerResponse.hxx"

#include <gtest/gtest.h>


class ValidatedServerResponseTest : public testing::Test
{
};


TEST_F(ValidatedServerResponseTest, moveConstructor)
{
    ServerResponse response;
    response.setHeader("key", "value");

    ValidatedServerResponse validatedResponse (std::move(response));

    ASSERT_EQ(validatedResponse.getHeader().header("key"), "value");
    ASSERT_FALSE(response.getHeader().hasHeader("key")); //NOLINT[bugprone-use-after-move,hicpp-invalid-access-moved]
}
