// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
//
// non-exclusively licensed to gematik GmbH

#include "fhirtools/validator/ValidationResult.hxx"
#include "shared/util/TLog.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/model/Element.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"

#include <magic_enum/magic_enum.hpp>
#include <sstream>
#include <utility>

using fhirtools::ValidationError;
using fhirtools::ValidationResults;

std::string ValidationError::text() const
{
    struct Printer {
        std::string operator()(const FhirConstraint& constraint)
        {
            std::string result{constraint.getKey()};
            result += ": ";
            result += constraint.getHuman();
            return result;
        };
        std::string operator()(const std::tuple<Severity, std::string>& message)
        {
            return std::get<std::string>(message);
        };
        std::string operator()(const ValidationError::ExtendedValidationFailure& failure)
        {
            switch (std::get<ExtendedValidation>(failure))
            {
                case ExtendedValidation::unslicedExtension:
                    return "element doesn't belong to any slice.";
                case ExtendedValidation::invalidUrnUuidInUri:
                    return "value is not a valid lower-case urn:uuid.";
                case ExtendedValidation::bundleFullUrlMissing:
                    return "missing fullUrl for resource";
                case ExtendedValidation::bundleFullUrlIdMissmatch:
                    return "ID provided in RESTful fullUrl doesn't match resource ID";
                case ExtendedValidation::bundleFullUrlResourceTypeMissmatch:
                    return "Resource type provided in RESTful fullUrl doesn't match type of resource";
                case ExtendedValidation::bundleFullUrlInvalidFormat:
                    return "Invalid format for fullUrl must be FHIR-RESTful or urn:uuid";
                case ExtendedValidation::bundledResourceMissingId:
                    return "Bundled resource has no ID.";
                case ExtendedValidation::unresolveableReferenceInBundle:
                    return "Unresolveable reference in bundle";
            }
            return "unknown ExtendedValidation failure: " +
                   std::to_string(static_cast<uintmax_t>(std::get<ExtendedValidation>(failure)));
        };
    };
    return std::visit(Printer{}, reason);
}

std::ostream& fhirtools::operator<<(std::ostream& out, const ValidationError& err)
{
    if (! err.fieldName.empty())
    {
        out << err.fieldName << ": ";
    }
    out << magic_enum::enum_name(err.severity()) << ": " << err.text();
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

ValidationError::ValidationError(MessageReason inReason, std::string inFieldName,
                                 const FhirStructureDefinition* inProfile)
    : fieldName{std::move(inFieldName)}
    , reason{std::move(inReason)}
    , profile{inProfile}
{
}

fhirtools::ValidationError::ValidationError(ExtendedValidationFailure inFailure, std::string inFieldName,
                                            const FhirStructureDefinition* inProfile)
    : fieldName(std::move(inFieldName))
    , reason(std::move(inFailure))
    , profile(inProfile)
{
}

auto fhirtools::ValidationAdditionalInfo::operator<=>(const fhirtools::ValidationAdditionalInfo& other) const
{
    auto elementShared = element.lock();
    Expect3(elementShared != nullptr, "additional info element is gone", std::logic_error);
    auto elementLevel = elementShared->subElementLevel();
    auto otherElementShared = other.element.lock();
    Expect3(otherElementShared != nullptr, "additional info element is gone", std::logic_error);
    auto otherElementLevel = otherElementShared->subElementLevel();
    return std::tie(kind, elementLevel, elementFullPath, typeId, constraintKey, elementShared) <=>
           std::tie(other.kind, otherElementLevel, elementFullPath, typeId, constraintKey, otherElementShared);
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
                                       const fhirtools::FhirStructureDefinition* profiled)
{
#ifdef NDEBUG
    // do not store severites in release builds
    // as merging the results can significantly slow down validation
    if (severity < fhirtools::Severity::info)
    {
        return;
    }
#endif
    mResults.emplace(std::make_tuple(severity, std::move(message)), std::move(elementFullPath), profiled);
}

void fhirtools::ValidationResults::add(FhirConstraint constraint, std::string elementFullPath,
                                       const fhirtools::FhirStructureDefinition* profile)
{
    mResults.emplace(std::move(constraint), std::move(elementFullPath), profile);
}

void fhirtools::ValidationResults::add(Severity severity, ExtendedValidation extendedValidation,
                                       std::string elementFullPath, const FhirStructureDefinition* profile)
{
#ifdef NDEBUG
    // do not store severites in release builds
    // as merging the results can significantly slow down validation
    if (severity < fhirtools::Severity::info)
    {
        return;
    }
#endif
    mResults.emplace(std::make_tuple(severity, extendedValidation), std::move(elementFullPath), profile);
}

void fhirtools::ValidationResults::merge(fhirtools::ValidationResults inResults)
{
    mInfo.merge(inResults.mInfo);
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
        Severity operator()(const FhirConstraint& r)
        {
            return r.getSeverity();
        }
        Severity operator()(const MessageReason& r)
        {
            return std::get<Severity>(r);
        }
        Severity operator()(const ExtendedValidationFailure& f)
        {
            return std::get<Severity>(f);
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

void fhirtools::ValidationResults::addInfo(fhirtools::ValidationAdditionalInfo::Kind kind,
                                           const std::shared_ptr<const Element>& element,
                                           const std::optional<std::string>& elementFullPath, const std::string& typeId,
                                           const std::optional<FhirConstraint::Key>& constraintKey)
{
    mInfo.emplace(kind, element, elementFullPath, typeId, constraintKey);
}

const fhirtools::ValidationResults::AdditionalInfoSet& fhirtools::ValidationResults::infos() const
{
    return mInfo;
}
