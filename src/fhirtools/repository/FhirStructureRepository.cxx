/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "erp/fhir/Fhir.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/model/ValueElement.hxx"
#include "fhirtools/repository/internal/FhirStructureDefinitionParser.hxx"
#include "fhirtools/repository/internal/FhirStructureRepositoryFixer.hxx"
#include "fhirtools/typemodel/ProfiledElementTypeInfo.hxx"
#include "fhirtools/util/Constants.hxx"

#include <boost/algorithm/string.hpp>
#include <ranges>
#include <set>
#include <utility>

using fhirtools::FhirStructureDefinition;
using fhirtools::FhirStructureRepository;

/// @brief does consistency checks after the loading all structure definitions.
class FhirStructureRepository::Verifier
{
public:
    explicit Verifier(FhirStructureRepository& repo)
        : mRepo{repo}
    {
    }

    void verify()
    {
        for (const auto& def : mRepo.mLatestDefinitionsByUrl)
        {
            verifyStructure(*def.second);
        }
        if (! unresolvedBase.empty())
        {
            LOG(ERROR) << R"(Could not resolve FHIR baseDefinitions: [")" + boost::join(unresolvedBase, R"(", ")") +
                              R"("])";
            mVerfied = false;
        }
        if (! missingType.empty())
        {
            LOG(ERROR) << R"(Could not find FHIR types: [")" + boost::join(missingType, R"(", ")") + R"("])";
            mVerfied = false;
        }
        if (! elementsWithUnresolvedType.empty())
        {
            LOG(ERROR) << R"(Could not resolve FHIR types for: [")" +
                              boost::join(elementsWithUnresolvedType, R"(", ")") + R"("])";
            mVerfied = false;
        }
        if (! unresolvedProfiles.empty())
        {
            LOG(ERROR) << R"(Could not resolve FHIR profiles: [")" + boost::join(unresolvedProfiles, R"(", ")") +
                              R"("])";
            mVerfied = false;
        }
        if (! unresolvedBindings.empty())
        {
            LOG(ERROR) << R"(Could not resolve ValueSet Bindings: [")" + boost::join(unresolvedBindings, R"(", ")") +
                              R"("])";
        }

        //verifyValueSetsFull();
        verifyValueSetsRequiredOnly();
        if (! unresolvedCodeSystems.empty())
        {
            LOG(ERROR) << R"(Could not resolve CodeSystems: [")" + boost::join(unresolvedCodeSystems, R"(", ")") +
                              R"("])";
        }
        if (! unresolvedValueSets.empty())
        {
            LOG(ERROR) << R"(Could not resolve ValueSets: [")" + boost::join(unresolvedValueSets, R"(", ")") + R"("])";
        }
        Expect3(mVerfied, "FHIR-Structure-Repository verification failed", std::logic_error);
    }

    const std::set<std::string>& getRequiredValueSets() const
    {
        return requiredValueSets;
    }

