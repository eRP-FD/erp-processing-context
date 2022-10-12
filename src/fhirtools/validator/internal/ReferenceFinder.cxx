// (C) Copyright IBM Deutschland GmbH 2022
// (C) Copyright IBM Corp. 2022

#include "fhirtools/validator/internal/ReferenceFinder.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/model/Element.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/util/Constants.hxx"


using namespace fhirtools;

void ReferenceFinder::FinderResult::merge(ReferenceFinder::FinderResult&& source)
{
    validationResults.append(std::move(source.validationResults));
    referenceContext.merge(std::move(source.referenceContext));
}
fhirtools::ReferenceFinder::FinderResult fhirtools::ReferenceFinder::find(const fhirtools::Element& element,
                                                                          std::set<ProfiledElementTypeInfo> profiles,
                                                                          const fhirtools::ValidatorOptions& options,
                                                                          std::string_view elementFullPath)
{
    if (profiles.empty())
    {
        profiles.emplace(element.definitionPointer());
    }
    const bool isResource = element.isResource();
    const bool isBundle = isResource && element.definitionPointer().profile()->url() == constants::bundleUrl;
    auto [resourceId, resultList] = element.resourceIdentity(elementFullPath);
    auto resourceInfo = std::make_shared<ResourceInfo>(ResourceInfo{.identity{std::move(resourceId)},
                                                                    .elementFullPath{std::string{elementFullPath}},
                                                                    .resourceRoot{element.shared_from_this()},
                                                                    .anchorType = ReferenceContext::AnchorType::none});
    ReferenceFinder finder{profiles, resourceInfo, true, isDocumentBundle(isBundle, element), 0};
    finder.mResult.validationResults.append(std::move(resultList));
    if (isResource)
    {
        finder.mProfiledElementTypes.merge(finder.profilesFromResource(element, elementFullPath));
    }
    finder.findInternal(element, elementFullPath, std::string{});
    if (isResource)
    {
        finder.mResult.referenceContext.addResource(std::move(resourceInfo));
    }
    finder.mResult.validationResults.append(finder.mResult.referenceContext.finalize(options));
    return std::move(finder.mResult);
}

ReferenceFinder::ReferenceFinder(std::set<ProfiledElementTypeInfo> profiles,
                                 std::shared_ptr<ResourceInfo> currentResource, bool followBundleEntry,
                                 bool isDocumentBundle, size_t bundleIndex)
    : mProfiledElementTypes{std::move(profiles)}
    , mCurrentResource{std::move(currentResource)}
    , mFollowBundleEntry{followBundleEntry}
    , mIsDocumentBundle{isDocumentBundle}
    , mBundleIndex{bundleIndex}
{
}

// NOLINTNEXTLINE(misc-no-recursion)
void ReferenceFinder::findInternal(const Element& element, std::string_view elementFullPath,
                                   const std::string& resourcePath)
{
    using namespace std::string_literals;
    const auto& repo = *element.getFhirStructureRepository();
    addSliceProfiles(element, elementFullPath);
    if (element.definitionPointer().element()->typeId() == "Reference")
    {
        processReference(element, elementFullPath, resourcePath);
    }
    for (const auto& subName : element.subElementNames())
    {
        const auto& subDef = element.definitionPointer().subField(subName);
        if (! subDef)
        {
            mResult.validationResults.add(Severity::debug, "undefined subfield: " + subName,
                                          std::string{elementFullPath}, element.definitionPointer().profile());
            continue;
        }
        const auto elementType = getElementType(repo, *subDef);
        if (! mFollowBundleEntry && elementType == ElementType::bundledResource)
        {
            continue;
        }
        const bool isArray = subDef->element()->isArray();
        std::ostringstream subFullPathBase;
        subFullPathBase << elementFullPath << '.' << subName;
        const auto& commonSubPets = subProfiledElementTypes(repo, subName, subFullPathBase.view());
        size_t idx = 0;
        for (const auto& subElement : element.subElements(subName))
        {
            std::ostringstream subElementFullPath;
            subElementFullPath << subFullPathBase.view();
            if (isArray)
            {
                subElementFullPath << '[' << idx << ']';
                ++idx;
            }
            if (subElement->isResource())
            {
                processResource(*subElement, commonSubPets, elementType, std::move(subElementFullPath));
            }
            else
            {
                ReferenceFinder subFinder{commonSubPets, mCurrentResource, mFollowBundleEntry, mIsDocumentBundle,
                                          mBundleIndex};
                std::ostringstream subResourcePath;
                subResourcePath << resourcePath << '.' << subName;
                subFinder.findInternal(*subElement, subElementFullPath.view(), subResourcePath.str());
                mResult.merge(std::move(subFinder.mResult));
                if (elementType == ElementType::bundleEntry)
                {
                    ++mBundleIndex;
                }
            }
        }
    }
}

