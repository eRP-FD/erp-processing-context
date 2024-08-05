/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
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
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>()};
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


TEST_F(UrlArgumentsTest, parseWithBundleLinks)//NOLINT(readability-function-cognitive-complexity)
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

    // add a hidden argument, which we do not expect to end up in the links
    std::vector<std::optional<model::TimePeriod>> timePeriods;
    timePeriods.emplace_back(model::TimePeriod::fromFhirSearchDate(model::Timestamp::now().toGermanDate()));
    arguments.addHiddenSearchArgument(
        SearchArgument{SearchArgument::Prefix::LE, "id", "date", SearchParameter::Type::DateAsUuid, timePeriods, {""}});

    const auto links = arguments.createBundleLinks("base", "/Resource", 50);

    EXPECT_EQ(links.size(), 5);

    ASSERT_EQ(links.count(model::Link::Self), 1);
    ASSERT_EQ(links.count(model::Link::Prev), 1);
    ASSERT_EQ(links.count(model::Link::Next), 1);
    ASSERT_EQ(links.count(model::Link::First), 1);
    ASSERT_EQ(links.count(model::Link::Last), 1);

    EXPECT_EQ(links.find(model::Link::Self)->second, "base/Resource?date=eq2013&name=somebody&_count=3&__offset=13");
    EXPECT_EQ(links.find(model::Link::Prev)->second, "base/Resource?date=eq2013&name=somebody&_count=3&__offset=10");
    EXPECT_EQ(links.find(model::Link::Next)->second, "base/Resource?date=eq2013&name=somebody&_count=3&__offset=16");
    EXPECT_EQ(links.find(model::Link::First)->second, "base/Resource?date=eq2013&name=somebody&_count=3&__offset=0");
    EXPECT_EQ(links.find(model::Link::Last)->second, "base/Resource?date=eq2013&name=somebody&_count=3&__offset=48");
}


TEST_F(UrlArgumentsTest, parseWithIdBundleLinks)//NOLINT(readability-function-cognitive-complexity)
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

    // add a hidden argument, which we do not expect to end up in the links
    std::vector<std::optional<model::TimePeriod>> timePeriods;
    timePeriods.emplace_back(model::TimePeriod::fromFhirSearchDate(model::Timestamp::now().toGermanDate()));
    arguments.addHiddenSearchArgument(
        SearchArgument{SearchArgument::Prefix::LE, "id", "date", SearchParameter::Type::DateAsUuid, timePeriods, {""}});

    const auto earlierTimestamp = model::Timestamp::fromXsDateTime("2023-02-15T01:00:00+01:00");
    const auto laterTimestamp = model::Timestamp::fromXsDateTime("2023-02-16T01:00:00+01:00");

    {
        arguments.setResultDateRange(earlierTimestamp, laterTimestamp);
        const auto links = arguments.createBundleLinks(true, "base", "/Resource", UrlArguments::LinkMode::id);

        EXPECT_EQ(links.size(), 4);
        ASSERT_EQ(links.count(model::Link::Self), 1);
        ASSERT_EQ(links.count(model::Link::Prev), 1);
        ASSERT_EQ(links.count(model::Link::Next), 1);
        ASSERT_EQ(links.count(model::Link::First), 1);

        // self link returns the span of dates we found
        EXPECT_EQ(links.find(model::Link::Self)->second, "base/Resource?date=eq2013&name=somebody&_count=3&_id=ge" +
                                                             earlierTimestamp.toDatabaseSUuid() + "&_id=le" +
                                                             laterTimestamp.toDatabaseSUuid());
        // previous page expects older dates
        EXPECT_EQ(links.find(model::Link::Prev)->second,
                  "base/Resource?date=eq2013&name=somebody&_count=3&_id=lt" + earlierTimestamp.toDatabaseSUuid());
        // next page expects newer dates
        EXPECT_EQ(links.find(model::Link::Next)->second,
                  "base/Resource?date=eq2013&name=somebody&_count=3&_id=gt" + laterTimestamp.toDatabaseSUuid());
        // first page expects no _id value
        EXPECT_EQ(links.find(model::Link::First)->second,
                  "base/Resource?date=eq2013&name=somebody&_count=3");
    }

    // reverse the result list
    {
        arguments.setResultDateRange(laterTimestamp, earlierTimestamp);
        const auto links = arguments.createBundleLinks(true, "base", "/Resource", UrlArguments::LinkMode::id);
        EXPECT_EQ(links.find(model::Link::Self)->second, "base/Resource?date=eq2013&name=somebody&_count=3&_id=le" +
                                                             laterTimestamp.toDatabaseSUuid() + "&_id=ge" +
                                                             earlierTimestamp.toDatabaseSUuid());
        EXPECT_EQ(links.find(model::Link::Prev)->second,
                  "base/Resource?date=eq2013&name=somebody&_count=3&_id=gt" + laterTimestamp.toDatabaseSUuid());
        EXPECT_EQ(links.find(model::Link::Next)->second,
                  "base/Resource?date=eq2013&name=somebody&_count=3&_id=lt" + earlierTimestamp.toDatabaseSUuid());
    }
}


TEST_F(UrlArgumentsTest, parseDisallowOffsetAndIdPaging)
{
    UrlArguments arguments({
        {"date", SearchParameter::Type::Date},
        {"name", SearchParameter::Type::String},
    });

    auto request = ServerRequest(Header());
    request.setQueryParameters({{"_count", "3"},
                                {"date", "2013"},
                                {"name", "somebody"},
                                {"__offset", "13"},
                                {"_id", "lt01eba808-de93-7ea8-0000-000000000000"}});
    ASSERT_ANY_THROW(arguments.parse(request, mKeyDerivation));
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
