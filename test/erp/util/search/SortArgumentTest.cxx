/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/util/search/UrlArguments.hxx"

#include "shared/hsm/HsmPool.hxx"
#include "shared/hsm/KeyDerivation.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "test/mock/MockBlobDatabase.hxx"

#include <pqxx/transaction>
#include <gtest/gtest.h>

class SortArgumentTest : public testing::Test
{
public:
    void testSyntax (
        const std::vector<std::string>& urlArguments,
        const std::string& expectedUrlResult,
        const std::string& expectedSqlResult)
    {
        auto search = UrlArguments(
            {{"sent",      SearchParameter::Type::Date},
             {"received",  SearchParameter::Type::Date},
             {"sender",    SearchParameter::Type::HashedIdentity},
             {"recipient", SearchParameter::Type::HashedIdentity}
            });
        Header header;
        ServerRequest request(std::move(header));
        ServerRequest::QueryParametersType arguments;
        for (const auto& urlArgument : urlArguments)
            arguments.emplace_back(SortArgument::sortKey, urlArgument);
        request.setQueryParameters(std::move(arguments));
        search.parse(request, mKeyDerivation);

        ASSERT_EQ(search.getLinkPathArguments(model::Link::Type::Self), expectedUrlResult);

        ASSERT_EQ(search.getSqlSortExpression(), expectedSqlResult);
    }
private:
    HsmPool mHsmPool{
        std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(),
                                         MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm)),
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>()};
    KeyDerivation mKeyDerivation{mHsmPool};

};


TEST_F(SortArgumentTest, syntax_multipleArguments)
{
    // It does not matter whether sort arguments are provided in a single "_sort=<values>" argument or several.
    // The result is the same.
    testSyntax({"sender,   sent,   ignored"}, "?_sort=sender,sent", "ORDER BY sender ASC, sent ASC");
    testSyntax({"sender", "sent", "ignored"}, "?_sort=sender,sent", "ORDER BY sender ASC, sent ASC");
}


TEST_F(SortArgumentTest, syntax_ascendingDescending)
{
    testSyntax({"sender,-ignored,-sent"}, "?_sort=sender,-sent", "ORDER BY sender ASC, sent DESC");
}
