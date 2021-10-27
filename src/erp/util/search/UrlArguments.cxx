/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/search/UrlArguments.hxx"

#include "erp/hsm/KeyDerivation.hxx"
#include "erp/model/Task.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/TLog.hxx"

#include <date/date.h>
#include <pqxx/nontransaction>


UrlArguments::UrlArguments (std::vector<SearchParameter>&& urlArguments)
    : mSupportedParameters(std::move(urlArguments)),
      mSearchArguments(),
      mSortArguments(),
      mPagingArgument()
{
}


void UrlArguments::parse (const ServerRequest& request, const KeyDerivation& keyDerivation)
{
    for (const auto& entry : request.getQueryParameters())
    {
        ErpExpect( ! entry.first.empty(), HttpStatus::BadRequest, "empty arguments are not permitted");
        if (entry.first[0] == '_')
        {
            if (entry.first == SortArgument::sortKey) // "_sort"
                addSortArguments(entry.second);
            else if (entry.first == PagingArgument::countKey) // "_count"
                mPagingArgument.setCount(entry.second);
            else if (entry.first == PagingArgument::offsetKey) // "__offset"
                mPagingArgument.setOffset(entry.second);
            else if (entry.first == ReverseIncludeArgument::revIncludeKey &&
                     entry.second ==
                         ReverseIncludeArgument::revIncludeAuditEventKey)// "_revinclude=AuditEvent:entity.what"
                mReverseIncludeAuditEventArgument = true;
        }
        else
        {
            try
            {
                addSearchArguments(entry.first, entry.second, keyDerivation);
            }
            catch (const model::ModelException& exc)
            {
                TVLOG(1) << "ModelException: " << exc.what();
                ErpFail(HttpStatus::BadRequest,
                        "Unable to add search argument " + entry.first);
            }
        }
    }
}


void UrlArguments::addSearchArguments (const std::string& name,
                                       const std::string& rawValues,
                                       const KeyDerivation& keyDerivation)
{
    const auto parameterType = getParameterType(name);
    if ( ! parameterType.has_value())
    {
        // The argument name is not that of a supported parameter. Therefore the argument is ignored according to FHIR
        // specification.
        TVLOG(1) << "ignoring unsupported parameter " << name << '=' << rawValues;
        return;
    }
    const auto parameterDbName = getParameterDbName(name);
    Expect3(parameterDbName.has_value(), "at this point the name lookup cannot fail", std::logic_error);

    switch (parameterType.value())
    {
        case SearchParameter::Type::Date:
            addDateSearchArguments(name, rawValues, *parameterDbName);
            return;
        case SearchParameter::Type::String:
            addStringSearchArguments(name, rawValues, *parameterDbName);
            return;
        case SearchParameter::Type::HashedIdentity:
            addIdentitySearchArguments(name, rawValues, *parameterDbName, keyDerivation);
            return;
        case SearchParameter::Type::TaskStatus:
            addTaskStatusSearchArguments(name, rawValues, * parameterDbName);
            return;
    }
    ErpFail(HttpStatus::InternalServerError, "check the switch-case above for missing return statement");
}

void UrlArguments::addDateSearchArguments(const std::string& name, const std::string& rawValues,
                                          const std::string& parameterDbName)
{
    const auto [prefix, values] = SearchArgument::splitPrefixFromValues(rawValues, SearchParameter::Type::Date);
    if (!values.empty())
    {
        std::vector<std::optional<model::TimePeriod>> timePeriods;
        const std::vector<std::string> strlstRawValues = String::split(values, ',');
        for( const auto& rawValue : strlstRawValues)
        {
            if (!rawValue.empty())
            {
                if (rawValue == "NULL")
                {
                    timePeriods.emplace_back(std::optional<model::TimePeriod>());
                }
                else
                {
                    timePeriods.emplace_back(model::TimePeriod::fromFhirSearchDate(rawValue));
                }
            }
            // else missing rawValue is ignored.
        }
        mSearchArguments.emplace_back(
            prefix, parameterDbName, name,
            SearchParameter::Type::Date, timePeriods, strlstRawValues);
    }
    // else missing rawValue is ignored.
}

