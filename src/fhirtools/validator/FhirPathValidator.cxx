/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "fhirtools/validator/FhirPathValidator.hxx"

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <ranges>
#include <utility>

#include "fhirtools/FPExpect.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/typemodel/ProfiledElementTypeInfo.hxx"
#include "fhirtools/validator/internal/ProfileSetValidator.hxx"

using fhirtools::FhirConstraint;
using fhirtools::FhirPathValidator;
fhirtools::FhirPathValidator::FhirPathValidator(const fhirtools::ValidatorOptions& options,
                                                std::unique_ptr<ProfiledElementTypeInfo> initExtensionRootDefPtr,
                                                std::unique_ptr<ProfiledElementTypeInfo> initElementExtensionDefPtr)
    : mOptions{options}
    , mExtensionRootDefPtr{std::move(initExtensionRootDefPtr)}
    , mElementExtensionDefPtr{std::move(initElementExtensionDefPtr)}
{
}

fhirtools::FhirPathValidator fhirtools::FhirPathValidator::create(const fhirtools::ValidatorOptions& options,
                                                                  const fhirtools::FhirStructureRepository* repo)
{
    using namespace std::string_literals;
    Expect3(repo != nullptr, "Failed to get FhirStructureRepository from element", std::logic_error);
    const auto* extensionDef = repo->findTypeById("Extension"s);
    Expect3(extensionDef != nullptr, "StructureDefinition for Extension not found.", std::logic_error);
    auto [initElementExtensionDefPtr, idx] = repo->resolveBaseContentReference("#Element.extension");
    Expect3(initElementExtensionDefPtr.element()->slicing().has_value(), "Element.extension must define slicing",
            std::logic_error);

    return FhirPathValidator{options,
        std::make_unique<ProfiledElementTypeInfo>(extensionDef),
        std::make_unique<ProfiledElementTypeInfo>(std::move(initElementExtensionDefPtr))};
}


fhirtools::ValidationResultList FhirPathValidator::validate(const std::shared_ptr<const Element>& element,
                                                            const std::string& elementFullPath,
                                                            const fhirtools::ValidatorOptions& options)
{
    auto validator = create(options, element->getFhirStructureRepository());
    validator.validateInternal(element, elementFullPath);
    return validator.result;
}

fhirtools::ValidationResultList FhirPathValidator::validateWithProfiles(const std::shared_ptr<const Element>& element,
                                                                        const std::string& elementFullPath,
                                                                        const std::set<std::string>& profileUrls,
                                                                        const ValidatorOptions& options)
{
    auto validator = create(options, element->getFhirStructureRepository());
    const auto* structureDefinition = element->getStructureDefinition();
    FPExpect(structureDefinition, "missing structure definition");
    std::set<ProfiledElementTypeInfo> profiles;
    for (const auto& url : profileUrls)
    {
        const auto* profile = element->getFhirStructureRepository()->findDefinitionByUrl(url);
        if (profile)
        {
            profiles.emplace(profile);
        }
        else
        {
            validator.result.add(Severity::error, "profile unknown: " + url, elementFullPath, nullptr);
        }
    }
    ProfileSetValidator info{element->definitionPointer(), profiles, validator};
    validator.validateElement(element, info, elementFullPath);
    return validator.result;
}

const fhirtools::ValidatorOptions& FhirPathValidator::options() const
{
    return mOptions;
}

const fhirtools::ProfiledElementTypeInfo& FhirPathValidator::elementExtensionDefPtr() const
{
    return *mElementExtensionDefPtr;
}

const fhirtools::ProfiledElementTypeInfo& fhirtools::FhirPathValidator::extensionRootDefPtr() const
{
    return *mExtensionRootDefPtr;
}

