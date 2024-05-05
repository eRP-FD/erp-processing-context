/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/validator/FhirPathValidator.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/typemodel/ProfiledElementTypeInfo.hxx"
#include "fhirtools/util/Constants.hxx"
#include "fhirtools/validator/internal/ProfileSetValidator.hxx"
#include "fhirtools/validator/internal/ReferenceFinder.hxx"

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <ranges>
#include <utility>

using fhirtools::FhirConstraint;
using fhirtools::FhirPathValidator;
fhirtools::FhirPathValidator::FhirPathValidator(const fhirtools::ValidatorOptions& options,
                                                std::unique_ptr<ProfiledElementTypeInfo> initExtensionRootDefPtr,
                                                const std::shared_ptr<const FhirStructureRepository>& repo)
    : mOptions{options}
    , mExtensionRootDefPtr{std::move(initExtensionRootDefPtr)}
    , mRepo(repo)
{
}

fhirtools::FhirPathValidator
fhirtools::FhirPathValidator::create(const fhirtools::ValidatorOptions& options,
                                     const std::shared_ptr<const FhirStructureRepository>& repo)
{
    using namespace std::string_literals;
    Expect3(repo != nullptr, "Failed to get FhirStructureRepository from element", std::logic_error);
    const auto* extensionDef = repo->findTypeById("Extension"s);
    Expect3(extensionDef != nullptr, "StructureDefinition for Extension not found.", std::logic_error);
    return FhirPathValidator{options, std::make_unique<ProfiledElementTypeInfo>(extensionDef), repo};
}


fhirtools::ValidationResults FhirPathValidator::validate(const std::shared_ptr<const Element>& element,
                                                         const std::string& elementFullPath,
                                                         const fhirtools::ValidatorOptions& options)
{
    auto validator = create(options, element->getFhirStructureRepository());
    validator.validateInternal(element, {}, elementFullPath);
    return validator.result;
}

fhirtools::ValidationResults FhirPathValidator::validateWithProfiles(const std::shared_ptr<const Element>& element,
                                                                     const std::string& elementFullPath,
                                                                     const std::set<std::string>& profileUrls,
                                                                     const ValidatorOptions& options)
{
    std::set<ProfiledElementTypeInfo> profiles;
    const auto& elementType = element->definitionPointer().element()->typeId();
    ValidationResults resultList;
    for (const auto& url : profileUrls)
    {
        const auto* profile = element->getFhirStructureRepository()->findDefinitionByUrl(url);
        if (profile)
        {
            if (profile->typeId() == elementType)
            {
                profiles.emplace(profile);
            }
            else
            {
                resultList.add(Severity::error,
                               // NOLINTNEXTLINE(performance-inefficient-string-concatenation)
                               "requested Profile does not match element type '" + elementType + "': " + url,
                               elementFullPath, profile);
            }
        }
        else
        {
            resultList.add(Severity::error, "profile unknown: " + url, elementFullPath, nullptr);
        }
    }
    if (resultList.highestSeverity() >= Severity::error)
    {// fail early in this case, because it occurs quite often, when slicing by profile.
        return resultList;
    }
    auto validator = create(options, element->getFhirStructureRepository());
    validator.result.merge(std::move(resultList));
    validator.validateInternal(element, profiles, elementFullPath);
    return validator.result;
}

const fhirtools::ValidatorOptions& FhirPathValidator::options() const
{
    return mOptions;
}