void UrlArguments::addStringSearchArguments(const std::string& name, const std::string& rawValues,
                                            const std::string& parameterDbName)
{
    if (!rawValues.empty())
    {
        std::vector<std::string> dbValues;
        std::vector<std::string> originalValues;
        const std::vector<std::string> strlstRawValues = String::split(rawValues, ',');
        for (const auto& rawValue : strlstRawValues)
        {
            if (!rawValue.empty())
            {
                const auto dbValue = getParameterDbValue(name, rawValue);
                dbValues.emplace_back(dbValue.value_or(rawValue));
                originalValues.emplace_back(rawValue);
            }
            // else missing rawValue is ignored.
        }
        mSearchArguments.emplace_back(
            SearchArgument::Prefix::EQ, parameterDbName, name,
            SearchParameter::Type::String, dbValues, originalValues);
    }
    // else missing rawValue is ignored.
}

void UrlArguments::addIdentitySearchArguments(const std::string& name, const std::string& rawValues,
                                              const std::string& parameterDbName, const KeyDerivation& keyDerivation)
{
    if (!rawValues.empty())
    {
        std::vector<db_model::HashedId> dbValues;
        std::vector<std::string> originalValues;
        const std::vector<std::string> strlstRawValues = String::split(rawValues, ',');
        for (const auto& rawValue : strlstRawValues)
        {
            if (!rawValue.empty())
            {
                const auto dbValue = getParameterDbValue(name, rawValue);
                dbValues.emplace_back(keyDerivation.hashIdentity(dbValue.value_or(rawValue)));
                originalValues.emplace_back(rawValue);
            }
            // else missing rawValue is ignored.
        }
        mSearchArguments.emplace_back(
            SearchArgument::Prefix::EQ, parameterDbName, name,
            SearchParameter::Type::HashedIdentity, dbValues, originalValues);
    }
    // else missing rawValue is ignored.
}

void UrlArguments::addTaskStatusSearchArguments(const std::string& name, const std::string& rawValues,
                                                const std::string& parameterDbName)
{
    if (!rawValues.empty())
    {
        std::vector<model::Task::Status> dbValues;
        std::vector<std::string> originalValues;
        const std::vector<std::string> strlstRawValues = String::split(rawValues, ',');
        for (const auto& rawValue : strlstRawValues)
        {
            if (!rawValue.empty())
            {
                auto candidate = model::Task::StatusNamesReverse.find(rawValue);
                if (candidate != model::Task::StatusNamesReverse.end())
                {
                    dbValues.emplace_back(candidate->second);
                    originalValues.emplace_back(rawValue);
                }
                else
                    ErpFail(HttpStatus::BadRequest, "Invalid value for status search parameter: " + rawValue);
            }
            // else missing rawValue is ignored.
        }
        mSearchArguments.emplace_back(
            SearchArgument::Prefix::EQ, parameterDbName, name,
            SearchParameter::Type::TaskStatus, dbValues, originalValues);
    }
    // else missing rawValue is ignored.
}

std::optional<SearchParameter::Type> UrlArguments::getParameterType (const std::string& argumentName) const
{
    // Look up the parameter by name. With the expected small amount of parameters (<=4) a loop may be the fastest way
    // to do that.
    for (const auto& parameter : mSupportedParameters)
    {
        if (parameter.nameUrl == argumentName)
            return parameter.type;
    }

    return {};
}


std::optional<std::string> UrlArguments::getParameterDbName(const std::string& argumentName) const
{
    for (const auto& parameter : mSupportedParameters)
    {
        if (parameter.nameUrl == argumentName)
            return parameter.nameDb;
    }
    return {};
}


std::optional<std::string> UrlArguments::getParameterDbValue(const std::string& argumentName, const std::string_view& rawValue) const
{
    for (const auto& parameter : mSupportedParameters)
    {
        if (parameter.nameUrl == argumentName && parameter.searchToDbValue.has_value()) {
            return std::string(parameter.searchToDbValue.value()(rawValue));
        }
    }
    return {};
}