private:
    void verifyStructure(const FhirStructureDefinition& def)
    {
        const auto& baseDefinition = def.baseDefinition();
        if (! baseDefinition.empty())
        {
            if (def.derivation() == FhirStructureDefinition::Derivation::basetype)
            {
                LOG(ERROR) << "Structure definition has derivation 'basetype', but baseDefinition is defined: "
                           << def.url() << '|' << def.version();
                mVerfied = false;
            }
            const auto* baseType = mRepo.findDefinitionByUrl(baseDefinition);
            if (! baseType)
            {
                unresolvedBase.insert(baseDefinition);
                LOG(ERROR) << "Failed to resolve base type for " << def.url() << '|' << def.version() << ": "
                           << baseDefinition;
            }
        }
        else
        {
            if (def.derivation() != FhirStructureDefinition::Derivation::basetype)
            {
                LOG(ERROR) << "Profile has derivation '" << def.derivation()
                           << "', but baseDefinition is not defined: " << def.url() << '|' << def.version();
                mVerfied = false;
            }
        }
        for (const auto& element : def.elements())
        {
            Expect3(element != nullptr, "ElementDefinition must not be null.", std::logic_error);
            verifyElement(def, *element);
        }
    }

    //NOLINTNEXTLINE(misc-no-recursion)
    void verifyElement(const FhirStructureDefinition& def, const FhirElement& element)
    {
        using boost::starts_with;
        if (element.name() != def.typeId())
        {
            const auto& elementType = element.typeId();
            if (elementType.empty())
            {
                verifyContentReference(def, element);
            }
            else if (not starts_with(elementType, "http://hl7.org/fhirpath/System."))
            {
                const auto* typeDefinition = mRepo.findTypeById(elementType);
                if (! typeDefinition)
                {
                    missingType.insert(elementType);
                    LOG(ERROR) << "Failed to resolve type for " << def.url() << '|' << def.version() << "@"
                               << element.originalName() << ": " << elementType;
                }
            }
        }
        verifyElementProfiles(def, element);
        verifyBinding(element);
        verifySlicing(def, element);
        parseConstraints(def, element);
        verifyFixedCodeSystems(element);
    }

    void verifyElementProfiles(const FhirStructureDefinition& def, const FhirElement& element)
    {
        for (const auto& profile : element.profiles())
        {
            if (! mRepo.findDefinitionByUrl(profile))
            {
                unresolvedProfiles.emplace(profile);
                LOG(ERROR) << "Failed to resolve profile type for " << def.url() << '|' << def.version() << "@"
                           << element.originalName() << ": " << profile;
            }
        }
        for (const auto& profile : element.referenceTargetProfiles())
        {
            if (! mRepo.findDefinitionByUrl(profile))
            {
                unresolvedProfiles.emplace(profile);
                LOG(ERROR) << "Failed to resolve targetProfile type for " << def.url() << '|' << def.version() << "@"
                           << element.originalName() << ": " << profile;
            }
        }
    }

    void verifyBinding(const FhirElement& element)
    {
        if (element.hasBinding() && element.binding().strength == FhirElement::BindingStrength::required)
        {
            const auto* valueSet = mRepo.findValueSet(element.binding().valueSetUrl, element.binding().valueSetVersion);
            if (tryResolve(element.binding().valueSetUrl, element.binding().valueSetVersion,
                           element.binding().strength))
            {
                const auto verStr =
                    (element.binding().valueSetVersion.has_value() ? "|" + *element.binding().valueSetVersion : "");
                requiredValueSets.insert(element.binding().valueSetUrl + verStr);
                for (const auto& include : valueSet->getIncludes())
                {
                    for (const auto& item : include.valueSets)
                    {
                        if (tryResolve(item, {}, element.binding().strength))
                        {
                            requiredValueSets.insert(item);
                        }
                    }
                }
            }
        }
    }

    bool tryResolve(const std::string& valueSetUrl, const std::optional<std::string>& version,
                    FhirElement::BindingStrength bindingStrength)
    {
        const auto* valueSet = mRepo.findValueSet(valueSetUrl, version);
        if (! valueSet)
        {
            std::stringstream msg;
            std::string valuesetUrlWithVersion = valueSetUrl + "|" + version.value_or("<no-ver>");
            msg << "Failed to resolve " << magic_enum::enum_name(bindingStrength) << " binding '"
                << valuesetUrlWithVersion << "'";
            switch (bindingStrength)
            {
                case FhirElement::BindingStrength::required:
                    LOG(ERROR) << msg.str();
                    unresolvedBindings.insert(valuesetUrlWithVersion);
                    break;
                case FhirElement::BindingStrength::example:
                    VLOG(2) << msg.str();
                    break;
                case FhirElement::BindingStrength::extensible:
                case FhirElement::BindingStrength::preferred:
                    LOG(WARNING) << msg.str();
                    break;
            }
        }
        return valueSet != nullptr;
    }

    void verifyContentReference(const FhirStructureDefinition& def, const FhirElement& element)
    {
        if (element.contentReference().empty())
        {
            elementsWithUnresolvedType.emplace(def.url() + "@" + element.name());
            LOG(ERROR) << "Failed to resolve element type for " << def.url() << '|' << def.version() << "@"
                       << element.originalName();
        }
        else
        {
            try
            {
                (void) mRepo.resolveContentReference(element);
            }
            catch (const std::logic_error&)
            {
                LOG(ERROR) << "Failed to resolve element type for " << def.url() << '|' << def.version() << "@"
                           << element.originalName() << ": " + element.contentReference();
                elementsWithUnresolvedType.emplace(def.url() + "@" + element.name());
            }
        }
    }

    //NOLINTNEXTLINE(misc-no-recursion)
    void verifySlicing(const FhirStructureDefinition& def, const FhirElement& element)
    {
        const auto& slicing = element.slicing();
        if (! slicing)
        {
            return;
        }
        for (const auto& slice : slicing->slices())
        {
            const FhirStructureDefinition& profile = slice.profile();
            for (const auto& disc : slicing->discriminators())
            {
                try
                {
                    (void) disc.condition(mRepo, &profile);
                    TVLOG(4) << "Verified " << def.url() << '|' << def.version() << ":" << element.originalName() << ":"
                             << slice.name() << " discriminator: " << magic_enum::enum_name(disc.type()) << '@'
                             << disc.path();
                }
                catch (const std::exception& ex)
                {
                    mVerfied = false;
                    LOG(ERROR) << "Slice verification failed " << def.url() << '|' << def.version() << ":"
                               << element.originalName() << ":" << slice.name() << ": " << ex.what();
                }
            }
            for (const auto& sliceElement : profile.elements())
            {
                verifyElement(profile, *sliceElement);
            }
        }
    }

    void parseConstraints(const FhirStructureDefinition& def, const FhirElement& element)
    {
        for (const auto& constraint : element.getConstraints())
        {
            try
            {
                (void) constraint.parsedExpression(mRepo, &mExpressionCache);
            }
            catch (const std::exception& ex)
            {
                LOG(ERROR) << ex.what() << ":" << def.url() << '|' << def.version() << "@" << element.originalName()
                           << ": " << constraint.getExpression();
                mVerfied = false;
            }
        }
    }

    void verifyFixedCodeSystems(const FhirElement& element)
    {
        if (element.pattern() != nullptr)
        {
            ValueElement valueElement(&mRepo, element.pattern());
            verifyFixedCodeSystems(element, valueElement);
        }
        if (element.fixed() != nullptr)
        {
            ValueElement valueElement(&mRepo, element.fixed());
            verifyFixedCodeSystems(element, valueElement);
        }
    }

    void verifyFixedCodeSystems(const FhirElement& element, const ValueElement& fixedOrPattern)
    {
        if (element.typeId() == "CodeableConcept")
        {
            auto fixedCodings = fixedOrPattern.subElements("coding");
            for (const auto& fixedCoding : fixedCodings)
            {
                auto codingSystems = fixedCoding->subElements("system");
                if (codingSystems.size() == 1)
                {
                    verifyFixedCodeSystems(codingSystems[0]->asString());
                }
            }
        }
        else if (element.typeId() == "Coding")
        {
            auto codingSystems = fixedOrPattern.subElements("system");
            if (codingSystems.size() == 1)
            {
                verifyFixedCodeSystems(codingSystems[0]->asString());
            }
        }
        else if (element.name().ends_with("coding.system"))
        {
            verifyFixedCodeSystems(fixedOrPattern.asString());
        }
    }

    void verifyFixedCodeSystems(const std::string& codeSystemUrl)
    {
        const auto* codeSystem = mRepo.findCodeSystem(codeSystemUrl, {});
        if (! codeSystem || codeSystem->isSynthesized())
        {
            unresolvedCodeSystems.insert(codeSystemUrl);
        }
    }

    void verifyValueSetsFull()
    {
        for (const auto& valueSet : mRepo.mValueSetsByKey)
        {
            verifyValueSet(valueSet.first.url, valueSet.second.get());
        }
    }

    void verifyValueSetsRequiredOnly()
    {
        for (const auto& valueSetUrl : requiredValueSets)
        {
            FhirValueSet* valueSet = mRepo.findValueSet(valueSetUrl, {});
            verifyValueSet(valueSetUrl, valueSet);
        }
    }

    void verifyValueSet(const std::string& valueSetUrl, FhirValueSet* valueSet)
    {
        verifyValueSetIncludes(valueSetUrl, valueSet);
        verifyValueSetExcludes(valueSetUrl, valueSet);
    }
    void verifyValueSetExcludes(const std::string& valueSetUrl, FhirValueSet* valueSet)
    {
        for (const auto& exclude : valueSet->getExcludes())
        {
            if (exclude.codeSystemUrl.has_value())
            {
                const auto* codeSystem = mRepo.findCodeSystem(*exclude.codeSystemUrl, {});
                verifyValueSetCodeSystem(valueSetUrl, valueSet, codeSystem, exclude.codes, *exclude.codeSystemUrl);
            }
            FPExpect(exclude.valueSets.empty(), "Not implemented: ValueSet.compose.exclude.valueSet");
        }
    }
    void verifyValueSetIncludes(const std::string& valueSetUrl, FhirValueSet* valueSet)
    {
        for (const auto& include : valueSet->getIncludes())
        {
            if (include.codeSystemUrl.has_value())
            {
                const auto* codeSystem = mRepo.findCodeSystem(*include.codeSystemUrl, {});
                verifyValueSetCodeSystem(valueSetUrl, valueSet, codeSystem, include.codes, *include.codeSystemUrl);
            }
            for (const auto& referencedValueSet : include.valueSets)
            {
                if (! mRepo.findValueSet(referencedValueSet, {}))
                {
                    unresolvedValueSets.insert(referencedValueSet);
                }
            }
        }
    }
    void verifyValueSetCodeSystem(const std::string& valueSetUrl, FhirValueSet* valueSet,
                                  const fhirtools::FhirCodeSystem* codeSystem, const std::set<std::string>& codes,
                                  const std::basic_string<char>& codeSystemUrl)
    {
        if (codeSystem != nullptr)
        {
            for (const auto& code : codes)
            {
                if (! codeSystem->containsCode(code))
                {
                    if (codeSystem->getContentType() == FhirCodeSystem::ContentType::complete)
                    {
                        LOG(ERROR) << "code " << code << " in ValueSet " << valueSetUrl
                                   << " not contained in referenced CodeSystem " << codeSystemUrl;
                    }
                }
            }
        }
        else
        {
            if (codes.empty())
            {
                unresolvedCodeSystems.insert(codeSystemUrl);
                valueSet->addError("Unresolved CodeSystem " + codeSystemUrl);
            }
        }
    }

    FhirStructureRepository& mRepo;
    bool mVerfied = true;
    FhirConstraint::ExpressionCache mExpressionCache;
    std::set<std::string> unresolvedBase;
    std::set<std::string> missingType;
    std::set<std::string> elementsWithUnresolvedType;
    std::set<std::string> unresolvedProfiles;
    std::set<std::string> unresolvedBindings;
    std::set<std::string> requiredValueSets;
    std::set<std::string> unresolvedCodeSystems;
    std::set<std::string> unresolvedValueSets;
};

