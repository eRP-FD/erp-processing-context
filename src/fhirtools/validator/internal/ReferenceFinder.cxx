// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
//
// non-exclusively licensed to gematik GmbH

#include "fhirtools/validator/internal/ReferenceFinder.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/model/Element.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/groups/FhirResourceGroup.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"
#include "fhirtools/util/Constants.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"

using namespace fhirtools;

void ReferenceFinder::FinderResult::merge(ReferenceFinder::FinderResult&& source)
{
    validationResults.merge(std::move(source.validationResults));
    referenceContext.merge(std::move(source.referenceContext));
}
fhirtools::ReferenceFinder::FinderResult
fhirtools::ReferenceFinder::find(const std::shared_ptr<const FhirStructureRepositoryView>& view,
                                 const fhirtools::Element& element, std::set<ProfiledElementTypeInfo> profiles,
                                 const fhirtools::ValidatorOptions& options, std::string_view elementFullPath)
{
    if (profiles.empty())
    {
        profiles.emplace(element.definitionPointer());
    }
    const bool isResource = element.isResource();
    const bool isBundle = isResource && element.definitionPointer().profile()->url() == constants::bundleUrl;
    auto [resourceId, resultList] = element.resourceIdentity(elementFullPath);
    auto resourceInfo = std::make_shared<ResourceInfo>(ResourceInfo{
        .identity{std::move(resourceId)},
        .elementFullPath{std::string{elementFullPath}},
        .resourceRoot{element.shared_from_this()},
        .anchorType = anchorType(element, ResourceHandling::other, false),
        .parent = {},
    });
    ReferenceFinder finder{view, profiles, resourceInfo, true, isDocumentBundle(isBundle, element), 0, options};
    finder.mResult.validationResults.merge(std::move(resultList));
    if (isResource)
    {
        finder.mProfiledElementTypes.merge(finder.profilesFromResource(element, elementFullPath));
    }
    finder.findInternal(element, elementFullPath, std::string{});
    if (isResource)
    {
        finder.mResult.referenceContext.addResource(std::move(resourceInfo));
    }
    finder.mResult.validationResults.merge(finder.mResult.referenceContext.finalize(options));
    return std::move(finder.mResult);
}

ReferenceFinder::ReferenceFinder(std::shared_ptr<const FhirStructureRepositoryView> view,
                                 std::set<ProfiledElementTypeInfo> profiles,
                                 std::shared_ptr<ResourceInfo> currentResource, bool followBundleEntry,
                                 bool isDocumentBundle, size_t bundleIndex, const ValidatorOptions& options)
    : mView{std::move(view)}
    , mProfiledElementTypes{std::move(profiles)}
    , mCurrentResource{std::move(currentResource)}
    , mFollowBundleEntry{followBundleEntry}
    , mIsDocumentBundle{isDocumentBundle}
    , mBundleIndex{bundleIndex}
    , mOptions{options}
{
}

// NOLINTNEXTLINE(misc-no-recursion)
void ReferenceFinder::findInternal(const Element& element, std::string_view elementFullPath,
                                   const std::string& resourcePath)
{
    using namespace std::string_literals;
    addSliceProfiles(element, elementFullPath);
    if (element.definitionPointer().element()->typeId() == "Reference")
    {
        processReference(element, elementFullPath, resourcePath);
    }
    for (const auto& subName : element.subElementNames())
    {
        processSubElements(element, subName, elementFullPath, resourcePath);
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
void fhirtools::ReferenceFinder::processSubElements(const Element& element, const std::string& subName,
                                                    std::string_view elementFullPath, const std::string& resourcePath)
{
    const auto& subDef = element.definitionPointer().subField(subName);
    if (! subDef)
    {
        mResult.validationResults.add(Severity::debug, "undefined subfield: " + subName,
                                      std::string{elementFullPath}, element.definitionPointer().profile());
        return;
    }
    const auto elementType = getElementType(*subDef);
    if (! mFollowBundleEntry && elementType == ElementType::bundledResource)
    {
        return;
    }
    const bool isArray = subDef->element()->isArray();
    std::ostringstream subFullPathBase;
    subFullPathBase << elementFullPath << '.' << subName;
    const auto& commonSubPets = subProfiledElementTypes(*mView, subName, subFullPathBase.view());
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
            ReferenceFinder subFinder{
                mView,        commonSubPets, mCurrentResource, mFollowBundleEntry, mIsDocumentBundle,
                mBundleIndex, mOptions};
                if (elementType== ElementType::bundleEntry)
                {
                    subFinder.mFullUrl = fullUrlFromEntry(*subElement);
                }
                std::ostringstream subResourcePath;
                subResourcePath << resourcePath << '.' << subName;
                subFinder.findInternal(*subElement, subElementFullPath.view(), subResourcePath.str());
                mResult.merge(std::move(subFinder.mResult));
                switch (elementType)
                {
                    using enum ElementType;
                    case bundleEntry:
                        ++mBundleIndex;
                        break;
                    case resourceId:
                        mCurrentResource->id = subElement->asString();
                        break;
                    case bundle:
                    case bundledResource:
                    case containedResource:
                    case other:
                        break;
                }
        }
    }
}


