#include <pqxx/transaction>
#include <gtest/gtest.h>
#include <erp/database/Database.hxx>

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
             {"sender",    SearchParameter::Type::String},
             {"recipient", SearchParameter::Type::String}
            });
        Header header;
        ServerRequest request(std::move(header));
        ServerRequest::QueryParametersType arguments;
        for (const auto& urlArgument : urlArguments)
            arguments.emplace_back(SortArgument::sortKey, urlArgument);
        request.setQueryParameters(std::move(arguments));
        search.parse(request);

        ASSERT_EQ(search.getSelfLinkPathArguments(), expectedUrlResult);

        ASSERT_EQ(search.getSqlSortExpression(), expectedSqlResult);
    }
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