FhirStructureRepository::DefinitionKey::DefinitionKey(std::string url, std::string version)
    : url(std::move(url))
    , version(std::move(version))
{
}


FhirStructureRepository::DefinitionKey::DefinitionKey(std::string_view urlWithVersion)
{
    size_t delim = urlWithVersion.rfind('|');
    url = urlWithVersion.substr(0, delim);
    if (delim != std::string_view::npos)
    {
        version.assign(urlWithVersion, delim + 1);
    }
}


FhirStructureRepository::FhirStructureRepository()
{
    addSystemTypes();
}

FhirStructureRepository::~FhirStructureRepository() = default;

void FhirStructureRepository::load(const std::list<std::filesystem::path>& filesAndDirectories)
{
    TVLOG(1) << "Loading FHIR structure definitions.";
    std::list<FhirCodeSystem> supplements;
    for (const auto& file : filesAndDirectories)
    {
        if (is_regular_file(file))
        {
            loadFile(supplements, file);
        }
        else if (is_directory(file))
        {
            for (const auto& dirEntry : std::filesystem::directory_iterator{file})
            {
                FPExpect(dirEntry.is_regular_file(), "only regular files supported in profile directories.");
                loadFile(supplements, dirEntry.path());
            }
        }
        else
        {
            FPFail("unsupported path: " + file.string());
        }
    }
    processCodeSystemSupplements(supplements);
    TVLOG(2) << "done loading";
    Verifier verifier(*this);
    verifier.verify();
    FhirStructureRepositoryFixer repoFixer{*this};
    repoFixer.fix();
    const auto& requiredValueSets = verifier.getRequiredValueSets();
    for (const auto& valueSetKeyValue : mValueSetsByKey)
    {
        const auto& valueSet = valueSetKeyValue.second;
        if (! valueSet->finalized())
        {
            valueSet->finalize(this);
            if (! valueSet->getWarnings().empty())
            {
                const auto valueSetFullUrl = valueSet->getUrl() + "|" + valueSet->getVersion();
                if (requiredValueSets.contains(valueSetFullUrl) || requiredValueSets.contains(valueSet->getUrl()))
                {
                    TLOG(ERROR) << "Required Valueset " << valueSetFullUrl
                                << " finalization with errors: " << valueSet->getWarnings();
                }
            }
        }
    }
}
void FhirStructureRepository::loadFile(std::list<FhirCodeSystem>& supplements, const std::filesystem::path& file)
{
    try
    {
        TVLOG(2) << "loading: " << file;
        auto [definitions, codeSystems, valueSets] = FhirStructureDefinitionParser::parse(file);
        for (auto&& def : definitions)
        {
            addDefinition(std::make_unique<FhirStructureDefinition>(std::move(def)));
        }
        for (auto&& codeSystem : codeSystems)
        {
            if (codeSystem.getContentType() == FhirCodeSystem::ContentType::supplement)
            {
                supplements.emplace_back(std::move(codeSystem));
            }
            else
            {
                addCodeSystem(std::make_unique<FhirCodeSystem>(std::move(codeSystem)));
            }
        }
        for (auto&& valueSet : valueSets)
        {
            addValueSet(std::make_unique<FhirValueSet>(std::move(valueSet)));
        }
    }
    catch (const std::exception& ex)
    {
        Fail2(file.string() + ": " + ex.what(), std::logic_error);
    }
}