// NOLINTNEXTLINE(misc-no-recursion, performance-unnecessary-value-param)
void ReferenceFinder::processResource(const Element& element, std::set<ProfiledElementTypeInfo> allSubPets,
                                      ElementType elementType, std::ostringstream&& elementFullPath)
{
    using AnchorType = ReferenceContext::AnchorType;
    const auto& repo = *element.getFhirStructureRepository();
    const auto& resourceType = element.resourceType();
    elementFullPath << '{' << resourceType << '}';
    const auto* resourceDef = repo.findTypeById(resourceType);
    if (! resourceDef)
    {
        mResult.validationResults.add(Severity::debug, "undefined resource type: " + resourceType,
                                      elementFullPath.str(), nullptr);
        return;
    }
    std::set<ProfiledElementTypeInfo> resourcePets;
    for (auto& pet : allSubPets) //NOLINT(readability-qualified-auto)
    {
        if (! pet.element()->isRoot() || resourceDef->isDerivedFrom(repo, *pet.profile()))
        {
            continue;
        }
        resourcePets.emplace(std::move(pet)); //NOLINT(hicpp-move-const-arg,performance-move-const-arg)
    }
    resourcePets.merge(profilesFromResource(element, elementFullPath.view()));
    if (resourcePets.empty())
    {
        resourcePets.emplace(resourceDef);
    }
    auto resourceHandling = getResourceHandling(elementType);
    const bool isBundle = resourceType == "Bundle";
    const bool isComposition = ! isBundle && resourceType == "Composition";
    if (resourceHandling == ResourceHandling::expectedComposition && ! isComposition)
    {
        mResult.validationResults.add(Severity::error,
                                      "First resource in Bundle of type document must be a Composition",
                                      elementFullPath.str(), nullptr);
    }

    const bool followBundleEntry = mFollowBundleEntry && ! isBundle;
    const bool isAnchor = resourceHandling == ResourceHandling::expectedComposition && isComposition;
    auto [resourceId, resultList] = element.resourceIdentity(elementFullPath.view());
    auto resourceInfo =
        std::make_shared<ResourceInfo>(ResourceInfo{.identity{std::move(resourceId)},
                                                    .elementFullPath{elementFullPath.str()},
                                                    .resourceRoot{element.shared_from_this()},
                                                    .anchorType = isAnchor ? AnchorType::composition : AnchorType::none,
                                                    .referenceRequirement = referenceRequirement(resourceHandling)});
    ReferenceFinder subFinder{resourcePets, resourceInfo, followBundleEntry, isDocumentBundle(isBundle, element), 0};
    subFinder.findInternal(element, elementFullPath.view(), std::string{});
    mResult.merge(std::move(subFinder.mResult));
    if (resourceHandling == ResourceHandling::contained)
    {
        mCurrentResource->contained.emplace_back(resourceInfo);
    }
    mResult.referenceContext.addResource(std::move(resourceInfo));
}


