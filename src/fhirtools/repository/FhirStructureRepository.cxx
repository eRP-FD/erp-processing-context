/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/model/ValueElement.hxx"
#include "fhirtools/repository/groups/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/internal/FhirStructureDefinitionParser.hxx"
#include "fhirtools/repository/internal/FhirStructureRepositoryFixer.hxx"
#include "fhirtools/repository/views/FhirResourceViewVerifier.hxx"
#include "fhirtools/typemodel/ProfiledElementTypeInfo.hxx"
#include "fhirtools/util/Constants.hxx"
#include "shared/fhir/Fhir.hxx"

#include <boost/algorithm/string.hpp>
#include <boost/fusion/view/transform_view.hpp>
#include <algorithm>
#include <ranges>
#include <set>
#include <utility>

using fhirtools::FhirStructureDefinition;
using fhirtools::FhirStructureRepositoryBackend;

template<typename Self>
decltype(auto) FhirStructureRepositoryBackend::findValueSetHelper(Self* self, const std::string_view url,
                                                                  const fhirtools::FhirVersion& version)
{
    DefinitionKey key{std::string{url}, version};
    auto candidate = self->mValueSetsByKey.find(key);
    if (candidate != self->mValueSetsByKey.end())
    {
        return candidate->second.get();
    }
    return static_cast<FhirValueSet*>(nullptr);
}

/// @brief does consistency checks after the loading all structure definitions.

fhirtools::DefinitionKey::DefinitionKey(std::string url, std::optional<FhirVersion> version)
    : url(std::move(url))
    , version(std::move(version))
{
}


fhirtools::DefinitionKey::DefinitionKey(std::string_view urlWithVersion)
{
    size_t delim = urlWithVersion.rfind('|');
    url = urlWithVersion.substr(0, delim);
    if (delim != std::string_view::npos)
    {
        version.emplace(std::string{urlWithVersion.substr(delim + 1)});
    }
}


namespace fhirtools
{


FhirStructureRepositoryView::ContentReferenceResolution
DefaultFhirStructureRepositoryView::resolveContentReference(const FhirResourceGroup& group,
                                                            const FhirElement& element) const
{
    return mBackend->resolveContentReference(group, element);
}

std::tuple<ProfiledElementTypeInfo, size_t>
DefaultFhirStructureRepositoryView::resolveBaseContentReference(std::string_view elementId) const
{
    return mBackend->resolveBaseContentReference(elementId);
}

}


FhirStructureRepositoryBackend::FhirStructureRepositoryBackend()
    : mDefaultFhirStructureRepositoryView(fhirtools::DefaultFhirStructureRepositoryView::create("default", this))
    , mSystemGroupResolver{std::make_shared<FhirResourceGroupConst>("system")}
{
    addSystemTypes();
}

FhirStructureRepositoryBackend::~FhirStructureRepositoryBackend() = default;

void FhirStructureRepositoryBackend::load(const std::list<std::filesystem::path>& filesAndDirectories,
                                          const FhirResourceGroupResolver& groupResolver)
{
    bool success = true;
    TVLOG(1) << "Loading FHIR structure definitions.";
    mergeGroups(groupResolver.allGroups());
    for (const auto& file : filesAndDirectories)
    {
        if (is_regular_file(file))
        {
            if (!loadFile(file, groupResolver))
            {
                success = false;
            }
        }
        else if (is_directory(file))
        {
            for (const auto& dirEntry : std::filesystem::directory_iterator{file})
            {
                FPExpect(dirEntry.is_regular_file(),
                         "only regular files supported in profile directories: " + dirEntry.path().native());
                if (!loadFile(dirEntry.path(), groupResolver))
                {
                    success = false;
                }
            }
        }
        else
        {
            FPFail("unsupported path: " + file.string());
        }
    }
    TVLOG(2) << "done loading";
    FhirStructureRepositoryFixer repoFixer{*this};
    repoFixer.fix();
    FhirResourceViewVerifier verifier(*this);
    verifier.verify();
    if (!success)
    {
        Fail2("error loading repository", std::logic_error);
    }
}

void fhirtools::FhirStructureRepositoryBackend::synthesizeCodeSystem(const std::string& url, const FhirVersion& version,
                                                                     const FhirResourceGroupResolver& groupResolver)
{
    mergeGroups(groupResolver.allGroups());
    FhirCodeSystem::Builder builder{};
    builder.synthesized().url(url).version(version).initGroup(groupResolver);
    addCodeSystem(std::make_unique<FhirCodeSystem>(builder.getAndReset()));
}

void fhirtools::FhirStructureRepositoryBackend::synthesizeValueSet(const std::string& url, const FhirVersion& version,
                                                                   const FhirResourceGroupResolver& groupResolver)
{
    mergeGroups(groupResolver.allGroups());
    FhirValueSet::Builder builder{};
    builder.url(url).version(version).initGroup(groupResolver);
    addValueSet(std::make_unique<FhirValueSet>(builder.getAndReset()));
}