const FhirStructureDefinition* FhirStructureRepository::findDefinitionByUrl(std::string_view url) const
{
    DefinitionKey key{url};
    if (key.version.empty())
    {
        const auto& item = mLatestDefinitionsByUrl.find(key.url);
        return (item != mLatestDefinitionsByUrl.end()) ? item->second : (nullptr);
    }
    const auto& item = mDefinitionsByKey.find(key);
    return (item != mDefinitionsByKey.end()) ? item->second.get() : (nullptr);
}

const FhirStructureDefinition* FhirStructureRepository::findTypeById(const std::string& typeName) const
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

fhirtools::FhirValueSet* fhirtools::FhirStructureRepository::findValueSet(const std::string_view url,
                                                                          std::optional<std::string> version)
{
    return findValueSetHelper(this, url, std::move(version));
}
const fhirtools::FhirValueSet*
fhirtools::FhirStructureRepository::findValueSet(const std::string_view url, std::optional<std::string> version) const
{
    return findValueSetHelper(this, url, std::move(version));
}

const fhirtools::FhirCodeSystem*
fhirtools::FhirStructureRepository::findCodeSystem(const std::string_view url, std::optional<std::string> version) const
{
    auto key = version.has_value() ? DefinitionKey{std::string{url}, *version} : DefinitionKey{url};
    if (key.version.empty())
    {
        auto candidate = mLatestCodeSystemsByUrl.find(key.url);
        if (candidate != mLatestCodeSystemsByUrl.end())
        {
            return candidate->second;
        }
    }
    else
    {
        auto candidate = mCodeSystemsByKey.find(key);
        if (candidate != mCodeSystemsByKey.end())
        {
            return candidate->second.get();
        }
    }
    return nullptr;
}

