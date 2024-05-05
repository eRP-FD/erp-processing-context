// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
//
// non-exclusively licensed to gematik GmbH

#ifndef FHIR_TOOLS_FHIRSTRUCTUREREPOSITORYFIXER_HXX
#define FHIR_TOOLS_FHIRSTRUCTUREREPOSITORYFIXER_HXX

#include "fhirtools/repository/FhirElement.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"

#include <optional>

namespace fhirtools
{
class FhirElement;
class FhirStructureDefinition;
class ProfiledElementTypeInfo;

class FhirStructureRepositoryFixer
{
    explicit FhirStructureRepositoryFixer(FhirStructureRepositoryBackend&);

    void fix();

    using StructureDefinitionEntries = decltype(FhirStructureRepositoryBackend::mDefinitionsByKey);
    using StructureDefinitionEntry = StructureDefinitionEntries::iterator;
    void fixStructureDefinition(StructureDefinitionEntry& profile);

    using ElementEntries = decltype(FhirStructureDefinition::mElements);
    using ElementEntry = ElementEntries::iterator;
    void fixElement(FhirStructureDefinition& profile, ElementEntry& element);


    [[nodiscard]] std::unique_ptr<FhirElement::Builder>
    fixMissingSlicingInDerivate(FhirStructureDefinition& profile, std::shared_ptr<const FhirElement>& element,
                                std::unique_ptr<FhirElement::Builder> builder = nullptr);

    std::optional<fhirtools::FhirSlicing> findParentSlicing(const ProfiledElementTypeInfo& pet) const;
    std::optional<fhirtools::FhirSlicing> findParentSlicing(const FhirStructureDefinition& profile,
                                                            const std::string_view baseElementName,
                                                            const std::string_view rest) const;


    FhirStructureRepositoryBackend& mRepo;
    friend class FhirStructureRepositoryBackend;
};
}// namespace fhirtools

#endif// FHIR_TOOLS_FHIRSTRUCTUREREPOSITORYFIXER_HXX
