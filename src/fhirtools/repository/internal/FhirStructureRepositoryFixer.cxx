// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
//
// non-exclusively licensed to gematik GmbH

#include "fhirtools/FPExpect.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/repository/internal/FhirStructureRepositoryFixer.hxx"
#include "fhirtools/typemodel/ProfiledElementTypeInfo.hxx"

using fhirtools::FhirStructureRepositoryFixer;


FhirStructureRepositoryFixer::FhirStructureRepositoryFixer(FhirStructureRepositoryBackend& repo)
    : mRepo{repo}
{
}

void FhirStructureRepositoryFixer::fix()
{
    for (auto def = mRepo.mDefinitionsByKey.begin(), end = mRepo.mDefinitionsByKey.end(); def != end; ++def)
    {
        fixStructureDefinition(def);
    }
}
void FhirStructureRepositoryFixer::fixStructureDefinition(StructureDefinitionEntry& profile)
{
    auto& elements = profile->second->mElements;
    for (auto element = elements.begin(), end = elements.end(); element != end; ++element)
    {
        fixElement(*profile->second, element);
    }
}

void FhirStructureRepositoryFixer::fixElement(FhirStructureDefinition& profile, ElementEntry& element)
{
    auto builder = fixMissingSlicingInDerivate(profile, *element);
    if (builder)
    {
        *element = builder->getAndReset();
    }
}

std::unique_ptr<fhirtools::FhirElement::Builder>
FhirStructureRepositoryFixer::fixMissingSlicingInDerivate(FhirStructureDefinition& profile,
                                                          std::shared_ptr<const FhirElement>& element,
                                                          std::unique_ptr<FhirElement::Builder> builder)
{
    // sometimes in a derived Profile, the slicing isn't copied from the baseDefinition
    // this function traverses the derivation-tree down to all possible basetypes and copies the slicing if it was found
    bool isSliceRoot = profile.kind() == FhirStructureDefinition::Kind::slice && element->name() == profile.typeId();
    if (element->slicing() || isSliceRoot)
    {
        return builder;
    }
    auto parentSlicing = findParentSlicing(ProfiledElementTypeInfo{&profile, element});
    if (parentSlicing)
    {
        TVLOG(2) << "Slicing not copied from parent type: " << profile.url() << '|' << profile.version() << "@"
                 << element->originalName() << " - fixing";
        if (! builder)
        {
            builder = std::make_unique<FhirElement::Builder>(*element);
        }
        builder->slicing(FhirSlicing{*parentSlicing});
    }
    return builder;
}

std::optional<fhirtools::FhirSlicing>
// NOLINTNEXTLINE(misc-no-recursion)
FhirStructureRepositoryFixer::findParentSlicing(const ProfiledElementTypeInfo& pet) const
{
    if (pet.profile()->isSystemType())
    {
        return std::nullopt;
    }
    for (auto parentPet = pet.typeInfoInParentStuctureDefinition(*mRepo.defaultView()); parentPet;
         parentPet = parentPet->typeInfoInParentStuctureDefinition(*mRepo.defaultView()))
    {
        if (parentPet && parentPet->element()->slicing())
        {
            return parentPet->element()->slicing();
        }
    }
    std::string_view elementName{pet.element()->name()};
    auto firstDot = elementName.find('.');
    for (size_t dot = elementName.rfind('.'); dot > firstDot; dot = elementName.rfind('.', dot - 1))
    {
        auto baseElementName = elementName.substr(0, dot);
        auto rest = elementName.substr(dot);
        auto slicing = findParentSlicing(*pet.profile(), baseElementName, rest);
        if (slicing)
        {
            return slicing;
        }
    }
    return std::nullopt;
}

// NOLINTNEXTLINE(misc-no-recursion)
std::optional<fhirtools::FhirSlicing> fhirtools::FhirStructureRepositoryFixer::findParentSlicing(
    const FhirStructureDefinition& profile, const std::string_view baseElementName, const std::string_view rest) const
{
    auto baseElement = profile.findElement(baseElementName);
    FPExpect3(baseElement, ("Missing elemnent in " + profile.urlAndVersion() + ": ").append(baseElementName),
              std::logic_error);
    auto baseElementTypeId = baseElement->typeId();
    if (baseElementTypeId.empty())
    {
        auto contentRef =
            get<ProfiledElementTypeInfo>(mRepo.resolveBaseContentReference(baseElement->contentReference()));
        return findParentSlicing(*contentRef.profile(), contentRef.element()->name(), rest);
    }
    const auto* baseElementType = mRepo.findTypeById(baseElement->typeId());
    FPExpect3(baseElementType,
              ("Unknown type for element " + profile.urlAndVersion() + '@').append(baseElementName) + ": " +
                  baseElement->typeId(),
              std::logic_error);
    auto element = baseElementType->findElement(rest);
    if (element)
    {
        return element->slicing() ? element->slicing()
                                  : findParentSlicing(ProfiledElementTypeInfo{baseElementType, element});
    }
    return std::nullopt;
}
