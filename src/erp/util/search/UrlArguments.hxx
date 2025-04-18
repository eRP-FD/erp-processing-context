/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_URLARGUMENTS_HXX
#define ERP_PROCESSING_CONTEXT_URLARGUMENTS_HXX


#include "shared/model/Link.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/server/request/ServerRequest.hxx"
#include "erp/util/search/PagingArgument.hxx"
#include "erp/util/search/SearchArgument.hxx"
#include "erp/util/search/SearchParameter.hxx"
#include "erp/util/search/SortArgument.hxx"

#include <pqxx/connection>

class KeyDerivation;

/**
 * A set of search, sort or paging arguments. Extracted from a URL based on a set of supported parameters.
 *
 * The intended use case is this:
 * - create a new object with a list of supported parameters. This set depends on the endpoint.
 * - parse a URL to extract arguments and split them into search, sort and paging arguments. This extraction
 *   keeps the relative ordering inside each group of arguments but not between groups.
 * - create an SQL WHERE expression for search parameters
 * - create an SQL SORT expression for sort parameters
 * - create a (part of a) self link with all recognized arguments so that the caller knows which arguments where
 *   ignored.
 */
class UrlArguments
{
public:
    UrlArguments (std::vector<SearchParameter>&& searchParameters, const std::string& defaultSortArgument = {});

    void parse(const ServerRequest& request, const KeyDerivation& keyDerivation);
    void parse(const std::vector<std::pair<std::string, std::string>>& queryParameters, const KeyDerivation& keyDerivation);

    enum class LinkMode
    {
        offset,
        id,
    };

    /**
     * Return a string that can be appended to a link prefix that ends in "&".
     * Note that order is maintained from the original URL between arguments of the same type (sorting vs searching vs paging) but not
     * between these groups.
     */
    std::string getLinkPathArguments(const model::Link::Type linkType, LinkMode linkMode = LinkMode::offset) const;

    std::unordered_map<model::Link::Type, std::string> createBundleLinks (
        const std::string& linkBase,
        const std::string& pathHead,
        const std::size_t& totalSearchMatches);

    std::unordered_map<model::Link::Type, std::string> createBundleLinks(bool hasNextPage, const std::string& linkBase,
                                                                      const std::string& pathHead,
                                                                      LinkMode linkMode = LinkMode::offset) const;

    void setResultDateRange(const model::Timestamp& firstEntry, const model::Timestamp& lastEntry);

    /**
     * Return a string that can be appended to a query that ends in a WHERE clause.
     * Depending on whether search, sort or paging arguments where provided to the `parse()` method, this method
     * will add
     * - expressions to the WHERE clause for the search arguments
     * - an ORDER BY clause for the sort arguments
     * - a LIMIT and OFFSET clause for paging arguments.
     * Note that the later may be subject to change in order to allow for a more performant realisation of paging.
     *
     * If `indentation` is not empty, then a mild form of pretty printing is applied by adding a line break and then
     * the given indentation after individual expressions.
     *
     * Note that the `connection` argument is used only for escaping of some arguments.
     * - Date values do not require escaping because they are are serialized to a string representation from an internal
     *   numerical representation.
     * - String values are currently only used for KVNR and TelematikID values. This can be validated to a level that
     *   rules out an SQL injection attack. But to avoid any risks with possible future uses for other content, all
     *   string values are still escaped. This is done in appendStringComparison, which is the only use for the `connection`
     *   value.
     */
    std::string getSqlExpression(const pqxx::connection& connection, const std::string& indentation,
                                 bool oneAdditionalItemPerPage = false) const;

    std::string getSqlWhereExpression (const pqxx::connection& connection, const std::string& indentation = "") const;
    std::string getSqlSortExpression (void) const;
    std::string getSqlPagingExpression (bool oneAdditionalItem) const;

    bool hasReverseIncludeAuditEventArgument() const;

