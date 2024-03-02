// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
//
// non-exclusively licensed to gematik GmbH

#ifndef FHIRTOOLS_INTERNAL_VALIDATOR_VALIDATIONDATA_H
#define FHIRTOOLS_INTERNAL_VALIDATOR_VALIDATIONDATA_H

#include "fhirtools/validator/Severity.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "fhirtools/validator/internal/ProfileSolver.hxx"

#include <map>
#include <memory>
#include <set>
#include <string>

namespace fhirtools
{
class FhirStructureDefinition;
class FhirConstraint;
class ProfileSetValidator;

/**
 * @brief Holds Validation information for ProfileValidator
 *
 * This information is shared across multiple instances of ProfileValidator and is therefore in a separate class.
 */
class ValidationData
{
public:
    ValidationData(std::unique_ptr<ProfiledElementTypeInfo> initMapKey);
    void add(Severity, std::string message, std::string elementFullPath, const FhirStructureDefinition* profile);
    void add(FhirConstraint constraint, std::string elementFullPath, const FhirStructureDefinition* profile);
    void append(ValidationResults);
    const ValidationResults& results() const;
    [[nodiscard]] bool isFailed() const;
    void fail();
    void merge(const ValidationData& other);
    const ProfiledElementTypeInfo& mapKey();

private:
    std::unique_ptr<ProfiledElementTypeInfo> mMapKey;
    ValidationResults mResult{};
    bool mFailed = false;
};

}

#endif// FHIRTOOLS_INTERNAL_VALIDATOR_VALIDATIONDATA_H
