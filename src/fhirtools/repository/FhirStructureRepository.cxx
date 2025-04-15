/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "shared/fhir/Fhir.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/model/ValueElement.hxx"
#include "fhirtools/repository/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/FhirResourceViewVerifier.hxx"
#include "fhirtools/repository/internal/FhirStructureDefinitionParser.hxx"
#include "fhirtools/repository/internal/FhirStructureRepositoryFixer.hxx"
#include "fhirtools/typemodel/ProfiledElementTypeInfo.hxx"
#include "fhirtools/util/Constants.hxx"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/fusion/view/transform_view.hpp>
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

DefaultFhirStructureRepositoryView::DefaultFhirStructureRepositoryView(
    std::string initId, gsl::not_null<const FhirStructureRepositoryBackend*> backend)
    : mId{std::move(initId)}
    , mBackend(std::move(backend))
{
}

std::string_view fhirtools::DefaultFhirStructureRepositoryView::id() const
{
    return mId;
}

const FhirStructureDefinition* DefaultFhirStructureRepositoryView::findStructure(const DefinitionKey& key) const
{
    FPExpect(key.version.has_value(), "Unknown StructureDefinition: " + key.url);
    return mBackend->findDefinition(key.url, *key.version);
}

const FhirStructureDefinition* DefaultFhirStructureRepositoryView::findTypeById(const std::string& typeName) const
{
    return mBackend->findTypeById(typeName);
}

const FhirValueSet* DefaultFhirStructureRepositoryView::findValueSet(const DefinitionKey& key) const
{
    FPExpect(key.version.has_value(), "Unknown ValueSet: " + key.url);
    return mBackend->findValueSet(key.url, *key.version);
}

const FhirCodeSystem* DefaultFhirStructureRepositoryView::findCodeSystem(const DefinitionKey& key) const
{
    FPExpect(key.version.has_value(), "Unknown CodeSystem: " + key.url);
    return mBackend->findCodeSystem(key.url, *key.version);
}

const FhirStructureDefinition* DefaultFhirStructureRepositoryView::systemTypeBoolean() const
{
    return mBackend->systemTypeBoolean();
}

const FhirStructureDefinition* DefaultFhirStructureRepositoryView::systemTypeString() const
{
    return mBackend->systemTypeString();
}

const FhirStructureDefinition* DefaultFhirStructureRepositoryView::systemTypeDate() const
{
    return mBackend->systemTypeDate();
}

const FhirStructureDefinition* DefaultFhirStructureRepositoryView::systemTypeTime() const
{
    return mBackend->systemTypeTime();
}

const FhirStructureDefinition* DefaultFhirStructureRepositoryView::systemTypeDateTime() const
{
    return mBackend->systemTypeDateTime();
}

const FhirStructureDefinition* DefaultFhirStructureRepositoryView::systemTypeDecimal() const
{
    return mBackend->systemTypeDecimal();
}

const FhirStructureDefinition* DefaultFhirStructureRepositoryView::systemTypeInteger() const
{
    return mBackend->systemTypeInteger();
}