std::set<ProfiledElementTypeInfo> ReferenceFinder::subProfiledElementTypes(const FhirStructureRepository& repo,
                                                                           std::string_view subFieldName,
                                                                           std::string_view subFullPathBase)
{
    std::set<ProfiledElementTypeInfo> result;
    for (const auto& pet : mProfiledElementTypes)
    {
        const auto& petSubDefs = pet.subDefinitions(repo, subFieldName);
        result.insert(petSubDefs.begin(), petSubDefs.end());
        const auto& petSubField = pet.subField(subFieldName);
        if (petSubField)
        {
            addProfiles(repo, petSubField->element()->profiles(), std::string{subFullPathBase}, pet.profile());
        }
    }
    return result;
}

std::set<ProfiledElementTypeInfo>
fhirtools::ReferenceFinder::sliceProfiledElementTypes(const fhirtools::Element& element,
                                                      const fhirtools::ProfiledElementTypeInfo& profiledElementTypeInfo)
{
    std::set<ProfiledElementTypeInfo> result;
    const auto& fhirElement = profiledElementTypeInfo.element();
    const auto& slicing = fhirElement->slicing();
    if (! slicing.has_value())
    {
        return {};
    }
    const auto& repo = *element.getFhirStructureRepository();
    for (const auto& slice : slicing->slices())
    {
        const auto& condition = slice.condition(repo, slicing->discriminators());
        if (condition->test(element))
        {
            static_assert(std::is_reference_v<decltype(slice.profile())>,
                          "slice.profile() must return reference to allow taking the address");
            result.emplace(&slice.profile());
        }
    }
    return result;
}


std::set<ProfiledElementTypeInfo> fhirtools::ReferenceFinder::profilesFromResource(const fhirtools::Element& element,
                                                                                   std::string_view elementFullPath)
{
    using namespace std::string_literals;
    std::set<ProfiledElementTypeInfo> result;
    const auto& repo = *element.getFhirStructureRepository();
    for (const auto& profileUrl : element.profiles())
    {
        const auto* profile = repo.findDefinitionByUrl(profileUrl);
        if (! profile)
        {
            mResult.validationResults.add(Severity::debug, "undefined profile: "s.append(profileUrl),
                                          std::string{elementFullPath}, nullptr);
            continue;
        }
        mResult.validationResults.add(Severity::debug, "added profile: "s.append(profileUrl),
                                      std::string{elementFullPath}, nullptr);
        result.emplace(profile);
    }
    return result;
}


void ReferenceFinder::processReference(const Element& element, std::string_view elementFullPath,
                                       std::string resourcePath)
{
    const auto& repo = *element.getFhirStructureRepository();
    auto [referenceId, resultList] = element.referenceTargetIdentity(elementFullPath);
    mResult.validationResults.append(std::move(resultList));
    ReferenceContext::ReferenceInfo refInfo{.identity = std::move(referenceId),
                                            .elementFullPath = std::string{elementFullPath},
                                            .localPath = std::move(resourcePath),
                                            .referencingElement{element.shared_from_this()},
                                            .targetProfileSets{}};
    for (const auto& pet : mProfiledElementTypes)
    {
        std::set<const FhirStructureDefinition*> profileSet;
        std::ostringstream targetProfiles;
        targetProfiles << '[';
        std::string_view sep;
        for (const auto& profileUrl : pet.element()->referenceTargetProfiles())
        {
            const auto* profile = repo.findDefinitionByUrl(profileUrl);
            if (! profile)
            {
                mResult.validationResults.add(Severity::debug, "profile not found: " + profileUrl,
                                              std::string{elementFullPath}, pet.profile());
                continue;
            }
            targetProfiles << sep << profileUrl;
            sep = ", ";
            profileSet.emplace(profile);
        }

        targetProfiles << ']';
        mResult.validationResults.add(Severity::debug,
                                      (to_string(refInfo.identity) + " targetProfile: ").append(targetProfiles.view()),
                                      std::string{elementFullPath}, pet.profile());
        refInfo.targetProfileSets.emplace(pet.profile(), profileSet);
    }
    mCurrentResource->referenceTargets.emplace_back(std::move(refInfo));
}

bool fhirtools::ReferenceFinder::isDocumentBundle(bool isBundle, const fhirtools::Element& element)
{
    if (! isBundle)
    {
        return false;
    }
    auto bundleType = element.subElements("type");
    return ! bundleType.empty() && bundleType[0]->asString() == "document";
}

