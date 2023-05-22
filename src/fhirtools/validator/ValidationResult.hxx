// (C) Copyright IBM Deutschland GmbH 2021, 2023
// (C) Copyright IBM Corp. 2021, 2023
//
// non-exclusively licensed to gematik GmbH

#ifndef FHIR_TOOLS_VALIDATIONRESULT_H
#define FHIR_TOOLS_VALIDATIONRESULT_H

#include "fhirtools/repository/FhirConstraint.hxx"
#include "fhirtools/validator/Severity.hxx"

#include <set>
#include <string>
#include <variant>


namespace fhirtools
{
class FhirStructureDefinition;

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
    auto operator<=>(const ValidationError&) const = default; //NOLINT(hicpp-use-nullptr,modernize-use-nullptr)
    Severity severity() const;
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

private:
    std::set<ValidationError> mResults;
};

std::ostream& operator<<(std::ostream& out, const ValidationError& err);
std::string to_string(const ValidationError& err);

}

#endif// FHIR_TOOLS_VALIDATIONRESULT_H
