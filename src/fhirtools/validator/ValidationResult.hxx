// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
//
// non-exclusively licensed to gematik GmbH

#ifndef FHIR_TOOLS_VALIDATIONRESULT_H
#define FHIR_TOOLS_VALIDATIONRESULT_H

#include "fhirtools/repository/FhirConstraint.hxx"
#include "fhirtools/validator/Severity.hxx"

#include <optional>
#include <set>
#include <string>
#include <variant>


namespace fhirtools
{
class FhirStructureDefinition;
class Element;

/**
 * @brief Records Error information for a single issue found during Validation
 */
class ValidationError
{
public:
    using MessageReason = std::tuple<Severity, std::string>;
    ValidationError(FhirConstraint, std::string inFieldName, const FhirStructureDefinition* inProfile = nullptr);
    ValidationError(MessageReason, std::string inFieldName, const FhirStructureDefinition* inProfile = nullptr);
    std::string fieldName;
    std::variant<FhirConstraint, MessageReason> reason;
    const FhirStructureDefinition* profile = nullptr;

    bool operator==(const ValidationError&) const = default;
    auto operator<=>(const ValidationError&) const = default;
    Severity severity() const;
};

struct ValidationAdditionalInfo {
    enum Kind : uint8_t
    {
        MissingMandatoryElement,
        IllegalElement,
        ConstraintViolation
    };
    Kind kind;
    std::weak_ptr<const Element> element;
    std::optional<std::string> elementFullPath;
    std::string typeId;
    std::optional<FhirConstraint::Key> constraintKey;

    auto operator<=>(const ValidationAdditionalInfo& other) const;
};

/**
 * @brief Records a list of ValidationError from Validation
 */
class ValidationResults
{
public:
    const std::set<ValidationError>& results() const&;
    std::set<ValidationError> results() &&;
    void add(Severity, std::string message, std::string elementFullPath, const FhirStructureDefinition* profile);
    void add(FhirConstraint constraint, std::string elementFullPath, const FhirStructureDefinition* profile);
    void merge(ValidationResults);
    void merge(std::set<ValidationError>);
    Severity highestSeverity() const;
    void dumpToLog() const;
    std::string summary(Severity minSeverity = Severity::error) const;

    void addInfo(ValidationAdditionalInfo::Kind kind, const std::shared_ptr<const Element>& element,
                 const std::optional<std::string>& elementFullPath, const std::string& typeId,
                 const std::optional<FhirConstraint::Key>& constraintKey = std::nullopt);

    // sorted by greater, to ensure order from inner sub-elements to outer.
    using AdditionalInfoSet = std::set<ValidationAdditionalInfo, std::greater<>>;
    const fhirtools::ValidationResults::AdditionalInfoSet& infos() const;

private:
    std::set<ValidationError> mResults;
    // info will only be filled if requested in options
    AdditionalInfoSet mInfo;
};

std::ostream& operator<<(std::ostream& out, const ValidationError& err);
std::string to_string(const ValidationError& err);

}

#endif// FHIR_TOOLS_VALIDATIONRESULT_H