    const PagingArgument& pagingArgument() const;
    void disablePagingArgument();
    [[nodiscard]] std::optional<SearchArgument> getSearchArgument(const std::string_view& name) const;

    /**
     * Add a search argument that is used to limit the result, but
     * not passed back when requesting links via `createBundleLinks()`
     */
    void addHiddenSearchArgument(SearchArgument arg);

private:
    std::vector<SearchParameter> mSupportedParameters;
    std::vector<SearchArgument> mSearchArguments;
    std::vector<SearchArgument> mHiddenSearchArguments;
    std::vector<SortArgument> mSortArguments;
    PagingArgument mPagingArgument;
    bool mPagingArgument_disabled = false;
    bool mReverseIncludeAuditEventArgument = false;
    std::optional<std::string> mDefaultSortArgument;
    friend class TestUrlArguments;

    /**
     * Split rawValues into parts. If a part is empty, the method throws
     * a ModelException (this happens for strings like ",hi", ",", ",,xyz", etc.
     */
    std::vector<std::string> splitCheckedArgs(const std::string& rawValues);

    void addSearchArguments (const std::string& name, const std::string& rawValues, const KeyDerivation& keyDerivation);
    void addDateSearchArguments(const std::string& name, const std::string& rawValues,
                                const std::string& parameterDbName, SearchParameter::Type parameterType);
    void addStringSearchArguments(const std::string& name, const std::string& rawValues,
                                   const std::string& parameterDbName);
    void addIdentitySearchArguments(const std::string& name, const std::string& rawValues,
                                     const std::string& parameterDbName, const KeyDerivation& keyDerivation);
    void addTaskStatusSearchArguments(const std::string& name, const std::string& rawValues,
                                       const std::string& parameterDbName);
    void addPrescriptionIdSearchArguments(const std::string& name, const std::string& rawValues,
                                          const std::string& parameterDbName);


    /**
     * Add one or more sort arguments.
     * @param arguments is a comma separated list of one or more parameter names, each of which may be prefix by a '-'.
     */
    void addSortArguments (const std::string& arguments);

    void appendComparison(std::ostream& os, const SearchArgument& argument, const pqxx::connection& connection) const;
    void appendDateComparison (std::ostream& os, const SearchArgument& argument, const pqxx::connection& connection) const;
    void appendDateComparisonEQ(std::ostream& os, const SearchArgument& argument) const;
    void appendDateComparisonNE(std::ostream& os, const SearchArgument& argument) const;
    void appendDateComparisonGTSA(std::ostream& os, const SearchArgument& argument) const;
    void appendDateComparisonGE(std::ostream& os, const SearchArgument& argument) const;
    void appendDateComparisonLTEB(std::ostream& os, const SearchArgument& argument) const;
    void appendDateComparisonLE(std::ostream& os, const SearchArgument& argument) const;
    void appendStringComparison (std::ostream& os, const SearchArgument& argument, const pqxx::connection& connection) const;
    void appendIdentityComparison (std::ostream& os, const SearchArgument& argument) const;
    void appendEnumComparison (std::ostream& os, const SearchArgument& argument, const pqxx::connection& connection) const;
    void appendPrescriptionIdComparison (std::ostream& os, const SearchArgument& argument) const;

    void appendLinkSearchArguments (std::ostream& os) const;
    void appendLinkSortArguments (std::ostream& os) const;
    void appendLinkPagingArguments (std::ostream& os, const model::Link::Type type, LinkMode linkMode) const;
    void appendLinkPagingArgumentsWithId(std::ostream& os, const model::Link::Type type) const;
    void appendLinkPagingArgumentsWithOffset(std::ostream& os, const model::Link::Type type) const;
    void appendLinkSeparator (std::ostream& os) const;

    std::optional<SearchParameter::Type> getParameterType (const std::string& argumentName) const;
    std::optional<std::string> getParameterDbName(const std::string& argumentName) const;
    std::optional<std::string> getParameterDbValue(const std::string& argumentName, const std::string_view& rawValue) const;
};


#endif
