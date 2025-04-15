/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef FHIR_TOOLS_FHIR_PATH_FHIRPATHVALIDATOR_HXX
#define FHIR_TOOLS_FHIR_PATH_FHIRPATHVALIDATOR_HXX

#include "fhirtools/model/Element.hxx"
#include "fhirtools/repository/FhirConstraint.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"

#include <iosfwd>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <variant>

namespace fhirtools
{
class FhirStructureRepository;
class ProfileSetValidator;
class ProfiledElementTypeInfo;
class ReferenceContext;

/**
 * @brief Provides functions to Validate a FHIR-Structures starting from a given element.
 */
class FhirPathValidator
{
public:
    [[nodiscard]] static ValidationResults validate(const std::shared_ptr<const Element>& element,
                                                    const std::string& elementFullPath, const ValidatorOptions& = {});
    [[nodiscard]] static ValidationResults validateWithProfiles(const std::shared_ptr<const Element>& element,
                                                                const std::string& elementFullPath,
                                                                const std::set<DefinitionKey>& profileKeys,
                                                                const ValidatorOptions& = {});

    // internal use
    const ValidatorOptions& options() const;
    const ProfiledElementTypeInfo& extensionRootDefPtr() const;

private:
    FhirPathValidator(const ValidatorOptions& options, std::unique_ptr<ProfiledElementTypeInfo> initExtensionRootDefPtr,
                      const std::shared_ptr<const FhirStructureRepository>& repo);
    static FhirPathValidator create(const ValidatorOptions&, const std::shared_ptr<const FhirStructureRepository>&);
    void validateInternal(const std::shared_ptr<const Element>& element,
                          std::set<ProfiledElementTypeInfo> extraProfiles, const std::string& elementFullPath);

    void validateAllSubElements(const std::shared_ptr<const Element>& element, ReferenceContext& referenceContext,
                                ProfileSetValidator& elementInfo, const std::string& elementFullPath);
    void processSubElements(const std::shared_ptr<const Element>& element, const std::string& subName,
                            const std::vector<std::shared_ptr<const Element>>& subElements, ReferenceContext&,
                            fhirtools::ProfileSetValidator& elementInfo, const std::string& subFullPathBase);
    void validateElement(const std::shared_ptr<const Element>& element, ReferenceContext&, ProfileSetValidator&,
                         const std::string& elementFullPath);
    void validateResource(const std::shared_ptr<const Element>& element, ReferenceContext&, ProfileSetValidator&,
                          const std::string& elementFullPath);
    std::set<const FhirStructureDefinition*> profiles(const Element& element, std::string_view elementFullPath);
    void addProfiles(const Element& element, fhirtools::ProfileSetValidator& profileSetValidator,
                     fhirtools::ReferenceContext& parentReferenceContext, std::string_view elementFullPath);

    ValidationResults result;
    const ValidatorOptions& mOptions;
    const std::unique_ptr<const ProfiledElementTypeInfo> mExtensionRootDefPtr;
    const std::shared_ptr<const FhirStructureRepository> mRepo;
};


}
#endif// FHIR_TOOLS_FHIR_PATH_FHIRPATHVALIDATOR_HXX