const fhirtools::ProfiledElementTypeInfo& fhirtools::FhirPathValidator::extensionRootDefPtr() const
{
    return *mExtensionRootDefPtr;
}
void fhirtools::FhirPathValidator::validateInternal(const std::shared_ptr<const Element>& element,
                                                    std::set<ProfiledElementTypeInfo> extraProfiles,
                                                    const std::string& elementFullPath)
{
    using namespace std::string_literals;
    const auto* structureDefinition = element->getStructureDefinition();
    FPExpect(structureDefinition, "missing structure definition");
    FPExpect(element->definitionPointer().element() != nullptr, "missing element id");
    const auto& elementId = element->definitionPointer().element()->name();
    std::set<ProfiledElementTypeInfo> defPtrs;
    if (options().validateMetaProfiles)
    {
        for (const auto* profileDef : profiles(*element, elementFullPath))
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
    }
    defPtrs.merge(extraProfiles);
    ProfileSetValidator profileSetValidator{element->definitionPointer(), defPtrs, *this};
    auto referenceContext = profileSetValidator.buildReferenceContext(*element, elementFullPath);
    validateElement(element, referenceContext, profileSetValidator, elementFullPath);
}
//NOLINTNEXTLINE(misc-no-recursion)
void FhirPathValidator::validateAllSubElements(const std::shared_ptr<const Element>& element,
                                               ReferenceContext& referenceContext, ProfileSetValidator& elementInfo,
                                               const std::string& elementFullPath)
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
            result.merge(sub->results());
        }
        processSubElements(element, subName, subElements, referenceContext, elementInfo, subFullPathBase.str());
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
                                           ReferenceContext& referenceContext, ProfileSetValidator& elementInfo,
                                           const std::string& subFullPathBase)
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
            validateResource(subElement, referenceContext, *subElementInfo, fullSubName.str());
        }
        else
        {
            validateElement(subElement, referenceContext, *subElementInfo, fullSubName.str());
        }
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
void fhirtools::FhirPathValidator::validateResource(const std::shared_ptr<const Element>& resourceElement,
                                                    fhirtools::ReferenceContext& referenceContext,
                                                    fhirtools::ProfileSetValidator& profileSetValidator,
                                                    const std::string& elementFullPath)
{
    const auto& resourceType = resourceElement->resourceType();
    const auto* resourceDef = resourceElement->getFhirStructureRepository()->findTypeById(resourceType);
    if (! resourceDef)
    {
        result.add(Severity::error, "resourceType unknown: " + resourceType, elementFullPath, nullptr);
        return;
    }
    result.add(Severity::debug, "resource is: " + resourceType, elementFullPath, resourceDef);
    profileSetValidator.typecast(*resourceElement->getFhirStructureRepository(), resourceDef);
    addProfiles(*resourceElement, profileSetValidator, referenceContext, elementFullPath);
    if (resourceElement->definitionPointer().profile()->isDerivedFrom(*mRepo, constants::bundleUrl))
    {
        auto newContext = profileSetValidator.buildReferenceContext(*resourceElement, elementFullPath);
        validateElement(resourceElement, newContext, profileSetValidator, elementFullPath);
    }
    else
    {
        validateElement(resourceElement, referenceContext, profileSetValidator, elementFullPath);
    }
}


//NOLINTNEXTLINE(misc-no-recursion)
void FhirPathValidator::validateElement(const std::shared_ptr<const Element>& element,
                                        ReferenceContext& referenceContext, ProfileSetValidator& profileSetValidator,
                                        const std::string& elementFullPath)
{
    FPExpect3(element != nullptr, "element must not be null.", std::logic_error);
    profileSetValidator.process(*element, elementFullPath);
    validateAllSubElements(element, referenceContext, profileSetValidator, elementFullPath);
    profileSetValidator.finalize(elementFullPath);
    result.merge(profileSetValidator.results());
}


std::set<const fhirtools::FhirStructureDefinition*> FhirPathValidator::profiles(const Element& element,
                                                                                std::string_view elementFullPath)
{
    using namespace std::string_literals;
    const auto& profileUrls = element.profiles();
    std::set<const FhirStructureDefinition*> profs;
    for (const auto& url : profileUrls)
    {
        const auto* profileDef = element.getFhirStructureRepository()->findDefinitionByUrl(url);
        if (profileDef)
        {
            profs.emplace(profileDef);
        }
        else
        {
            result.add(Severity::error, "Unknown profile: "s.append(url), std::string{elementFullPath}, nullptr);
        }
    }
    return profs;
}

void fhirtools::FhirPathValidator::addProfiles(const Element& element,
                                               fhirtools::ProfileSetValidator& profileSetValidator,
                                               fhirtools::ReferenceContext& parentReferenceContext,
                                               std::string_view elementFullPath)
{
    const auto* resourceType = profileSetValidator.rootPointer().profile();
    std::set<const FhirStructureDefinition*> metaProfiles;
    for (const auto* metaProf : profiles(element, elementFullPath))
    {
        if (resourceType == metaProf->baseType(*mRepo))
        {
            metaProfiles.emplace(metaProf);
            if (options().validateMetaProfiles)
            {
                profileSetValidator.addProfile(*mRepo, metaProf);
            }
        }
        else
        {
            result.add(Severity::error,
                       "Profile must be based on " + resourceType->typeId() + ": " + metaProf->url() + '|' +
                           metaProf->version(),
                       std::string{elementFullPath}, nullptr);
        }
    }

    auto [identity, resultList] = element.resourceIdentity(elementFullPath);
    result.merge(std::move(resultList));


    for (const auto& resource : parentReferenceContext.resources())
    {
        for (const auto& target : resource->referenceTargets)
        {
            if (target.targetProfileSets.empty() || target.identity != identity)
            {
                continue;
            }
            profileSetValidator.addTargetProfiles(target, metaProfiles, elementFullPath);
        }
    }
}
