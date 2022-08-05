// (C) Copyright IBM Deutschland GmbH 2022
// (C) Copyright IBM Corp. 2022

#ifndef FHIRTOOLS_INTERNAL_VALIDATOR_VALIDATIONDATA_H
#define FHIRTOOLS_INTERNAL_VALIDATOR_VALIDATIONDATA_H

#include "fhirtools/validator/internal/ProfileSolver.hxx"
#include "fhirtools/validator/Severity.hxx"
#include "fhirtools/validator/ValidationResult.hxx"

#include <string>
#include <map>
#include <memory>

namespace fhirtools
{
class FhirStructureDefinition;
class FhirConstraint;
class ProfileSetValidator;
class ProfileValidatorMapKey;

/**
 * @brief Holds Validation information for ProfileValidator
 *
 * This information is shared accross multiple instances of ProfileValidator and is therfore in a separate class.
 */
class ValidationData
{
public:
    ValidationData(std::unique_ptr<ProfileValidatorMapKey> initMapKey);
    void add(Severity, std::string message, std::string elementFullPath, const FhirStructureDefinition* profile);
    void add(FhirConstraint constraint, std::string elementFullPath, const FhirStructureDefinition* profile);
    void append(ValidationResultList);
    const ValidationResultList& results() const;
    [[nodiscard]] bool isFailed() const;
    void fail();
    void merge(const ValidationData& other);
    const ProfileValidatorMapKey& mapKey();
private:
    std::unique_ptr<ProfileValidatorMapKey> mMapKey;
    ValidationResultList mResult{};
    bool mFailed = false;
};

}

#endif// FHIRTOOLS_INTERNAL_VALIDATOR_VALIDATIONDATA_H
