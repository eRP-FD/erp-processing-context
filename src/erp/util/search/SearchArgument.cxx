/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/util/search/SearchArgument.hxx"
#include "erp/util/UrlHelper.hxx"
#include "erp/model/Task.hxx"

#include <boost/algorithm/string/join.hpp>
#include <date/date.h>

#include <utility>


SearchArgument::SearchArgument (
    const Prefix prefix,
    std::string name,
    std::string originalName,
    Type type,
    values_t values,
    std::vector<std::string> originalValues)
    : name(std::move(name)),
      originalName(std::move(originalName)),
      values(std::move(values)),
      originalValues(std::move(originalValues)),
      type(type),
      prefix(prefix)
{
}


std::pair<SearchArgument::Prefix, std::string> SearchArgument::splitPrefixFromValues (
    const std::string& values,
    const Type type)
{
    if (type == Type::Date)
    {
        // Extract the optional prefix from the first two characters of the values.
        if (values.length() >= 2)
        {
            const auto optionalPrefix = prefixFromString(values.substr(0,2));
            if (optionalPrefix.has_value())
                return {optionalPrefix.value(), values.substr(2)};
        }
    }
    // no prefixes for other types

    return {Prefix::EQ, values};
}


std::string SearchArgument::valuesAsString() const
{
    switch (type)
    {
    case Type::Date:
    case Type::DateAsUuid:
        return dateValuesAsString();
    case Type::HashedIdentity:
        return String::concatenateStrings(originalValues, ",");
    case Type::String:
        return String::concatenateStrings(std::get<std::vector<std::string>>(values), ",");
    case Type::TaskStatus:
        return taskStatusAsString();
    case Type::PrescriptionId:
        return prescriptionIdValuesAsString();
    }
    ErpFail(HttpStatus::InternalServerError, "check the switch-case above for missing return statement");
}


size_t SearchArgument::valuesCount() const
{
    switch (type)
    {
    case Type::Date:
    case Type::DateAsUuid:
        return std::get<std::vector<std::optional<model::TimePeriod>>>(values).size();
    case Type::HashedIdentity:
        return std::get<std::vector<db_model::HashedId>>(values).size();
    case Type::String:
        return std::get<std::vector<std::string>>(values).size();
    case Type::TaskStatus:
        return std::get<std::vector<model::Task::Status>>(values).size();
    case Type::PrescriptionId:
        return std::get<std::vector<model::PrescriptionId>>(values).size();
    }
    ErpFail(HttpStatus::InternalServerError, "check the switch-case above for missing return statement");
}

std::string SearchArgument::valueAsString (size_t idx) const
{
    checkValueIndex(idx);

    switch (type)
    {
        case Type::Date:
        case Type::DateAsUuid:
            return dateValueAsString(idx);
        case Type::HashedIdentity:
            return originalValues[idx];
        case Type::String:
        case Type::TaskStatus:
            return std::get<std::vector<std::string>>(values)[idx];
        case Type::PrescriptionId:
            return originalValues[idx];
    }
    ErpFail(HttpStatus::InternalServerError, "check the switch-case above for missing return statement");
}


db_model::HashedId SearchArgument::valueAsHashedId(size_t idx) const
{
    checkValueIndex(idx);

    ErpExpect(type == Type::HashedIdentity, HttpStatus::InternalServerError, "value is not a hashed id");
    return std::get<std::vector<db_model::HashedId>>(values)[idx];
}


std::optional<model::TimePeriod> SearchArgument::valueAsTimePeriod (size_t idx) const
{
    checkValueIndex(idx);

    ErpExpect(type == Type::Date || type == Type::DateAsUuid, HttpStatus::InternalServerError, "value is not a date");
    return std::get<std::vector<std::optional<model::TimePeriod>>>(values)[idx];
}


model::Task::Status SearchArgument::valueAsTaskStatus(size_t idx) const
{
    checkValueIndex(idx);

    ErpExpect(type == Type::TaskStatus, HttpStatus::InternalServerError, "value is not a task status");
    return std::get<std::vector<model::Task::Status>>(values)[idx];
}

model::PrescriptionId SearchArgument::valueAsPrescriptionId(size_t idx) const
{
    checkValueIndex(idx);

    ErpExpect(type == Type::PrescriptionId, HttpStatus::InternalServerError, "value is not a PrescriptionID");
    return std::get<std::vector<model::PrescriptionId>>(values)[idx];
}