void UrlArguments::addSortArguments (const std::string& argumentsString)
{
    const auto arguments = String::split(argumentsString, SortArgument::argumentSeparator);
    for (const auto& argument : arguments)
    {
        const auto trimmedArgument = String::trim(argument);
        if ( ! trimmedArgument.empty())
        {
            auto sortArgument = SortArgument(trimmedArgument);
            if (getParameterType(sortArgument.nameUrl).has_value())
            {
                const auto parameterDbName = getParameterDbName(sortArgument.nameUrl);
                sortArgument.nameDb = *parameterDbName;
                mSortArguments.emplace_back(sortArgument);
            }
            // else ignore empty arguments or arguments that are not valid search parameters.
        }
    }
}


std::string UrlArguments::getLinkPathArguments (const model::Link::Type linkType) const
{
    std::ostringstream s;

    appendLinkSearchArguments(s);
    appendLinkSortArguments(s);
    appendLinkPagingArguments(s, linkType);

    return s.str();
}


void UrlArguments::appendLinkSearchArguments (std::ostream& os) const
{
    for (const auto& argument : mSearchArguments)
    {
        appendLinkSeparator(os);
        argument.appendLinkString(os);
    }
}


void UrlArguments::appendLinkSortArguments (std::ostream& os) const
{
    if ( ! mSortArguments.empty())
    {
        appendLinkSeparator(os);
        os << SortArgument::sortKey << '=';
        bool first = true;
        for (const auto& argument : mSortArguments)
        {
            if (first)
                first = false;
            else
                os << SortArgument::argumentSeparator;
            argument.appendLinkString(os);
        }
    }
}


void UrlArguments::appendLinkPagingArguments (
    std::ostream& os,
    const model::Link::Type type) const
{
    switch(type)
    {
        case model::Link::Self:
            if (mPagingArgument.isSet())
            {
                appendLinkSeparator(os);
                os << PagingArgument::countKey << "=" << mPagingArgument.getCount()
                   << '&'
                   << PagingArgument::offsetKey << '=' << mPagingArgument.getOffset();
            }
            break;

        case model::Link::Prev:
            if (mPagingArgument.hasPreviousPage())
            {
                appendLinkSeparator(os);
                // Note that the following may produce a page that overlaps with the current one.
                // The reason for that is that the page size, which originally may have been provided
                // by the client, is to be preserved. And in order to avoid negative offsets, the offset is
                // capped at the lower end to 0.
                os << PagingArgument::countKey << "=" << mPagingArgument.getCount()
                   << '&'
                   << PagingArgument::offsetKey << '=' << std::max(size_t(0), mPagingArgument.getOffset() - mPagingArgument.getCount());
            }
            // else we are already at the first page.
            break;

        case model::Link::Next:
            if (mPagingArgument.hasNextPage())
            {
                appendLinkSeparator(os);
                os << PagingArgument::countKey << "=" << mPagingArgument.getCount()
                   << '&'
                   << PagingArgument::offsetKey << '=' << mPagingArgument.getOffset() + mPagingArgument.getCount();
            }
            // else we are already at the last page.
            break;
    }
}


std::unordered_map<model::Link::Type, std::string> UrlArguments::getBundleLinks (
    const std::string& linkBase,
    const std::string& pathHead) const
{
    std::unordered_map<model::Link::Type, std::string> links;

    links.emplace(model::Link::Type::Self, linkBase + pathHead + getLinkPathArguments(model::Link::Type::Self));

    if (mPagingArgument.hasPreviousPage())
        links.emplace(model::Link::Type::Prev, linkBase + pathHead + getLinkPathArguments(model::Link::Type::Prev));

    if (mPagingArgument.hasNextPage())
        links.emplace(model::Link::Type::Next, linkBase + pathHead + getLinkPathArguments(model::Link::Type::Next));

    return links;
}


