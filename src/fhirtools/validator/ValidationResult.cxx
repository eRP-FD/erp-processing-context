// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
//
// non-exclusively licensed to gematik GmbH

#include "fhirtools/validator/ValidationResult.hxx"
#include "erp/util/TLog.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"

#include <magic_enum.hpp>
#include <sstream>

using fhirtools::ValidationError;
using fhirtools::ValidationResults;

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
    if (! err.fieldName.empty())
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

ValidationError::ValidationError(FhirConstraint inConstraint, std::string inFieldName,
                                 const FhirStructureDefinition* inProfile)
    : fieldName{std::move(inFieldName)}
    , reason{std::move(inConstraint)}
    , profile{inProfile}
{
}

ValidationError::ValidationError(MessageReason inReason, std::string inFieldName, const FhirStructureDefinition* inProfile)
    : fieldName{std::move(inFieldName)}
    , reason{std::move(inReason)}
    , profile{inProfile}
{
}

std::set<ValidationError> fhirtools::ValidationResults::results() &&
{
    return std::move(mResults);
}

const std::set<ValidationError>& fhirtools::ValidationResults::results() const&
{
    return mResults;
}

void fhirtools::ValidationResults::add(fhirtools::Severity severity, std::string message, std::string elementFullPath,
                                       const fhirtools::FhirStructureDefinition* profile)
{
#ifdef NDEBUG
    // do not store severites in release builds
    // as merging the results can significantly slow down validation
    if (severity < fhirtools::Severity::warning)
    {
        return;
    }
#endif
    mResults.emplace(std::make_tuple(severity, std::move(message)), std::move(elementFullPath), profile);
}

void fhirtools::ValidationResults::add(FhirConstraint constraint, std::string elementFullPath,
                                       const fhirtools::FhirStructureDefinition* profile)
{
    mResults.emplace(std::move(constraint), std::move(elementFullPath), profile);
}

void fhirtools::ValidationResults::merge(fhirtools::ValidationResults inResults)
{
    merge(std::move(inResults).results());
}

void fhirtools::ValidationResults::merge(std::set<ValidationError> inResults)
{
    mResults.merge(std::move(inResults));
}

void fhirtools::ValidationResults::dumpToLog() const
{
    for (const auto& item : mResults)
    {
        int tLevel{};
        switch (item.severity())
        {
            using enum fhirtools::Severity;
            case debug:
                TVLOG(3) << item;
                tLevel = 3;
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
    struct Visitor {
        Severity operator()(const FhirConstraint& reason)
        {
            return reason.getSeverity();
        }
        Severity operator()(const MessageReason& reason)
        {
            return std::get<0>(reason);
        }
    };
    return std::visit(Visitor{}, reason);
}

fhirtools::Severity fhirtools::ValidationResults::highestSeverity() const
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

std::string fhirtools::ValidationResults::summary(Severity minSeverity) const
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