bool FhirStructureRepositoryBackend::loadFile(const std::filesystem::path& file,
                                              const FhirResourceGroupResolver& groupResolver)
{
    bool success = true;
    try
    {
        TVLOG(2) << "loading: " << file;
        auto [definitions, codeSystems, valueSets] = FhirStructureDefinitionParser::parse(file, this, groupResolver);
        for (auto&& def : definitions)
        {
            addDefinition(std::move(def));
        }
        for (auto&& codeSystem : codeSystems)
        {
            addCodeSystem(std::make_unique<FhirCodeSystem>(std::move(codeSystem)));
        }
        for (auto&& valueSet : valueSets)
        {
            try {
                addValueSet(std::make_unique<FhirValueSet>(std::move(valueSet)));
            }
            catch (const std::exception& ex)
            {
                LOG(ERROR) << file.string() + ": " + ex.what();
                success = false;
            }
        }
    }
    catch (const std::exception& ex)
    {
        LOG(ERROR) << file.string() + ": " + ex.what();
        success = false;
    }
    return success;
}


void FhirStructureRepositoryBackend::mergeGroups(const std::map<std::string, std::shared_ptr<const FhirResourceGroup>>& groups)
{
    for (const auto& group : groups)
    {
        if (const auto& original = mAllGroups.try_emplace(group.first, group.second); ! original.second)
        {
            FPExpect3(group.second.get() == original.first->second.get(),
                      "Two FhirResourceGroup instances for the same group: " + original.first->first, std::logic_error);
        }
    }
}


const FhirStructureDefinition* FhirStructureRepositoryBackend::findDefinition(std::string_view url,
                                                                              const FhirVersion& version) const
{
    DefinitionKey key{std::string{url}, version};
    const auto& item = mDefinitionsByKey.find(key);
    return (item != mDefinitionsByKey.end()) ? item->second.get() : (nullptr);
}

const FhirStructureDefinition* FhirStructureRepositoryBackend::findTypeById(const std::string& typeName) const
{
    auto item = mDefinitionsByTypeId.find(typeName);
    if (item == mDefinitionsByTypeId.end() && ! typeName.empty())
    {
        const auto& ctype = std::use_facet<std::ctype<char>>(std::locale::classic());
        std::string typeNameLower = typeName;
        typeNameLower[0] = ctype.tolower(typeNameLower[0]);
        item = mDefinitionsByTypeId.find(typeNameLower);
    }
    return (item != mDefinitionsByTypeId.end()) ? (item->second) : (nullptr);
}

fhirtools::FhirValueSet* fhirtools::FhirStructureRepositoryBackend::findValueSet(const std::string_view url,
                                                                                 const FhirVersion& version)
{
    return findValueSetHelper(this, url, version);
}
const fhirtools::FhirValueSet* fhirtools::FhirStructureRepositoryBackend::findValueSet(const std::string_view url,
                                                                                       const FhirVersion& version) const
{
    return findValueSetHelper(this, url, version);
}

const fhirtools::FhirCodeSystem*
fhirtools::FhirStructureRepositoryBackend::findCodeSystem(const std::string_view url, const FhirVersion& version) const
{
    DefinitionKey key{std::string{url}, version};
    auto candidate = mCodeSystemsByKey.find(key);
    if (candidate != mCodeSystemsByKey.end())
    {
        return candidate->second.get();
    }
    return nullptr;
}

std::set<const fhirtools::FhirCodeSystem*>
fhirtools::FhirStructureRepositoryBackend::findSupplementers(const std::string& url,
                                                             const FhirVersion& version) const
{
    using std::ranges::copy_if;
    using std::views::values;

    std::set<const FhirCodeSystem*> result;
    auto supplementersPair = mSupplementsByUrl.equal_range(url);
    std::ranges::subrange supplementers{supplementersPair.first, supplementersPair.second};
    copy_if(supplementers | values, inserter(result, result.begin()), [&version](const FhirCodeSystem* cs) {
        auto supp = value(cs->getSupplements());
        return !supp.version.has_value() || *supp.version == version;
    });
    return result;
}