void UrlArguments::appendLinkSeparator (std::ostream& os) const
{
    if (os.tellp() == 0)
        os << '?';
    else
        os << '&';
}


std::string UrlArguments::getSqlExpression (const pqxx::connection& connection, const std::string& indentation) const
{
    std::string queryTail;

    const std::string where = getSqlWhereExpression(connection, indentation);
    if ( ! where.empty())
    {
        if (indentation.empty())
            queryTail += " AND " + where;
        else
            queryTail += "\n" + indentation + "AND " + where;
    }

    const std::string order = getSqlSortExpression();
    if ( ! order.empty())
    {
        if (indentation.empty())
            queryTail += " " + order;
        else
            queryTail += "\n" + indentation + order;
    }

    const std::string paging = getSqlPagingExpression();
    if ( ! paging.empty())
    {
        if (indentation.empty())
            queryTail += " " + paging;
        else
            queryTail += "\n" + indentation + paging;
    }

    return queryTail;
}


std::string UrlArguments::getSqlWhereExpression (const pqxx::connection& connection, const std::string& indentation) const
{
    std::ostringstream s;

    for (const auto& argument : mSearchArguments)
    {
        if (s.tellp() > 0)
        {
            if (indentation.empty())
                s << " AND ";
            else
                s << '\n' << indentation << "AND ";
        }
        if (argument.valuesCount() > 1)
        {
            s << "(";
        }

        appendComparison(s, argument, connection);

        if (argument.valuesCount() > 1)
        {
            s << ")";
        }
    }

    return s.str();
}


std::string UrlArguments::getSqlSortExpression (void) const
{
    std::ostringstream s;

    for (const auto& argument : mSortArguments)
    {
        if (s.tellp() > 0)
            s << ", ";

        s << argument.nameDb;
        if (argument.order == SortArgument::Order::Increasing)
            s << " ASC";
        else
            s << " DESC";
    }

    if (s.tellp() > 0)
        return "ORDER BY " + s.str();
    else
        return "";
}


std::string UrlArguments::getSqlPagingExpression (void) const
{
    return "LIMIT " + std::to_string(mPagingArgument.getCount())
        + " OFFSET " + std::to_string(mPagingArgument.getOffset());
}


void UrlArguments::appendComparison(std::ostream& os, const SearchArgument& argument, const pqxx::connection& connection) const
{
    switch (argument.type)
    {
        case SearchParameter::Type::Date:
            appendDateComparison(os, argument, connection);
            break;
        case SearchParameter::Type::String:
            appendStringComparison(os, argument, connection);
            break;
        case SearchParameter::Type::TaskStatus:
            appendEnumComparison(os, argument, connection);
            break;
        case SearchParameter::Type::HashedIdentity:
            appendIdentityComparison(os, argument);
            break;
        default:
            ErpFail(HttpStatus::InternalServerError,
                "has a new SearchParameter::Type value been added?");
    }
}

