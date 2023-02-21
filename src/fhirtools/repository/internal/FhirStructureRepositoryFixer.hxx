// (C) Copyright IBM Deutschland GmbH 2023
// (C) Copyright IBM Corp. 2023

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
    explicit FhirStructureRepositoryFixer(FhirStructureRepository&);

    void fix();

    using StructureDefinitionEntries = decltype(FhirStructureRepository::mDefinitionsByKey);
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


    FhirStructureRepository& mRepo;
    friend class FhirStructureRepository;
};
}// namespace fhirtools

#endif// FHIR_TOOLS_FHIRSTRUCTUREREPOSITORYFIXER_HXX