void FhirPathValidator::validateInternal(const std::shared_ptr<const Element>& element,
                                         const std::string& elementFullPath)
{
    using namespace std::string_literals;
    const auto* structureDefinition = element->getStructureDefinition();
    FPExpect(structureDefinition, "missing structure definition");
    FPExpect(element->definitionPointer().element() != nullptr, "missing element id");
    const auto& elementId = element->definitionPointer().element()->name();
    std::set<ProfiledElementTypeInfo> defPtrs;
    for (const auto* profileDef : profiles(element, elementFullPath))
    {
        auto elementDef = profileDef->findElement(elementId);
        if (elementDef)
        {
            defPtrs.emplace(profileDef, std::move(elementDef));
        }
        else
        {
            result.add(Severity::error,
                       profileDef->url() + '|' + profileDef->version() + " no such element: " + elementId,
                       elementFullPath, profileDef);
        }
    }
    ProfileSetValidator info{element->definitionPointer(), defPtrs, *this};
    validateElement(element, info, elementFullPath);
}
//NOLINTNEXTLINE(misc-no-recursion)
void FhirPathValidator::validateAllSubElements(const std::shared_ptr<const Element>& element,
                                               ProfileSetValidator& elementInfo, const std::string& elementFullPath)
{
    using namespace std::string_view_literals;
    const auto& allSubElements = elementInfo.rootPointer().subElements();
    std::set<std::string> unprocessedSubNames;
    std::ranges::copy(element->subElementNames(), std::inserter(unprocessedSubNames, unprocessedSubNames.end()));
    for (const auto& subDefPtr : allSubElements)
    {
        std::string subName{subDefPtr.element()->fieldName()};
        std::ostringstream subFullPathBase;
        subFullPathBase << elementFullPath << '.' << subName;
        bool exists = unprocessedSubNames.erase(subName) > 0;
        const auto& subElements =
            exists ? element->subElements(subName) : std::vector<std::shared_ptr<const Element>>{};
        if (! exists)
        {
            // needed to create counters for non-existing fields
            auto sub = elementInfo.subField(*element->getFhirStructureRepository(), subName);
            sub->finalize(subFullPathBase.str());
            result.append(sub->results());
        }
        // TODO: ERP-10501 handle value
        processSubElements(element, subName, subElements, elementInfo, subFullPathBase.str());
    }
    if (! unprocessedSubNames.empty())
    {
        std::ostringstream names;
        std::string_view sep{};
        for (const auto& s : unprocessedSubNames)
        {
            names << sep << s;
            sep = ", ";
        }
        result.add(Severity::error, "undefined sub element: " + names.str(), elementFullPath,
                   element->definitionPointer().profile());
    }
}

//NOLINTNEXTLINE(misc-no-recursion)
void FhirPathValidator::processSubElements(const std::shared_ptr<const Element>& element, const std::string& subName,
                                           const std::vector<std::shared_ptr<const Element>>& subElements,
                                           ProfileSetValidator& elementInfo, const std::string& subFullPathBase)
{
    size_t idx = 0;
    for (const auto& subElement : subElements)
    {
        auto subElementInfo = elementInfo.subField(*element->getFhirStructureRepository(), subName);
        bool isResource = subElementInfo->isResource();
        bool isArray = subElementInfo->isArray();
        std::ostringstream fullSubName;
        fullSubName << subFullPathBase;
        if (isArray)
        {
            fullSubName << '[' << idx << ']';
            ++idx;
        }
        if (isResource)
        {
            const auto& resourceType = subElement->resourceType();
            fullSubName << '{' << resourceType << '}';
            const auto* resourceDef = subElement->getFhirStructureRepository()->findTypeById(resourceType);
            if (! resourceDef)
            {
                result.add(Severity::error, "resourceType unknown: " + resourceType, fullSubName.str(), nullptr);
                continue;
            }

            result.add(Severity::debug, "resource is: " + resourceType, fullSubName.str(), resourceDef);
            subElementInfo->typecast(*subElement->getFhirStructureRepository(), resourceDef);
            subElementInfo->addProfiles(*subElement->getFhirStructureRepository(),
                                        profiles(subElement, fullSubName.str()));
            validateElement(subElement, *subElementInfo, fullSubName.str());
        }
        else
        {
            validateElement(subElement, *subElementInfo, fullSubName.str());
        }
    }
}

//NOLINTNEXTLINE(misc-no-recursion)
void FhirPathValidator::validateElement(const std::shared_ptr<const Element>& element,
                                        ProfileSetValidator& profileSetValidator, const std::string& elementFullPath)
{
    FPExpect3(element != nullptr, "element must not be null.", std::logic_error);
    profileSetValidator.process(*element, elementFullPath);
    validateAllSubElements(element, profileSetValidator, elementFullPath);
    profileSetValidator.finalize(elementFullPath);
    result.append(profileSetValidator.results());
}


std::set<const fhirtools::FhirStructureDefinition*>
FhirPathValidator::profiles(const std::shared_ptr<const Element>& element, const std::string& elementFullPath)
{
    using namespace std::string_literals;
    const auto& profileUrls = element->profiles();
    std::set<const FhirStructureDefinition*> profs;
    for (const auto& url : profileUrls)
    {
        const auto* profileDef = element->getFhirStructureRepository()->findDefinitionByUrl(url);
        if (profileDef)
        {
            profs.emplace(profileDef);
        }
        else
        {
            result.add(Severity::error, "Unknown profile: "s.append(url), elementFullPath, nullptr);
        }
    }
    return profs;
}