FhirStructureRepository::ContentReferenceResolution
FhirStructureRepository::resolveContentReference(const FhirElement& element) const
{
    using namespace std::string_literals;
    const auto& contentReference = element.contentReference();
    Expect3(! contentReference.empty(), "Cannot resolve empty contentReference", std::logic_error);
    const auto hash = contentReference.find('#');
    auto elementName = contentReference.substr(hash + 1);
    const FhirStructureDefinition* baseType = nullptr;
    if (hash != 0)
    {
        baseType = findDefinitionByUrl(contentReference.substr(0, hash));
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
        find_if(baseElements.begin(), baseElements.end(), [&](const std::shared_ptr<const FhirElement>& element) {
            return element->name() == elementName;
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
FhirStructureRepository::resolveBaseContentReference(std::string_view elementId) const
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
    auto element = profile->rootElement();
    size_t index = 0;
    if (dot != std::string::npos)
    {
        size_t nextDot = dot;
        do
        {
            if (element->isBackbone())
            {
                nextDot = elementId.find('.', nextDot + 1);
                std::tie(element, index) =
                    profile->findElementAndIndex(std::string{elementId.substr(dot, nextDot - dot)});
            }
            else
            {
                dot = nextDot;
                profile = findTypeById(element->typeId());
                Expect3(profile,
                        "failed to resolve DefinitionPointer for "s.append(elementId) +
                            " no such type: " + element->typeId(),
                        std::logic_error);
                element = profile->rootElement();
                index = 0;
            }
        } while (element && nextDot != std::string_view::npos);
    }
    Expect(element != nullptr, "element not found: "s.append(elementId));
    if (element->typeId().empty())
    {
        return resolveBaseContentReference(element->contentReference());
    }
    return {ProfiledElementTypeInfo{profile, element}, index};
}

void FhirStructureRepository::addSystemTypes()
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

const FhirStructureDefinition* FhirStructureRepository::addSystemType(FhirStructureDefinition::Kind kind,
                                                                      const std::string_view& url)
{
    std::string urlString{url};
    using namespace std::string_literals;
    FhirStructureDefinition::Builder builder;
    builder.kind(kind)
        .typeId(urlString)
        .url(urlString)
        .addElement(FhirElement::Builder{}.name(urlString).isArray(true).getAndReset(), {})
        .addElement(FhirElement::Builder{}.name(urlString + ".value"s).typeId(urlString).getAndReset(), {});
    return addDefinition(std::make_unique<FhirStructureDefinition>(builder.getAndReset()));
}

const FhirStructureDefinition*
FhirStructureRepository::addDefinition(std::unique_ptr<FhirStructureDefinition>&& definition)
{
    if (definition->derivation() != FhirStructureDefinition::Derivation::constraint)
    {
        auto [iter, inserted] = mDefinitionsByTypeId.try_emplace(definition->typeId(), definition.get());
        Expect3(inserted, "Duplicate type: " + definition->typeId(), std::logic_error);
    }
    {// scope
        auto [iter, inserted] = mLatestDefinitionsByUrl.try_emplace(definition->url(), definition.get());
        if (! inserted && iter->second->version() < definition->version())
        {
            iter->second = definition.get();
        }
    }
    {// scope
        auto [iter, inserted] = mDefinitionsByKey.try_emplace(DefinitionKey{definition->url(), definition->version()},
                                                              std::move(definition));
        if (! inserted)
        {
            Fail("Duplicate profile: " + iter->first.url + '|' + iter->first.version);
        }
        return iter->second.get();
    }
}

void fhirtools::FhirStructureRepository::addCodeSystem(std::unique_ptr<FhirCodeSystem>&& codeSystem)
{
    {
        auto [it, inserted] = mLatestCodeSystemsByUrl.try_emplace(codeSystem->getUrl(), codeSystem.get());
        if (! inserted && it->second->getVersion() < codeSystem->getVersion())
        {
            it->second = codeSystem.get();
        }
    }
    {
        auto [it, inserted] = mCodeSystemsByKey.try_emplace(
            DefinitionKey(codeSystem->getUrl(), codeSystem->getVersion()), std::move(codeSystem));
        FPExpect(inserted, "Duplicate CodeSystem: " + codeSystem->getUrl() + '|' + codeSystem->getVersion());
    }
}

void fhirtools::FhirStructureRepository::addValueSet(std::unique_ptr<FhirValueSet>&& valueSet)
{
    {
        auto [it, inserted] = mLatestValueSetsByUrl.try_emplace(valueSet->getUrl(), valueSet.get());
        if (! inserted && it->second->getVersion() < valueSet->getVersion())
        {
            it->second = valueSet.get();
        }
    }
    {
        auto [it, inserted] =
            mValueSetsByKey.try_emplace(DefinitionKey(valueSet->getUrl(), valueSet->getVersion()), std::move(valueSet));
        FPExpect(inserted, "Duplicate ValueSet: " + valueSet->getUrl() + '|' + valueSet->getVersion());
        TVLOG(3) << "ValueSet inserted: " << it->first.url << '|' << it->first.version;
    }
}

void fhirtools::FhirStructureRepository::processCodeSystemSupplements(const std::list<FhirCodeSystem>& supplements)
{
    for (auto&& supplement : supplements)
    {
        FPExpect(! supplement.getSupplements().empty(),
                 "CodeSystem.content=supplement, but CodeSystem.supplements is not defined.");
        DefinitionKey key{supplement.getSupplements()};
        FhirCodeSystem* cs{};
        if (key.version.empty())
        {
            auto codeSystem = mLatestCodeSystemsByUrl.find(key.url);
            if (codeSystem != mLatestCodeSystemsByUrl.end())
            {
                cs = codeSystem->second;
            }
        }
        else
        {
            auto codeSystem = mCodeSystemsByKey.find(DefinitionKey{supplement.getSupplements()});
            if (codeSystem != mCodeSystemsByKey.end())
            {
                cs = codeSystem->second.get();
            }
        }
        auto builder = cs ? FhirCodeSystem::Builder(*cs) : FhirCodeSystem::Builder();
        if (cs == nullptr)
        {
            VLOG(1) << "CodeSystem.supplements not found, synthesizing: " << supplement.getSupplements();
            builder.synthesized();
        }
        builder.url(supplement.getSupplements());
        for (const auto& code : supplement.getCodes())
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