FhirStructureRepository::ContentReferenceResolution
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
    : m_defaultFhirStructureRepositoryView(
          std::make_shared<const fhirtools::DefaultFhirStructureRepositoryView>("default", this))
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
    std::list<std::pair<FhirCodeSystem, std::filesystem::path>> supplements;
    for (const auto& file : filesAndDirectories)
    {
        if (is_regular_file(file))
        {
            if (!loadFile(supplements, file, groupResolver))
            {
                success = false;
            }
        }
        else if (is_directory(file))
        {
            for (const auto& dirEntry : std::filesystem::directory_iterator{file})
            {
                FPExpect(dirEntry.is_regular_file(), "only regular files supported in profile directories.");
                if (!loadFile(supplements, dirEntry.path(), groupResolver))
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
    processCodeSystemSupplements(supplements, groupResolver);
    TVLOG(2) << "done loading";
    finalizeValueSets();
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
    builder.synthesized().url(url).version(version).initGroup(groupResolver, {});
    addCodeSystem(std::make_unique<FhirCodeSystem>(builder.getAndReset()));
}

void fhirtools::FhirStructureRepositoryBackend::synthesizeValueSet(const std::string& url, const FhirVersion& version,
                                                                   const FhirResourceGroupResolver& groupResolver)
{
    mergeGroups(groupResolver.allGroups());
    FhirValueSet::Builder builder{};
    builder.url(url).version(version).initGroup(groupResolver, {});
    addValueSet(std::make_unique<FhirValueSet>(builder.getAndReset()));
    finalizeValueSets();
}

void FhirStructureRepositoryBackend::finalizeValueSets()
{
    for (const auto& valueSetKeyValue : mValueSetsByKey)
    {
        const auto& valueSet = valueSetKeyValue.second;
        if (! valueSet->finalized())
        {
            valueSet->finalize(this);
        }
    }
}

bool FhirStructureRepositoryBackend::loadFile(std::list<std::pair<FhirCodeSystem, std::filesystem::path>>& supplements,
                                              const std::filesystem::path& file,
                                              const FhirResourceGroupResolver& groupResolver)
{
    bool success = true;
    try
    {
        TVLOG(2) << "loading: " << file;
        auto [definitions, codeSystems, valueSets] = FhirStructureDefinitionParser::parse(file, groupResolver);
        for (auto&& def : definitions)
        {
            addDefinition(std::make_unique<FhirStructureDefinition>(std::move(def)));
        }
        for (auto&& codeSystem : codeSystems)
        {
            if (codeSystem.getContentType() == FhirCodeSystem::ContentType::supplement)
            {
                supplements.emplace_back(std::move(codeSystem), file);
            }
            else
            {
                addCodeSystem(std::make_unique<FhirCodeSystem>(std::move(codeSystem)));
            }
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

fhirtools::FhirStructureRepository::ContentReferenceResolution
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
    return resolveBaseContentReference(ProfiledElementTypeInfo{profile}, 0, dot, elementId);
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
        return resolveBaseContentReference(ProfiledElementTypeInfo{pet.profile(), element}, index, nextDot, originalRef);
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
            return resolveBaseContentReference(ProfiledElementTypeInfo{profile, profile->rootElement()},0, dot, originalRef);
        }
        else
        {
            auto [subPet, idx] =  resolveBaseContentReference(pet.element()->contentReference());
            return resolveBaseContentReference(std::move(subPet), idx, dot, originalRef);
        }
    }
}

std::shared_ptr<const fhirtools::DefaultFhirStructureRepositoryView> FhirStructureRepositoryBackend::defaultView() const
{
    return m_defaultFhirStructureRepositoryView;
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
        .typeId(urlString)
        .url(urlString)
        .addElement(FhirElement::Builder{}.name(urlString).isArray(true).isRoot(true).getAndReset(), {})
        .initGroup(*mSystemGroupResolver, {});
    return addDefinition(std::make_unique<FhirStructureDefinition>(builder.getAndReset()));
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
}

void fhirtools::FhirStructureRepositoryBackend::addValueSet(std::unique_ptr<FhirValueSet>&& valueSet)
{
    auto [it, inserted] =
        mValueSetsByKey.try_emplace(DefinitionKey(valueSet->getUrl(), valueSet->getVersion()), std::move(valueSet));
    FPExpect(inserted, "Duplicate ValueSet: " + valueSet->getUrl() + '|' + to_string(valueSet->getVersion()));
    TVLOG(3) << "ValueSet inserted: " << it->first;
}

void fhirtools::FhirStructureRepositoryBackend::processCodeSystemSupplements(
    const std::list<std::pair<FhirCodeSystem, std::filesystem::path>>& supplements,
    const FhirResourceGroupResolver& groupResolver)
{
    using namespace std::string_literals;
    for (auto&& supplement : supplements)
    {
        FPExpect(! supplement.first.getSupplements().empty(),
                 "CodeSystem.content=supplement, but CodeSystem.supplements is not defined.");
        auto [key, group] = supplement.first.resourceGroup()->find(DefinitionKey{supplement.first.getSupplements()});
        FhirCodeSystem* cs{};
        if (key.version)
        {
            auto codeSystem = mCodeSystemsByKey.find(key);
            if (codeSystem != mCodeSystemsByKey.end())
            {
                cs = codeSystem->second.get();
            }
        }
        auto builder = cs ? FhirCodeSystem::Builder(*cs) : FhirCodeSystem::Builder();
        if (cs == nullptr)
        {
            TVLOG(1) << "CodeSystem.supplements not found, synthesizing: " << key;
            builder.synthesized();
            builder.url(key.url);
            builder.version(FhirVersion::notVersioned);
            builder.initGroup(groupResolver, {});
        }
        for (const auto& code : supplement.first.getCodes())
        {
            builder.code(code.code);
        }
        if (cs)
        {
            *cs = builder.getAndReset();
        }
        else
        {
            addCodeSystem(std::make_unique<FhirCodeSystem>(builder.getAndReset()));
        }
    }
}
