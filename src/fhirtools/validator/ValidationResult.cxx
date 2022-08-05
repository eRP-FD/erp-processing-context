// (C) Copyright IBM Deutschland GmbH 2022
// (C) Copyright IBM Corp. 2022

#include "fhirtools/validator/ValidationResult.hxx"

#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "erp/util/TLog.hxx"

#include <magic_enum.hpp>
#include <sstream>

using fhirtools::ValidationError;
using fhirtools::ValidationResultList;

std::ostream& fhirtools::operator<<(std::ostream& out, const ValidationError& err)
{
    struct Printer {
        std::string operator()(const FhirConstraint& constraint)
        {
            return std::string{magic_enum::enum_name(constraint.getSeverity())} + ": " + constraint.getKey() + ": " +
                   constraint.getHuman();
        };
        std::string operator()(const std::tuple<Severity, std::string>& message)
        {
            return std::string{magic_enum::enum_name(std::get<Severity>(message))} + ": " +
                   std::get<std::string>(message);
        };
    };
    if (!err.fieldName.empty())
    {
        out << err.fieldName << ": ";
    }
    out << std::visit(Printer{}, err.reason);
    return out;
}

std::string fhirtools::to_string(const ValidationError& err)
{
    std::ostringstream strm;
    strm << err;
    return strm.str();
}

std::list<ValidationError> fhirtools::ValidationResultList::results() &&
{
    return std::move(mResults);
}

const std::list<ValidationError>& fhirtools::ValidationResultList::results() const&
{
    return mResults;
}

void fhirtools::ValidationResultList::add(fhirtools::Severity severity, std::string message,
                                          std::string elementFullPath,
                                          const fhirtools::FhirStructureDefinition* profile)
{
    mResults.emplace_back(
        ValidationError{std::make_tuple(severity, std::move(message)), std::move(elementFullPath), profile});
}

void fhirtools::ValidationResultList::add(fhirtools::FhirConstraint constraint, std::string elementFullPath,
                                          const fhirtools::FhirStructureDefinition* profile)
{
    mResults.emplace_back(ValidationError{std::move(constraint), std::move(elementFullPath), profile});
}

void fhirtools::ValidationResultList::append(fhirtools::ValidationResultList inList)
{
    append(std::move(inList).results());
}

void fhirtools::ValidationResultList::append(std::list<ValidationError> inList)
{
    mResults.splice(mResults.end(), std::move(inList));
}

void fhirtools::ValidationResultList::dumpToLog() const
{
    for (const auto& item : mResults)
    {
        int tLevel{};
        switch (item.severity())
        {
            using enum fhirtools::Severity;
            case debug:
                TVLOG(3) << item;
                tLevel = 2;
                break;
            case info:
                TVLOG(1) << item;
                tLevel = 1;
                break;
            case unslicedWarning:
            case warning:
                TLOG(INFO) << item;
                tLevel = 0;
                break;
            case error:
                TLOG(WARNING) << item;
                tLevel = 0;
        }
        if (item.profile)
        {
            TVLOG(tLevel) << "    from profile: " << item.profile->url() << '|' << item.profile->version();
        }
    }
}

fhirtools::Severity ValidationError::severity() const
{
    if (std::holds_alternative<FhirConstraint>(reason))
    {
        const auto& r = std::get<FhirConstraint>(reason);
        return r.getSeverity();
    }
    else
    {
        const auto& r = std::get<std::tuple<Severity, std::string>>(reason);
        return std::get<0>(r);
    }
}

fhirtools::Severity fhirtools::ValidationResultList::highestSeverity() const
{
    using std::max;
    fhirtools::Severity maxSeverity = Severity::MIN;
    for (const auto& res : mResults)
    {
        maxSeverity = max(maxSeverity, res.severity());
        if (maxSeverity == Severity::MAX)
        {
            return Severity::MAX;
        }
    }
    return maxSeverity;
}

std::string fhirtools::ValidationResultList::summary(Severity minSeverity) const
{
    std::ostringstream oss;
    for (const auto& item : mResults)
    {
        if (item.severity() >= minSeverity)
        {
            oss << item;
            if (item.profile)
            {
                oss << " (from profile: " << item.profile->url() << '|' << item.profile->version() << ")";
            }
            oss << "; ";
        }
    }
    return oss.str();
}