void UrlArguments::appendDateComparison (
    std::ostream& os,
    const SearchArgument &argument,
    const pqxx::connection &) const
{
    // Regarding quoting/escaping of search parameters.
    // 1. The argument name is compared against a list of supported search parameters. Arguments without perfect match
    //    with one of the supported names are not included in the search. Therefore the argument names used in this function
    //    can be regarded as constants.
    // 2. The argument values in this function represent dates and times and are temporarily converted into TimePeriod objects.
    //    That representation is basically an integer. The textual representation that is included in the returned WHERE
    //    expression is created from that integer value and therefore does not contain any characters that would
    //    require escaping.
    // For these reasons escaping is not required and quoting, by surrounding date values with single quotes, is done manually.

    // The following implementation assumes that target values are points in times, without a range. This is contrary to
    // what https://www.hl7.org/fhir/search.html#prefix describes. The reason for this assumption is that all date
    // values which may be subjected to searching and/or ordering are created by the processing context with a precision
    // that is chosen by the processing context. With sub-second precision and the use cases in the eRp it appears
    // unnecessary to represent a date as a one-millisecond long interval.
    // See also https://dth01.ibmgcloud.net/jira/browse/ERP-4246

    // The short notations below use T for the target value (timestamp value in the database). B for the inclusive lower
    // bound of the search interval (begin). E for the exclusive upper bound of the search interval (end).

    // Below you will find copies of the interpretation of the prefixes for ranges. Taken from https://www.hl7.org/fhir/search.html#prefix.
    // Prefixed with FHIR

    // Split into separated methods to avoid SonarQube warning.
    switch (argument.prefix)
    {
        case SearchArgument::Prefix::EQ:
            appendDateComparisonEQ(os, argument);
            break;
        case SearchArgument::Prefix::NE:
            appendDateComparisonNE(os, argument);
            break;
        case SearchArgument::Prefix::GT:
        case SearchArgument::Prefix::SA:
            appendDateComparisonGTSA(os, argument);
            break;
        case SearchArgument::Prefix::GE:
            appendDateComparisonGE(os, argument);
            break;
        case SearchArgument::Prefix::LT:
        case SearchArgument::Prefix::EB:
            appendDateComparisonLTEB(os, argument);
            break;
        case SearchArgument::Prefix::LE:
            appendDateComparisonLE(os, argument);
            break;
        default:
            ErpFail(HttpStatus::InternalServerError, "unsupported SearchArgument::Prefix");
    }
}

void UrlArguments::appendDateComparisonEQ(std::ostream& os, const SearchArgument& argument) const
{
    for (size_t idx = 0; idx < argument.valuesCount(); ++idx)
    {
        if (idx > 0)
        {
            os << " OR ";
        }

        const auto& value = argument.valueAsTimePeriod(idx);

        // FHIR: The range of the search value fully contains the range of the target value.
        // B <= T < E
        if (!value.has_value())
        {
            os << "(" << argument.name << " IS NULL)";
        }
        else
        {
            os << "(('"
                << value->begin().toXsDateTimeWithoutFractionalSeconds() << "' <= " << argument.name
                << ") AND ("
                << argument.name << " < '" << value->end().toXsDateTimeWithoutFractionalSeconds()
                << "'))";
        }
    }
}

void UrlArguments::appendDateComparisonNE(std::ostream& os, const SearchArgument& argument) const
{
    for (size_t idx = 0; idx < argument.valuesCount(); ++idx)
    {
        if (idx > 0)
        {
            os << " OR ";
        }

        const auto& value = argument.valueAsTimePeriod(idx);

        // FHIR: The range of the search value does not fully contain the range of the target value.
        // T < B || E <= T
        if (!value.has_value())
        {
            os << "(" << argument.name << " IS NOT NULL)";
        }
        else
        {
            os << "(('"
                << value->begin().toXsDateTimeWithoutFractionalSeconds() << "' > " << argument.name
                << ") OR ("
                << argument.name << " >= '" << value->end().toXsDateTimeWithoutFractionalSeconds()
                << "'))";
        }
    }
}

void UrlArguments::appendDateComparisonGTSA(std::ostream& os, const SearchArgument& argument) const
{
    for (size_t idx = 0; idx < argument.valuesCount(); ++idx)
    {
        if (idx > 0)
        {
            os << " OR ";
        }

        const auto& value = argument.valueAsTimePeriod(idx);

        ErpExpect(value.has_value(), HttpStatus::BadRequest, "unsupported SearchArgument::Prefix");

        // FHIR: The range above the search value intersects (i.e. overlaps) with the range of the target value.
        // T >= E
        // >= instead of > because the upper bound is exclusive
        // When target values are time points then SA becomes equivalent to GT.
        os << "("
            << argument.name << " >= '" << value->end().toXsDateTimeWithoutFractionalSeconds()
            << "')";
    }
}

