/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef FHIR_TOOLS_FHIR_PATH_FHIRPATHVALIDATOR_HXX
#define FHIR_TOOLS_FHIR_PATH_FHIRPATHVALIDATOR_HXX

#include <iosfwd>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <variant>

#include "fhirtools/model/Element.hxx"
#include "fhirtools/repository/FhirConstraint.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"

namespace fhirtools
{
class ProfileSetValidator;
class ProfiledElementTypeInfo;
class FhirStructureRepository;

/**
 * @brief Provides functions to Validate a FHIR-Structures starting from a given element.
 */
class FhirPathValidator
{
public:
    [[nodiscard]] static ValidationResultList validate(const std::shared_ptr<const Element>& element,
                                                       const std::string& elementFullPath,
                                                       const ValidatorOptions& = {});
    [[nodiscard]] static ValidationResultList validateWithProfiles(const std::shared_ptr<const Element>& element,
                                                                   const std::string& elementFullPath,
                                                                   const std::set<std::string>& profileUrls,
                                                                   const ValidatorOptions& = {});

// internal use
    const ValidatorOptions& options() const;
    const ProfiledElementTypeInfo& extensionRootDefPtr() const;
    const ProfiledElementTypeInfo& elementExtensionDefPtr() const;

private:
    FhirPathValidator(const ValidatorOptions& options,
                      std::unique_ptr<ProfiledElementTypeInfo> initExtensionRootDefPtr,
                      std::unique_ptr<ProfiledElementTypeInfo> initElementExtensionDefPtr);
    static FhirPathValidator create(const ValidatorOptions&, const FhirStructureRepository*);
    void validateInternal(const std::shared_ptr<const Element>& element, const std::string& elementFullPath);

    void validateAllSubElements(const std::shared_ptr<const Element>& element, ProfileSetValidator& elementInfo,
                                const std::string& elementFullPath);
    void processSubElements(const std::shared_ptr<const Element>& element, const std::string& subName,
                            const std::vector<std::shared_ptr<const Element>>& subElements,
                            fhirtools::ProfileSetValidator& elementInfo, const std::string& subFullPathBase);
    void validateElement(const std::shared_ptr<const Element>& element, ProfileSetValidator&,
                         const std::string& elementFullPath);
    std::set<const FhirStructureDefinition*> profiles(const std::shared_ptr<const Element>& element,
                                                      const std::string& elementFullPath);

    ValidationResultList result;
    const ValidatorOptions& mOptions;
    const std::unique_ptr<const ProfiledElementTypeInfo> mExtensionRootDefPtr;
    const std::unique_ptr<const ProfiledElementTypeInfo> mElementExtensionDefPtr;
};


}
#endif// FHIR_TOOLS_FHIR_PATH_FHIRPATHVALIDATOR_HXX