fhirtools::FhirStructureRepositoryView::ContentReferenceResolution
FhirStructureRepositoryBackend::resolveContentReference(const FhirResourceGroup& group,
                                                        const FhirElement& element) const
{
    using namespace std::string_literals;
    const auto& contentReference = element.contentReference();
    Expect3(! contentReference.empty(), "Cannot resolve empty contentReference", std::logic_error);
    const auto hash = contentReference.find('#');
    auto elementName = contentReference.substr(hash + 1);
    const FhirStructureDefinition* baseType = nullptr;
    if (hash != 0)
    {
        auto baseTypeKey = group.find(DefinitionKey{contentReference.substr(0, hash)}).first;
        baseType = baseTypeKey.version ? findDefinition(baseTypeKey.url, *baseTypeKey.version) : nullptr;
        FPExpect3(baseType,
                  ("Failed to resolve baseType for contentReference of " + element.originalName() + " in group (")
                          .append(group.id()) +
                      "): " + to_string(baseTypeKey) + '#' + elementName,
                  std::logic_error);
    }
    auto dot = contentReference.find('.', hash);
    Expect3(dot != std::string::npos, "Missing dot in contentReference: "s.append(contentReference), std::logic_error);
    const std::string baseTypeId{contentReference.substr(hash + 1, dot - hash - 1)};
    if (baseType)
    {
        Expect3(baseType->typeId() == baseTypeId, "Type mismatch between reference and referenced url",
                std::logic_error);
    }
    else
    {
        baseType = findTypeById(baseTypeId);
    }
    Expect3(baseType != nullptr, "Failed to find referenced type for: "s.append(contentReference), std::logic_error);

    const auto& baseElements = baseType->elements();
    const auto& refElementIt =
        std::ranges::find_if(baseElements, [&](const std::shared_ptr<const FhirElement>& xelement) {
            return xelement->name() == elementName;
        });
    size_t elementIndex = 0;
    std::shared_ptr<const FhirElement> refElement;
    if ((refElementIt == baseElements.end() && hash == 0) ||
        (refElementIt != baseElements.end() && (*refElementIt)->typeId().empty()))
    {
        auto [ptr, idx] = resolveBaseContentReference(elementName);
        elementIndex = idx;
        baseType = ptr.profile();
        refElement = ptr.element();
    }
    else
    {
        elementIndex = gsl::narrow<size_t>(refElementIt - baseElements.begin());
        refElement = *refElementIt;
    }
    Expect3(refElement != nullptr, "Element referenced Element not found: "s.append(contentReference),
            std::logic_error);
    const auto* refElementType = findTypeById((refElement)->typeId());
    Expect3(refElementType != nullptr, "Failed to resolve type of referenced element: " + refElement->name(),
            std::logic_error);
    refElement = FhirElement::Builder{*refElement}
                     .isArray(element.isArray())
                     .cardinalityMin(element.cardinality().min)
                     .cardinalityMax(element.cardinality().max)
                     .getAndReset();
    return {*baseType, *refElementType, elementIndex, std::move(refElement)};
}

std::tuple<fhirtools::ProfiledElementTypeInfo, size_t>
//NOLINTNEXTLINE(misc-no-recursion)
FhirStructureRepositoryBackend::resolveBaseContentReference(std::string_view elementId) const
{
    using std::min;
    using namespace std::string_literals;
    auto hash = elementId.find('#');
    if (hash == std::string::npos)
    {
        hash = 0;
    }
    else
    {
        ++hash;
    }
    size_t dot = elementId.find('.', hash);
    auto typeId = elementId.substr(hash, dot - hash);
    Expect(! typeId.empty(), "Cannot determine typeId from: "s.append(elementId));
    const auto* profile = findTypeById(std::string{typeId});
    Expect(profile != nullptr, "Profile not found for: "s.append(elementId));
    return resolveBaseContentReference(ProfiledElementTypeInfo{*profile}, 0, dot, elementId);
}

std::tuple<fhirtools::ProfiledElementTypeInfo, size_t>
//NOLINTNEXTLINE(misc-no-recursion)
FhirStructureRepositoryBackend::resolveBaseContentReference(ProfiledElementTypeInfo&& pet, size_t index, size_t dot, std::string_view originalRef) const
{
    using namespace std::string_literals;
    using namespace std::string_view_literals;
    if (dot == std::string_view::npos)
    {
        return std::forward_as_tuple(std::move(pet), index);
    }
    if (pet.element()->isBackbone())
    {
        auto nextDot = originalRef.find('.', dot + 1);
        std::string elementName = pet.element()->name();
        elementName += originalRef.substr(dot, nextDot - dot);
        std::shared_ptr<const FhirElement> element;
        std::tie(element, index) = pet.profile()->findElementAndIndex(elementName);
        FPExpect(element != nullptr, ("failed to resolve element `"s += elementName += "` while resolving: "sv) += originalRef);
        return resolveBaseContentReference(ProfiledElementTypeInfo{element}, index, nextDot, originalRef);
    }
    else
    {
        if (pet.element()->contentReference().empty())
        {
            const auto* profile = findTypeById(pet.element()->typeId());
            Expect3(profile,
                    "failed to resolve Definition for "s.append(originalRef) +
                        " no such type: " + pet.element()->typeId(),
                    std::logic_error);
            return resolveBaseContentReference(ProfiledElementTypeInfo{*profile},0, dot, originalRef);
        }
        else
        {
            auto [subPet, idx] =  resolveBaseContentReference(pet.element()->contentReference());
            return resolveBaseContentReference(std::move(subPet), idx, dot, originalRef);
        }
    }
}