// NOLINTNEXTLINE(misc-no-recursion, performance-unnecessary-value-param)
void ReferenceFinder::processResource(const Element& element, std::set<ProfiledElementTypeInfo> allSubPets,
                                      ElementType elementType, std::ostringstream&& elementFullPath)
{
    const auto& repo = element.getFhirStructureRepository();
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
    for (auto& pet : allSubPets)//NOLINT(readability-qualified-auto)
    {
        if (! pet.element()->isRoot() || resourceDef->isDerivedFrom(*pet.profile()))
        {
            continue;
        }
        resourcePets.emplace(std::move(pet));//NOLINT(hicpp-move-const-arg,performance-move-const-arg)
    }
    resourcePets.merge(profilesFromResource(element, elementFullPath.view()));
    if (resourcePets.empty())
    {
        resourcePets.emplace(*resourceDef);
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
    auto [resourceId, resultList] = element.resourceIdentity(elementFullPath.view());
    auto resourceInfo = std::make_shared<ResourceInfo>(ResourceInfo{
        .identity{std::move(resourceId)},
        .fullUrl = mFullUrl,
        .elementFullPath{elementFullPath.str()},
        .resourceRoot{element.shared_from_this()},
        .anchorType = anchorType(element, resourceHandling, isComposition),
        .referenceRequirement = referenceRequirement(resourceHandling),
        .parent = mCurrentResource,
    });
    ReferenceFinder subFinder{mView, resourcePets, resourceInfo, followBundleEntry, isDocumentBundle(isBundle, element),
                              0,     mOptions};
    subFinder.findInternal(element, elementFullPath.view(), std::string{});
    mResult.merge(std::move(subFinder.mResult));
    mCurrentResource->children.emplace_back(resourceInfo);
    mResult.referenceContext.addResource(std::move(resourceInfo));
}


std::set<ProfiledElementTypeInfo> ReferenceFinder::subProfiledElementTypes(const FhirStructureRepositoryView& repoView,
                                                                           std::string_view subFieldName,
                                                                           std::string_view subFullPathBase)
{
    std::set<ProfiledElementTypeInfo> result;
    for (const auto& pet : mProfiledElementTypes)
    {
        const auto& petSubDefs = pet.subDefinitions(subFieldName);
        result.insert(petSubDefs.begin(), petSubDefs.end());
        const auto& petSubField = pet.subField(subFieldName);
        if (petSubField)
        {
            addProfiles(repoView, petSubField->element()->profiles(), std::string{subFullPathBase}, pet.profile());
        }
    }
    return result;
}

std::set<ProfiledElementTypeInfo>
fhirtools::ReferenceFinder::sliceProfiledElementTypes(const fhirtools::Element& element,
                                                      const fhirtools::ProfiledElementTypeInfo& profiledElementTypeInfo)
{
    std::set<ProfiledElementTypeInfo> result;
    const auto* repo = std::addressof(element.getStructureDefinition()->repositoryBackend());
    const auto& fhirElement = profiledElementTypeInfo.element();
    const auto& slicing = fhirElement->slicing();
    if (slicing == nullptr)
    {
        return {};
    }
    for (const auto& slice : slicing->slices())
    {
        const auto& condition = slice.condition(repo, slicing->discriminators());
        if (condition->test(mView, element, mOptions))
        {
            static_assert(std::is_reference_v<decltype(slice.profile())>,
                          "slice.profile() must return reference to allow taking the address");
            result.emplace(slice.profile());
        }
    }
    return result;
}


std::set<ProfiledElementTypeInfo> fhirtools::ReferenceFinder::profilesFromResource(const fhirtools::Element& element,
                                                                                   std::string_view elementFullPath)
{
    using namespace std::string_literals;
    std::set<ProfiledElementTypeInfo> result;
    const auto& repo = element.getFhirStructureRepository();
    const auto& group = element.getStructureDefinition()->resourceGroup();
    for (const auto& profileUrl : element.profiles())
    {
        DefinitionKey profileKey{profileUrl};
        auto version = group->findVersion(profileKey).first;
        const auto* profile = version ? repo.findDefinition(profileKey.url, *version) : nullptr;
        if (! profile)
        {
            mResult.validationResults.add(Severity::debug, "undefined profile: "s.append(profileUrl),
                                          std::string{elementFullPath}, nullptr);
            continue;
        }
        mResult.validationResults.add(Severity::debug, "added profile: "s.append(profileUrl),
                                      std::string{elementFullPath}, nullptr);
        result.emplace(*profile);
    }
    return result;
}


void ReferenceFinder::processReference(const Element& element, std::string_view elementFullPath,
                                       std::string resourcePath)
{
    auto [referenceId, resultList] = element.referenceTargetIdentity(elementFullPath);
    mResult.validationResults.merge(std::move(resultList));
    ReferenceContext::ReferenceInfo refInfo{.identity = std::move(referenceId),
                                            .elementFullPath = std::string{elementFullPath},
                                            .localPath = std::move(resourcePath),
                                            .referencingElement{element.shared_from_this()},
                                            .targetProfileSets{}};
    const FhirStructureDefinition* type = getTypeFromReferenceElement(*mView, element, elementFullPath);
    for (const auto& pet : mProfiledElementTypes)
    {
        std::set<const FhirStructureDefinition*> profileSet;
        std::ostringstream targetProfiles;
        targetProfiles << '[';
        std::string_view sep;
        const auto& referenceTargetProfiles = pet.element()->referenceTargetProfiles();
        bool referenceTargetsOk = referenceTargetProfiles.empty();
        for (const auto& profileUrl : pet.element()->referenceTargetProfiles())
        {
            const auto* profile = mView->findStructure(DefinitionKey{profileUrl});
            if (! profile)
            {
                mResult.validationResults.add(Severity::debug, "profile not found: " + profileUrl,
                                              std::string{elementFullPath}, pet.profile());
                /// FIXME: Workaround for ERP-23967
                referenceTargetsOk = true;
                continue;
            }
            if (type != nullptr && ! (profile->isDerivedFrom(*type) || type->isDerivedFrom(*profile)))
            {
                continue;
            }
            referenceTargetsOk = true;
            targetProfiles << sep << profileUrl;
            sep = ", ";
            profileSet.emplace(profile);
        }
        if (!referenceTargetsOk)
        {
            std::ostringstream msg;
            msg << "Non of the allowed Target Profiles [";
            std::string_view sepProf;
            for (const auto& prof: referenceTargetProfiles)
            {
                msg << sepProf << '"' << prof << '"';
                sepProf = ", ";
            }
            msg << "] matches type: " << (type?type->urlAndVersion():"<undefined>");
            mResult.validationResults.add(Severity::error, msg.str(), std::string{elementFullPath} + ".type", pet.profile());
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

ReferenceFinder::ElementType ReferenceFinder::getElementType(const ProfiledElementTypeInfo& elementInfo)
{
    using namespace std::string_view_literals;
    if (elementInfo.profile()->kind() != FhirStructureDefinition::Kind::resource)
    {
        return ElementType::other;
    }
    const auto& elementPath = elementInfo.elementPath();
    if (elementPath == "id"sv)
    {
        return ElementType::resourceId;
    }
    // typeId must be resourceType, as kind() == resource
    const auto& resourceType = elementInfo.profile()->typeId();
    if (resourceType == "Bundle"sv)
    {
        return getBundleElementType(elementInfo, elementPath);
    }
    if (elementPath == "contained"sv &&
                elementInfo.profile()->isDerivedFrom(constants::domainResourceUrl))
    {
        return ElementType::containedResource;
    }
    return ElementType::other;
}

ReferenceFinder::ElementType
fhirtools::ReferenceFinder::getBundleElementType(const ProfiledElementTypeInfo& elementInfo,
                                                 std::string_view elementPath)
{
    using namespace std::string_view_literals;
    static constexpr std::string_view entry{"entry"};
    if (elementInfo.element()->isRoot())
    {
        return ElementType::bundle;
    }
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
    return ElementType::other;
}

std::optional<std::string> fhirtools::ReferenceFinder::fullUrlFromEntry(const Element& bundleEntry)
{
    std::ostringstream oss;
    bundleEntry.json(oss);
    auto fullUrls = bundleEntry.subElements("fullUrl");
    if (fullUrls.empty())
    {
        return std::nullopt;
    }
    const auto& fullUrl = fullUrls[0];
    if (!fullUrl->hasValue() || fullUrl->type() != Element::Type::String)
    {
        return std::nullopt;
    }
    return fullUrl->asString();
}

ReferenceContext::AnchorType fhirtools::ReferenceFinder::anchorType(const Element& element,
                                                                    ResourceHandling resourceHandling,
                                                                    bool isComposition)
{
    using AnchorType = ReferenceContext::AnchorType;
    if (resourceHandling == ResourceHandling::expectedComposition && isComposition)
    {
        return AnchorType::composition;
    }
    if (resourceHandling != ResourceHandling::contained &&
        element.definitionPointer().profile()->isDerivedFrom(constants::domainResourceUrl))
    {
        return AnchorType::contained;
    }
    return AnchorType::none;
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

const FhirStructureDefinition* ReferenceFinder::getTypeFromReferenceElement(const FhirStructureRepositoryView& repo,
                                                                            const Element& referenceElement,
                                                                            const std::string_view elementFullPath)
{
    auto typeElement = referenceElement.subElements("type");
    const FhirStructureDefinition* type = nullptr;
    if (! typeElement.empty() && typeElement[0]->type() == Element::Type::String)
    {
        auto typeStr = typeElement[0]->asString();
        type = repo.findTypeById(typeStr);
        if (! type)
        {
            std::string typeElementFullPath;
            typeElementFullPath.reserve(elementFullPath.size() + 5);
            typeElementFullPath.append(elementFullPath).append(".type");
            type = repo.findStructure(DefinitionKey{typeStr});
            if (type)
            {
                // http://www.hl7.org/fhir/references.html#type
                mResult.validationResults.add(Severity::error, "Urls only allowed for Logical Models.",
                                              typeElementFullPath, nullptr);
            }
            else
            {
                mResult.validationResults.add(Severity::error, "Unknown type: " + typeStr, typeElementFullPath,
                                              nullptr);
            }
        }
    }
    return type;
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
    addProfiles(*mView, sliceProfiles, elementFullPath);
}

// NOLINTNEXTLINE(misc-no-recursion)
void fhirtools::ReferenceFinder::addProfiles(const FhirStructureRepositoryView& repo,
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
void fhirtools::ReferenceFinder::addProfiles(const FhirStructureRepositoryView& repo,
                                             const std::list<std::string>& newProfileUrls,
                                             std::string_view elementFullPath,
                                             const FhirStructureDefinition* sourceProfile)
{
    std::set<ProfiledElementTypeInfo> newProfiles;
    for (const auto& profileUrl : newProfileUrls)
    {
        const auto* prof = repo.findStructure(DefinitionKey{profileUrl});
        if (! prof)
        {
            mResult.validationResults.add(Severity::debug, "profile not found: " + profileUrl,
                                          std::string{elementFullPath}, sourceProfile);
            continue;
        }
        newProfiles.emplace(*prof);
    }
    addProfiles(repo, newProfiles, elementFullPath);
}
