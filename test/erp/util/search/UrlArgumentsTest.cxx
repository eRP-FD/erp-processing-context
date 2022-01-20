/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/search/UrlArguments.hxx"

#include "erp/hsm/HsmPool.hxx"
#include "erp/hsm/KeyDerivation.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "test/mock/MockBlobDatabase.hxx"

#include <gtest/gtest.h>

class UrlArgumentsTest : public testing::Test
{
protected:
    HsmPool mHsmPool{
        std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(),
                                         MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm)),
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory()};
    KeyDerivation mKeyDerivation{mHsmPool};
};

TEST_F(UrlArgumentsTest, noParse)
{
    UrlArguments arguments (
        {
            {"date", SearchParameter::Type::Date},
            {"name", SearchParameter::Type::String},
        });

    // UrlArguments::parse was not called => empty self link
    ASSERT_EQ(arguments.getLinkPathArguments(model::Link::Type::Self), "");
}


TEST_F(UrlArgumentsTest, parseWithSelfLink_WithoutOffset)
{
    UrlArguments arguments (
        {
            {"date", SearchParameter::Type::Date},
            {"name", SearchParameter::Type::String},
        });

    auto request = ServerRequest(Header());
    request.setQueryParameters({
        {"date", "2013"},
        {"name", "somebody"}});
    arguments.parse(request, mKeyDerivation);

    ASSERT_EQ(arguments.getLinkPathArguments(model::Link::Type::Self), "?date=eq2013&name=somebody");
}



TEST_F(UrlArgumentsTest, parseWithSelfLink_WithOffset)
{
    UrlArguments arguments (
        {
            {"date", SearchParameter::Type::Date},
            {"name", SearchParameter::Type::String},
        });

    auto request = ServerRequest(Header());
    request.setQueryParameters({
        {"date", "2013"},
        {"__offset", "7"},
        {"name", "somebody"}});
    arguments.parse(request, mKeyDerivation);

    ASSERT_EQ(arguments.getLinkPathArguments(model::Link::Type::Self), "?date=eq2013&name=somebody&_count=50&__offset=7");
}



TEST_F(UrlArgumentsTest, parseWithSelfLink_WithOffsetAndCount)
{
    UrlArguments arguments (
        {
            {"date", SearchParameter::Type::Date},
            {"name", SearchParameter::Type::String},
        });

    auto request = ServerRequest(Header());
    request.setQueryParameters({
        {"_count",   "3"},
        {"date",     "2013"},
        {"__offset", "7"},
        {"name",     "somebody"}});
    arguments.parse(request, mKeyDerivation);

    ASSERT_EQ(arguments.getLinkPathArguments(model::Link::Type::Self), "?date=eq2013&name=somebody&_count=3&__offset=7");
}



TEST_F(UrlArgumentsTest, parseWithBundleLinks)
{
    UrlArguments arguments (
        {
            {"date", SearchParameter::Type::Date},
            {"name", SearchParameter::Type::String},
        });

    auto request = ServerRequest(Header());
    request.setQueryParameters({
        {"_count",   "3"},
        {"date",     "2013"},
        {"name",     "somebody"},
        {"__offset", "13"}
        });
    arguments.parse(request, mKeyDerivation);

    const auto links = arguments.getBundleLinks("base", "/Resource", 50);

    EXPECT_EQ(links.size(), 3);

    ASSERT_EQ(links.count(model::Link::Self), 1);
    ASSERT_EQ(links.count(model::Link::Prev), 1);
    ASSERT_EQ(links.count(model::Link::Next), 1);

    EXPECT_EQ(links.find(model::Link::Self)->second, "base/Resource?date=eq2013&name=somebody&_count=3&__offset=13");
    EXPECT_EQ(links.find(model::Link::Prev)->second, "base/Resource?date=eq2013&name=somebody&_count=3&__offset=10");
    EXPECT_EQ(links.find(model::Link::Next)->second, "base/Resource?date=eq2013&name=somebody&_count=3&__offset=16");
}



TEST_F(UrlArgumentsTest, parseWithBundleLinks_withOrder_failForNegativeOffset)
{
    UrlArguments arguments (
        {
            {"date", SearchParameter::Type::Date},
            {"name", SearchParameter::Type::String},
        });

    auto request = ServerRequest(Header());
    request.setQueryParameters({
        {"date",     "2013"},
        {"name",     "somebody"},
        {"__offset", "-13"},
        //            ^ negative offset
        {"_sort",    "-date"}
    });

    EXPECT_ANY_THROW(arguments.parse(request, mKeyDerivation));

}
