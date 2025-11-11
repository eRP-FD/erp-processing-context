// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
//
// non-exclusively licensed to gematik GmbH

#include "FhirStructureRepositoryFixer.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"
#include "fhirtools/typemodel/ProfiledElementTypeInfo.hxx"
#include "fhirtools/util/Constants.hxx"

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
        fixElement(element);
    }
}

void FhirStructureRepositoryFixer::fixElement(ElementEntry& element)
{
    auto builder = fixMissingSlicingInDerivate(*element);
    if (builder)
    {
        *element = builder->getAndReset();
    }
}

std::unique_ptr<fhirtools::FhirElement::Builder>
FhirStructureRepositoryFixer::fixMissingSlicingInDerivate(std::shared_ptr<const FhirElement>& element,
                                                          std::unique_ptr<FhirElement::Builder> builder)
{
    // sometimes in a derived Profile, the discriminators aren't copied from the baseDefinition
    // this function traverses the derivation-tree down to all possible basetypes and copies them if found
    const ProfiledElementTypeInfo elementPet{element};
    if (element->slicing() == nullptr && element->typeId() == "Extension")
    {
        // we need this for closed-extensions-light feature.
        const auto extension = get<ProfiledElementTypeInfo>(mRepo.resolveBaseContentReference(fhirtools::constants::extension));
        Expect3(extension.element()->slicing(), "Extension.extension is not sliced.", std::logic_error);
        if (! builder)
        {
            builder = std::make_unique<FhirElement::Builder>(*element);
        }
        TVLOG(4) << "Fixed extension: " << elementPet;
        builder->slicing(FhirSlicing::Builder{*extension.element()->slicing()});
        return builder;
    }
    if (!element->hasSlices() || !element->slicing()->discriminators().empty() || element->cardinality().max == 0)
    {
        return builder;
    }
    auto parentSlicing = findParentSlicing(elementPet);
    if (parentSlicing)
    {
        TVLOG(4) << "Slicing not copied from parent type: " << elementPet << " - fixing";
        if (! builder)
        {
            builder = std::make_unique<FhirElement::Builder>(*element);
        }
        FhirSlicing::Builder copied{*element->slicing()};
        for (auto disc: parentSlicing->discriminators())
        {
            copied.addDiscriminator(std::move(disc));
        }
        builder->slicing(std::move(copied));
        return builder;
    }
    TLOG(INFO) << "Failed to fix Slicing: " << elementPet;
    return builder;
}

std::shared_ptr<const fhirtools::FhirSlicing>
// NOLINTNEXTLINE(misc-no-recursion)
FhirStructureRepositoryFixer::findParentSlicing(const ProfiledElementTypeInfo& pet) const
{
    if (pet.profile()->isSystemType())
    {
        return nullptr;
    }
    for (auto parentPet = pet.typeInfoInParentStuctureDefinition(); parentPet;
         parentPet = parentPet->typeInfoInParentStuctureDefinition())
    {
        if (auto slicing = parentPet->element()->slicing(); slicing && !slicing->discriminators().empty())
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
    return nullptr;
}

// NOLINTNEXTLINE(misc-no-recursion)
std::shared_ptr<const fhirtools::FhirSlicing> fhirtools::FhirStructureRepositoryFixer::findParentSlicing(
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
                                  : findParentSlicing(ProfiledElementTypeInfo{element});
    }
    return nullptr;
}