ReferenceFinder::ResourceHandling ReferenceFinder::getResourceHandling(ElementType elementType) const
{
    using enum ResourceHandling;
    if (elementType == ElementType::bundledResource && mIsDocumentBundle)
    {
        return mBundleIndex == 0 ? expectedComposition : mustBeReferencedFromAnchor;
    }
    return elementType == ElementType::containedResource ? contained : other;
}

ReferenceFinder::ElementType ReferenceFinder::getElementType(const FhirStructureRepository& repo,
                                                             const ProfiledElementTypeInfo& elementInfo)
{
    using namespace std::string_view_literals;
    if (elementInfo.profile()->kind() == FhirStructureDefinition::Kind::resource)
    {
        static constexpr std::string_view entry{"entry"};
        if (elementInfo.profile()->typeId() == "Bundle"sv)
        {
            if (elementInfo.element()->isRoot())
            {
                return ElementType::bundle;
            }
            const auto& elementPath = elementInfo.elementPath();
            if (elementPath.starts_with(entry))
            {
                if (elementPath.size() == entry.size())
                {
                    return ElementType::bundleEntry;
                }
                if (elementPath.compare(entry.size(), std::string_view::npos, ".resource"sv) == 0)
                {
                    return ElementType::bundledResource;
                }
            }
        }
        else if (elementInfo.elementPath() == "contained" &&
                 elementInfo.profile()->isDerivedFrom(repo, constants::domainResourceUrl))
        {
            return ElementType::containedResource;
        }
    }
    return ElementType::other;
}

ReferenceContext::AnchorType ReferenceFinder::referenceRequirement(ReferenceFinder::ResourceHandling elementType)
{
    using AnchorType = ReferenceContext::AnchorType;
    switch (elementType)
    {
        case ResourceHandling::contained:
            return AnchorType::contained;
        case ResourceHandling::expectedComposition:
            return AnchorType::none;
        case ResourceHandling::mustBeReferencedFromAnchor:
            return AnchorType::composition;
        case ResourceHandling::other:
            return AnchorType::none;
    }
    Fail2("invalid value for elementType: " + std::to_string(intmax_t(elementType)), std::logic_error);
}


void fhirtools::ReferenceFinder::addSliceProfiles(const Element& element, std::string_view elementFullPath)
{
    std::set<ProfiledElementTypeInfo> sliceProfiles;
    for (const auto& pet : mProfiledElementTypes)
    {
        if (pet.element()->hasSlices())
        {
            sliceProfiles.merge(sliceProfiledElementTypes(element, pet));
        }
    }
    addProfiles(*element.getFhirStructureRepository(), sliceProfiles, elementFullPath);
}

// NOLINTNEXTLINE(misc-no-recursion)
void fhirtools::ReferenceFinder::addProfiles(const FhirStructureRepository& repo,
                                             const std::set<ProfiledElementTypeInfo>& newProfiles,
                                             std::string_view elementFullPath)
{
    for (const auto& profile : newProfiles)
    {
        bool inserted = mProfiledElementTypes.emplace(profile).second;
        if (inserted)
        {
            addProfiles(repo, profile.element()->profiles(), elementFullPath, profile.profile());
        }
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
void fhirtools::ReferenceFinder::addProfiles(const FhirStructureRepository& repo, const std::list<std::string>& newProfileUrls,
                                             std::string_view elementFullPath,
                                             const FhirStructureDefinition* sourceProfile)
{
    std::set<ProfiledElementTypeInfo> newProfiles;
    for (const auto& profileUrl : newProfileUrls)
    {
        const auto* prof = repo.findDefinitionByUrl(profileUrl);
        if (! prof)
        {
            mResult.validationResults.add(Severity::debug, "profile not found: " + profileUrl,
                                          std::string{elementFullPath}, sourceProfile);
            continue;
        }
        newProfiles.emplace(prof);
    }
    addProfiles(repo, std::move(newProfiles), elementFullPath);
}
