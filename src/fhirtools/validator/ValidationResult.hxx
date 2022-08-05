// (C) Copyright IBM Deutschland GmbH 2022
// (C) Copyright IBM Corp. 2022

#ifndef FHIR_TOOLS_VALIDATIONRESULT_H
#define FHIR_TOOLS_VALIDATIONRESULT_H

#include "fhirtools/repository/FhirConstraint.hxx"
#include "fhirtools/validator/Severity.hxx"

#include <list>
#include <string>
#include <variant>

namespace fhirtools
{
class FhirStructureDefinition;

/**
 * @brief Records Error information for a single issue found during Validation
 */
class ValidationError {
public:

    std::variant<FhirConstraint, std::tuple<Severity, std::string>> reason;
    std::string fieldName;
    const FhirStructureDefinition* profile = nullptr;
    bool operator == (const ValidationError&) const = default;
    Severity severity() const;
};

/**
 * @brief Records a list of ValidationError from Validation
 */
class ValidationResultList
{
public:

    const std::list<ValidationError>& results() const &;
    std::list<ValidationError> results() &&;
    void add(Severity, std::string message, std::string elementFullPath, const FhirStructureDefinition* profile);
    void add(FhirConstraint constraint, std::string elementFullPath, const FhirStructureDefinition* profile);
    void append(ValidationResultList);
    void append(std::list<ValidationError>);
    Severity highestSeverity() const;
    void dumpToLog() const;
    std::string summary(Severity minSeverity = Severity::error) const;

private:
    std::list<ValidationError> mResults;
};

std::ostream& operator << (std::ostream& out, const ValidationError& err);
std::string to_string(const ValidationError& err);

}

#endif// FHIR_TOOLS_VALIDATIONRESULT_H