std::string SearchArgument::prefixAsString (void) const
{
    return prefixAsString(prefix);
}


std::string SearchArgument::prefixAsString (const Prefix prefix)
{
    using namespace std::string_view_literals;

    switch (prefix)
    {
        case Prefix::EQ: return "eq";
        case Prefix::NE: return "ne";
        case Prefix::GT: return "gt";
        case Prefix::LT: return "lt";
        case Prefix::GE: return "ge";
        case Prefix::LE: return "le";
        case Prefix::SA: return "sa";
        case Prefix::EB: return "eb";
        default:
            ErpFail(HttpStatus::InternalServerError, "new SearchArgument::Prefix value is not supported, please add it");
    }
}


void SearchArgument::appendLinkString (std::ostream& os) const
{
    if (type == Type::Date || type == Type::DateAsUuid)
        os << originalName << '=' << prefixAsString() << UrlHelper::escapeUrl(valuesAsString());
    else
        os << originalName << '=' << UrlHelper::escapeUrl(valuesAsString());
}

void SearchArgument::checkValueIndex(size_t idx) const
{
    ErpExpect(idx < valuesCount(), HttpStatus::InternalServerError, "value index out of range");
}

std::string SearchArgument::dateValuesAsString() const
{
    std::string valuesString;
    const auto& dateValues = std::get<std::vector<std::optional<model::TimePeriod>>>(values);
    for (size_t idx = 0; idx < dateValues.size(); ++idx)
    {
        if (!valuesString.empty())
        {
            valuesString.append(",");
        }
        valuesString.append(dateValueAsString(idx));
    }
    return valuesString;
}

std::string SearchArgument::dateValueAsString(size_t idx) const
{
    checkValueIndex(idx);

    const auto& timestamp = std::get<std::vector<std::optional<model::TimePeriod>>>(values)[idx];
    if (timestamp.has_value())
    {
        switch(model::Timestamp::detectType(originalValues[idx]))
        {
            case model::Timestamp::Type::DateTimeWithFractionalSeconds:
                return timestamp->begin().toXsDateTime();
            case model::Timestamp::Type::DateTime:
            default:
                return timestamp->begin().toXsDateTimeWithoutFractionalSeconds();
            case model::Timestamp::Type::Date:
                return timestamp->begin().toGermanDate();
            case model::Timestamp::Type::YearMonth:
                return timestamp->begin().toXsGYearMonth(model::Timestamp::GermanTimezone);
            case model::Timestamp::Type::Year:
                return timestamp->begin().toXsGYear(model::Timestamp::GermanTimezone);
        }
    }
    else
        return "NULL";
}

std::string SearchArgument::taskStatusAsString() const
{
    std::string valuesString;
    const auto& statusValues = std::get<std::vector<model::Task::Status>>(values);
    for (const auto& taskStatus : statusValues)
    {
        if (!valuesString.empty())
        {
            valuesString.append(",");
        }
        valuesString.append(model::Task::StatusNames.at(taskStatus));
    }
    return valuesString;
}

std::string SearchArgument::prescriptionIdValuesAsString() const
{
    std::string valuesString;
    const auto& prescriptionIdvalues = std::get<std::vector<model::PrescriptionId>>(values);
    for (const auto& item : prescriptionIdvalues)
    {
        if (!valuesString.empty())
        {
            valuesString.append(",");
        }
        valuesString.append(item.toString());
    }
    return valuesString;
}

std::optional<SearchArgument::Prefix> SearchArgument::prefixFromString (const std::string& prefixString)
{
    // Should this be optimized?  With 8 elements a map would probably be slower. A vector or array might work better.
    // Not doing it now, because of likely minimal effect.
    if (prefixString == "eb")
        return Prefix::EB;
    else if (prefixString == "eq")
        return Prefix::EQ;
    else if (prefixString == "ge")
        return Prefix::GE;
    else if (prefixString == "gt")
        return Prefix::GT;
    else if (prefixString == "le")
        return Prefix::LE;
    else if (prefixString == "lt")
        return Prefix::LT;
    else if (prefixString == "ne")
        return Prefix::NE;
    else if (prefixString == "sa")
        return Prefix::SA;
    else
    {
        // Due to what may be a compiler bug in GCC (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80635) the return of
        // an empty optional is a bit longer than usual.
        std::optional<SearchArgument::Prefix> empty;
        return empty;
    }
}
