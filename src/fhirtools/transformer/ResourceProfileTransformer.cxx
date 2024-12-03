/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/transformer/ResourceProfileTransformer.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"

#include <ranges>

namespace fhirtools
{

void ResourceProfileTransformer::Result::merge(ResourceProfileTransformer::Result&& subResult)
{
    success &= subResult.success;
    if (! subResult.errors.empty())
    {
        errors.merge(std::move(subResult).errors);
    }
}

std::string ResourceProfileTransformer::Result::summary() const
{
    std::string summary;
    for (const auto& item : errors)
    {
        summary += item + "; ";
    }
    return summary;
}

ResourceProfileTransformer::ResourceProfileTransformer(const FhirStructureRepository* repository,
                                                       const FhirStructureDefinition* sourceProfile,
                                                       const FhirStructureDefinition* targetProfile,
                                                       const Options& options)
    : mRepository(repository)
    , mSourceProfile(sourceProfile)
    , mTargetProfile(targetProfile)
    , mOptions(options)
{
    if (mSourceProfile)
    {
        Expect(mSourceProfile->typeId() == mTargetProfile->typeId(),
               "source and target profile must derive from same base profile, but are actually derived from " +
                   mSourceProfile->typeId() + " and " + mTargetProfile->typeId());

        mValueMappings.emplace_back(mSourceProfile->urlAndVersion(), mTargetProfile->urlAndVersion());
    }
    mOptions.keepExtensions.emplace("http://hl7.org/fhir/StructureDefinition/data-absent-reason");
}

ResourceProfileTransformer& ResourceProfileTransformer::map(const std::vector<ValueMapping>& valueMappings)
{
    std::ranges::copy(valueMappings, std::back_inserter(mValueMappings));
    return *this;
}

ResourceProfileTransformer& ResourceProfileTransformer::map(const ValueMapping& valueMapping)
{
    return map(std::vector{valueMapping});
}


ResourceProfileTransformer::Result
ResourceProfileTransformer::transform(MutableElement& element, const DefinitionKey& targetProfileDefinitionKey) const
{
    Result result{};
    // run fhir validation to find missing mandatory elements and illegal elements that must be removed
    ValidatorOptions valOpts{.collectInfo = true};
    if (mOptions.removeUnknownExtensionsFromOpenSlicing)
    {
        valOpts.reportUnknownExtensions = ValidatorOptions::ReportUnknownExtensionsMode::onlyOpenSlicing;
    }
    auto validationResult = FhirPathValidator::validateWithProfiles(element.shared_from_this(),
                                                                    element.definitionPointer().element()->name(),
                                                                    {targetProfileDefinitionKey}, valOpts);
    for (const auto& validationInfo : validationResult.infos())
    {
        auto mutableElement = std::dynamic_pointer_cast<const MutableElement>(validationInfo.element.lock());
        if (! mutableElement)
        {
            continue;
        }
        switch (validationInfo.kind)
        {
            case ValidationAdditionalInfo::MissingMandatoryElement:
                TVLOG(1) << "missing element " << validationInfo.elementFullPath.value_or("???") << " type "
                         << validationInfo.typeId << " in " << mutableElement->definitionPointer().element()->name();
                try
                {
                    addDataAbsentExtension(*mutableElement, validationInfo);
                }
                catch (const std::runtime_error& re)
                {
                    result.merge({.success = false,
                                  .errors = {"error generating data-absent-reason for " +
                                             *validationInfo.elementFullPath + ": " + re.what()}});
                }
                break;
            case ValidationAdditionalInfo::IllegalElement:
                try
                {
                    removeElement(*mutableElement);
                }
                catch (const std::runtime_error& re)
                {
                    result.merge(
                        {.success = false,
                         .errors = {"error removing element " + *validationInfo.elementFullPath + ": " + re.what()}});
                }
                break;
            case ValidationAdditionalInfo::ConstraintViolation:
                TVLOG(1) << "constraint violation: " << validationInfo.constraintKey.value_or("???") << ": "
                         << validationInfo.elementFullPath.value_or("???") << " type " << validationInfo.typeId
                         << " in " << mutableElement->definitionPointer().element()->name();
                if (validationInfo.constraintKey == "rat-1")
                {
                    addDataAbsentExtension(*mutableElement, FhirStructureDefinition::Kind::complexType,
                                           *validationInfo.elementFullPath + ".denominator", "Quantity", "denominator");
                }
                break;
        }
    }

    return result;
}

// NOLINTNEXTLINE(*-no-recursion)
ResourceProfileTransformer::Result ResourceProfileTransformer::applyMappings(const MutableElement& element) const
{
    Result result{};
    try
    {
        if (element.hasValue())
        {
            for (const auto& [from, to, restrictFieldName] : mValueMappings)
            {
                const auto& asString = element.asString();
                if (asString == from)
                {
                    if (! restrictFieldName || element.elementName() == *restrictFieldName)
                    {
                        TVLOG(1) << "mapping value of element " << element.elementName() << ": " << asString << " -> "
                                 << to;
                        element.setString(to);
                        break;
                    }
                }
            }
        }
        for (const auto& subElementName : element.subElementNames())
        {
            for (const auto& subElement : element.subElements(subElementName))
            {
                auto mutableSubElement = std::dynamic_pointer_cast<const MutableElement>(subElement);
                FPExpect(mutableSubElement, "could not dynamic cast to const MutableElement");
                result.merge(applyMappings(*mutableSubElement));
            }
        }
    }
    catch (const std::runtime_error& err)
    {
        result.success = false;
        result.errors.emplace_back(err.what());
    }
    return result;
}

void ResourceProfileTransformer::addDataAbsentExtension(const MutableElement& element,
                                                        const ValidationAdditionalInfo& validationAdditionalInfo) const
{
    if (validationAdditionalInfo.elementFullPath)
    {
        auto missingElementName = std::string_view{*validationAdditionalInfo.elementFullPath};
        if (auto dot = missingElementName.rfind('.'); dot != std::string::npos && dot < missingElementName.size() - 1)
        {
            missingElementName = missingElementName.substr(dot + 1);
        }
        if (const auto* const type = mRepository->findTypeById(validationAdditionalInfo.typeId))
        {
            addDataAbsentExtension(element, type->kind(), *validationAdditionalInfo.elementFullPath, type->typeId(),
                                   missingElementName);
        }
    }
}

void ResourceProfileTransformer::addDataAbsentExtension(const MutableElement& element,
                                                        FhirStructureDefinition::Kind elementKind,
                                                        const std::string_view& elementFullPath,
                                                        const std::string_view& typeId,
                                                        const std::string_view& missingElementName) const
{
    if (missingElementName.find('*') != std::string_view::npos)
    {
        return;
    }
    if (elementKind == FhirStructureDefinition::Kind::primitiveType)
    {
        TVLOG(1) << "generating data-absent-reason for primitive: " << elementFullPath;
        element.setDataAbsentExtension("_" + std::string{missingElementName});
    }
    else if (elementKind == FhirStructureDefinition::Kind::complexType && typeId == "Quantity")
    {
        TVLOG(1) << "generating data-absent-reason for Quantity: " << elementFullPath;
        element.setDataAbsentExtension(std::string{missingElementName} + "/_value");
        element.setDataAbsentExtension(std::string{missingElementName} + "/_system");
        element.setDataAbsentExtension(std::string{missingElementName} + "/_code");
    }
    else if (elementKind == FhirStructureDefinition::Kind::complexType && typeId == "Coding")
    {
        TVLOG(1) << "generating data-absent-reason for Coding: " << elementFullPath;
        element.setDataAbsentExtension(std::string{missingElementName} + "/0/_system");
        //        element.setDataAbsentExtension(std::string{missingElementName} + "/0/code");
    }
}

void ResourceProfileTransformer::removeElement(const MutableElement& element) const
{
    if (element.hasSubElement("url"))
    {
        const auto url = element.subElements("url").front()->asString();
        if (mOptions.keepExtensions.contains(url))
        {
            TVLOG(1) << "keeping " << element.definitionPointer().element()->name() << " with url " << url;
            return;
        }
        else
        {
            TVLOG(1) << "removing " << element.definitionPointer().element()->name() << " with url " << url;
        }
    }
    else
    {
        TVLOG(1) << "removing " << element.definitionPointer().element()->name();
    }
    element.removeFromParent();
}

}
