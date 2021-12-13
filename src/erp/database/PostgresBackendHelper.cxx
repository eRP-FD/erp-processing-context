/*
* (C) Copyright IBM Deutschland GmbH 2021
* (C) Copyright IBM Corp. 2021
*/

#include "erp/database/PostgresBackendHelper.hxx"
#include "erp/database/DatabaseModel.hxx"
#include "erp/util/search/UrlArguments.hxx"

namespace PostgresBackendHelper
{

uint64_t executeCountQuery(pqxx::work& transaction, const std::string_view& query, const db_model::Blob& paramValue,
                           const std::optional<UrlArguments>& search, const std::string_view& context)
{
    std::string completeQuery(query);

    // Append an expression to the query for the search arguments, if there are any.
    if (search.has_value())
    {
        const auto whereExpression = search->getSqlWhereExpression(transaction.conn());
        if (! whereExpression.empty())
        {
            completeQuery += " AND ";
            completeQuery += whereExpression;
        }
    }

    TVLOG(2) << completeQuery;
    const pqxx::result result = transaction.exec_params(completeQuery, paramValue.binarystring());

    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.size() == 1, "Expecting one element as result containing count.");
    int64_t count = 0;
    Expect(result.front().at(0).to(count), "Could not retrieve count of" + std::string(context) + " as int64");

    return gsl::narrow<uint64_t>(count);
}

}