void UrlArguments::appendDateComparisonGE(std::ostream& os, const SearchArgument& argument) const
{
    for (size_t idx = 0; idx < argument.valuesCount(); ++idx)
    {
        if (idx > 0)
        {
            os << " OR ";
        }

        const auto& value = argument.valueAsTimePeriod(idx);

        ErpExpect(value.has_value(), HttpStatus::BadRequest, "unsupported SearchArgument::Prefix");

        // The range above the search value intersects (i.e. overlaps) with the range of the target value, or the range of the search value fully contains the range of the target value.
        // T >= B
        os << "("
            << argument.name << " >= '" << value->begin().toXsDateTimeWithoutFractionalSeconds()
            << "')";
    }
}

void UrlArguments::appendDateComparisonLTEB(std::ostream& os, const SearchArgument& argument) const
{
    for (size_t idx = 0; idx < argument.valuesCount(); ++idx)
    {
        if (idx > 0)
        {
            os << " OR ";
        }

        const auto& value = argument.valueAsTimePeriod(idx);

        ErpExpect(value.has_value(), HttpStatus::BadRequest, "unsupported SearchArgument::Prefix");

        // FHIR: The range below the search value intersects (i.e. overlaps) with the range of the target value.
        // T < B
        // When target values are time points then EB becomes equivalent to LT.
        os << "("
            << argument.name << " < '" << value->begin().toXsDateTimeWithoutFractionalSeconds()
            << "')";
    }
}

void UrlArguments::appendDateComparisonLE(std::ostream& os, const SearchArgument& argument) const
{
    for (size_t idx = 0; idx < argument.valuesCount(); ++idx)
    {
        if (idx > 0)
        {
            os << " OR ";
        }

        const auto& value = argument.valueAsTimePeriod(idx);

        ErpExpect(value.has_value(), HttpStatus::BadRequest, "unsupported SearchArgument::Prefix");

        // The range below the search value intersects (i.e. overlaps) with the range of the target value or the range of the search value fully contains the range of the target value.
        // T < E
        // < instead of <= because E is exclusive
        os << "("
            << argument.name << " < '" << value->end().toXsDateTimeWithoutFractionalSeconds()
            << "')";
    }
}

void UrlArguments::appendStringComparison (
    std::ostream& os,
    const SearchArgument& argument,
    const pqxx::connection& connection) const
{
    ErpExpect(argument.prefix == SearchArgument::Prefix::EQ, HttpStatus::InternalServerError,
        "search prefixes are not supported for string arguments");

    for (size_t idx = 0; idx < argument.valuesCount(); ++idx)
    {
        if (idx > 0)
        {
            os << " OR ";
        }

        os << "("
           << argument.name << " = '" << connection.esc(argument.valueAsString(idx))
           << "')";
    }
}

void UrlArguments::appendIdentityComparison (
    std::ostream& os,
    const SearchArgument& argument) const
{
    ErpExpect(argument.prefix == SearchArgument::Prefix::EQ, HttpStatus::InternalServerError,
        "search prefixes are not supported for string arguments");

    for (size_t idx = 0; idx < argument.valuesCount(); ++idx)
    {
        if (idx > 0)
        {
            os << " OR ";
        }

        const auto& value = argument.valueAsHashedId(idx);

        os << "("
            << argument.name << " = '\\x" << value.toHex()
            << "')";
    }
}

void UrlArguments::appendEnumComparison(std::ostream& os, const SearchArgument& argument,
                                        const pqxx::connection&) const
{
    ErpExpect(argument.prefix==SearchArgument::Prefix::EQ, HttpStatus::InternalServerError,
        "search prefixes are not supported for string arguments");

    for (size_t idx = 0; idx < argument.valuesCount(); ++idx)
    {
        if (idx > 0)
        {
            os << " OR ";
        }

        const auto& value = argument.valueAsTaskStatus(idx);

        os << "("
           << argument.name << " = " << static_cast<int>(model::Task::toNumericalStatus(value))
           << ")";
    }
}

bool UrlArguments::hasReverseIncludeAuditEventArgument() const
{
    return mReverseIncludeAuditEventArgument;
}

const PagingArgument& UrlArguments::pagingArgument() const
{
    return mPagingArgument;
}
