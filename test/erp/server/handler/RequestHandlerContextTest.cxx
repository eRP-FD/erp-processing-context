/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/server/handler/RequestHandlerContext.hxx"
#include "erp/service/task/TaskHandler.hxx"

#include <gtest/gtest.h>

class RequestHandlerContextTest
    : public ::testing::Test
{
};


TEST_F(RequestHandlerContextTest, createWithoutParameters)
{
    RequestHandlerContext<PcServiceContext> context (
        HttpMethod::GET,
        "some/path",
        std::make_unique<GetTaskHandler>(std::initializer_list<std::string_view>{}));

    ASSERT_EQ(0u, context.pathParameterNames.size());
}


TEST_F(RequestHandlerContextTest, createWithParameters)
{
    RequestHandlerContext<PcServiceContext> context (
        HttpMethod::POST,
        "with/{parameter1}/and/{parameter2}",
        std::make_unique<GetTaskHandler>(std::initializer_list<std::string_view>{}));

    ASSERT_EQ(2u, context.pathParameterNames.size());
    ASSERT_EQ("parameter1", context.pathParameterNames[0]);
    ASSERT_EQ("parameter2", context.pathParameterNames[1]);
}


TEST_F(RequestHandlerContextTest, matches)
{
    RequestHandlerContext<PcServiceContext> context (
        HttpMethod::UNKNOWN,
        "with/{parameter1}/and/{parameter2}",
        std::make_unique<GetTaskHandler>(std::initializer_list<std::string_view>{}));

    auto [matches, values] = context.matches(HttpMethod::UNKNOWN, "with/value1/and/value2");

    ASSERT_TRUE(matches);
    ASSERT_EQ(2u, values.size());
    ASSERT_EQ("value1", values[0]);
    ASSERT_EQ("value2", values[1]);
}


TEST_F(RequestHandlerContextTest, matchesWrongMethod)
{
    RequestHandlerContext<PcServiceContext> context(
        HttpMethod::UNKNOWN,
        "with/{parameter1}/and/{parameter2}",
        std::make_unique<GetTaskHandler>(std::initializer_list<std::string_view>{}));

    auto[matches, values] = context.matches(HttpMethod::GET, "with/value1/and/value2");
    (void)values;
    ASSERT_FALSE(matches);
}


TEST_F(RequestHandlerContextTest, matchesWrongParams)
{
    RequestHandlerContext<PcServiceContext> context(
        HttpMethod::UNKNOWN,
        "with/{parameter1}/and/{parameter2}",
        std::make_unique<GetTaskHandler>(std::initializer_list<std::string_view>{}));

    auto[matches, values] = context.matches(HttpMethod::UNKNOWN, "with/value1");
    (void)values;
    ASSERT_FALSE(matches);
}


TEST_F(RequestHandlerContextTest, matchesNoParams)
{
    RequestHandlerContext<PcServiceContext> context(
        HttpMethod::UNKNOWN,
        "with",
        std::make_unique<GetTaskHandler>(std::initializer_list<std::string_view>{}));

    auto[matches, values] = context.matches(HttpMethod::UNKNOWN, "with");
    ASSERT_TRUE(matches);
    ASSERT_TRUE(values.empty());
}