std::shared_ptr<const fhirtools::FhirStructureRepositoryView> FhirStructureRepositoryBackend::defaultView() const
{
    return mDefaultFhirStructureRepositoryView;
}

const std::map<std::string, std::shared_ptr<const fhirtools::FhirResourceGroup>>&
fhirtools::FhirStructureRepositoryBackend::allGroups() const
{
    return mAllGroups;
}

const std::unordered_map<fhirtools::DefinitionKey, std::unique_ptr<fhirtools::FhirCodeSystem>>&
fhirtools::FhirStructureRepositoryBackend::codeSystemsByKey() const
{
    return mCodeSystemsByKey;
}

const std::unordered_map<fhirtools::DefinitionKey, std::unique_ptr<FhirStructureDefinition>>&
fhirtools::FhirStructureRepositoryBackend::definitionsByKey() const
{
    return mDefinitionsByKey;
}

const std::unordered_map<fhirtools::DefinitionKey, std::unique_ptr<fhirtools::FhirValueSet>>&
fhirtools::FhirStructureRepositoryBackend::valueSetsByKey() const
{
    return mValueSetsByKey;
}

void FhirStructureRepositoryBackend::addSystemTypes()
{
    using K = FhirStructureDefinition::Kind;
    // clang-format off
    mSystemTypeBoolean  = addSystemType(K::systemBoolean , constants::systemTypeBoolean);
    mSystemTypeString   = addSystemType(K::systemString  , constants::systemTypeString);
    mSystemTypeDate     = addSystemType(K::systemDate    , constants::systemTypeDate);
    mSystemTypeTime     = addSystemType(K::systemTime    , constants::systemTypeTime);
    mSystemTypeDateTime = addSystemType(K::systemDateTime, constants::systemTypeDateTime);
    mSystemTypeDecimal  = addSystemType(K::systemDouble  , constants::systemTypeDecimal);
    mSystemTypeInteger  = addSystemType(K::systemInteger , constants::systemTypeInteger);
    // clang-format on
}

const FhirStructureDefinition* FhirStructureRepositoryBackend::addSystemType(FhirStructureDefinition::Kind kind,
                                                                             const std::string_view& url)
{
    std::string urlString{url};
    using namespace std::string_literals;
    FhirStructureDefinition::Builder builder;
    builder.kind(kind)
        .repositoryBackend(this)
        .typeId(urlString)
        .url(urlString)
        .addElement(std::move(FhirElement::Builder{}.name(urlString).isArray(true).isRoot(true)), {})
        .initGroup(*mSystemGroupResolver);
    return addDefinition(builder.getAndReset());
}

const FhirStructureDefinition*
FhirStructureRepositoryBackend::addDefinition(std::unique_ptr<FhirStructureDefinition>&& definition)
{
    if (definition->derivation() != FhirStructureDefinition::Derivation::constraint)
    {
        auto [iter, inserted] = mDefinitionsByTypeId.try_emplace(definition->typeId(), definition.get());
        Expect3(inserted, "Duplicate type: " + definition->typeId(), std::logic_error);
    }
    auto [iter, inserted] =
        mDefinitionsByKey.try_emplace(DefinitionKey{definition->url(), definition->version()}, std::move(definition));
    if (! inserted)
    {
        Fail("Duplicate profile: " + to_string(iter->first));
    }
    return iter->second.get();
}

void fhirtools::FhirStructureRepositoryBackend::addCodeSystem(std::unique_ptr<FhirCodeSystem>&& codeSystem)
{
    auto [it, inserted] = mCodeSystemsByKey.try_emplace(DefinitionKey(codeSystem->getUrl(), codeSystem->getVersion()),
                                                        std::move(codeSystem));
    FPExpect(inserted,
             "Duplicate CodeSystem: " + to_string(DefinitionKey{codeSystem->getUrl(), codeSystem->getVersion()}));
    if (auto supplements = it->second->getSupplements())
    {
        mSupplementsByUrl.emplace(supplements->url, std::to_address(it->second));
    }
}

void fhirtools::FhirStructureRepositoryBackend::addValueSet(std::unique_ptr<FhirValueSet>&& valueSet)
{
    auto [it, inserted] =
        mValueSetsByKey.try_emplace(DefinitionKey(valueSet->getUrl(), valueSet->getVersion()), std::move(valueSet));
    FPExpect(inserted, "Duplicate ValueSet: " + valueSet->getUrl() + '|' + to_string(valueSet->getVersion()));
    TVLOG(3) << "ValueSet inserted: " << it->first;
}
