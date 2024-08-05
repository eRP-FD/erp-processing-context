#include "FhirResourceViewVerifier.hxx"

#include <ranges>

namespace fhirtools
{

fhirtools::FhirResourceViewVerifier::FhirResourceViewVerifier(const FhirStructureRepositoryBackend& repo,
                                                              const FhirStructureRepository* repoView)
    : mRepo{repo}
    , mView{repoView}
{
}

void FhirResourceViewVerifier::verify()
{
    if (mView)
    {
        TLOG(INFO) << "Validating FHIR-Repository View: " << mView->id();
    }
    else
    {
        TLOG(INFO) << "Validating FHIR-Repository Backend";
    }
    for (const auto& def : mRepo.definitionsByKey())
    {
        if (! mView || mView->findStructure(def.first))
        {
            verifyStructure(*def.second);
        }
    }
    if (mView)
    {
        verifyValueSetsRequiredOnly();
    }
    if (! unresolvedBase.empty())
    {
        TLOG(ERROR) << R"(Could not resolve FHIR baseDefinitions: [")" + boost::join(unresolvedBase, R"(", ")") +
                           R"("])";
        mVerfied = false;
    }
    if (! missingType.empty())
    {
        TLOG(ERROR) << R"(Could not find FHIR types: [")" + boost::join(missingType, R"(", ")") + R"("])";
        mVerfied = false;
    }
    if (! elementsWithUnresolvedType.empty())
    {
        TLOG(ERROR) << R"(Could not resolve FHIR types for: [")" + boost::join(elementsWithUnresolvedType, R"(", ")") +
                           R"("])";
        mVerfied = false;
    }
    if (! unresolvedProfiles.empty())
    {
        TLOG(ERROR) << R"(Could not resolve FHIR profiles: [")" + boost::join(unresolvedProfiles, R"(", ")") + R"("])";
        mVerfied = false;
    }
    if (! unresolvedBindings.empty())
    {
        TLOG(WARNING) << R"(Could not resolve ValueSet Bindings: [")" + boost::join(unresolvedBindings, R"(", ")") +
                             R"("])";
    }
    if (! unresolvedCodeSystems.empty())
    {
        TLOG(WARNING) << R"(Could not resolve CodeSystems: [")" + boost::join(unresolvedCodeSystems, R"(", ")") +
                             R"("])";
    }
    if (! unresolvedValueSets.empty())
    {
        std::list<std::string> as_string;
        std::ranges::transform(unresolvedValueSets, std::back_inserter(as_string),
                               static_cast<std::string (*)(const DefinitionKey&)>(&fhirtools::to_string));
        TLOG(WARNING) << R"(Could not resolve ValueSets: [")" + boost::join(as_string, R"(", ")") + R"("])";
    }
    if (mView)
    {
        FPExpect3(mVerfied, "FHIR-Structure-Repository-View verification failed: " + std::string{mView->id()},
                  std::logic_error);
    }
    else
    {
        FPExpect3(mVerfied, "FHIR-Structure-Repository verification failed", std::logic_error);
    }
}

void FhirResourceViewVerifier::verifyStructure(const FhirStructureDefinition& def)
{
    FPExpect3(def.resourceGroup(), "missing resourceGroup in: " + def.urlAndVersion(), std::logic_error);
    const auto& baseDefinition = def.baseDefinition();
    if (! baseDefinition.empty())
    {
        if (def.derivation() == FhirStructureDefinition::Derivation::basetype)
        {
            TLOG(ERROR) << "Structure definition has derivation 'basetype', but baseDefinition is defined: "
                        << def.url() << '|' << def.version();
            mVerfied = false;
        }
        auto baseKey = def.resourceGroup()->find(DefinitionKey{baseDefinition}).first;
        if (baseKey.version.has_value())
        {
            const auto* baseType = mRepo.findDefinition(baseKey.url, *baseKey.version);
            if (! baseType)
            {
                unresolvedBase.insert(baseDefinition);
                TLOG(ERROR) << "Failed to resolve base type for " << def.urlAndVersion() << ": " << baseDefinition;
            }
        }
        else
        {
            TLOG(ERROR) << "basetype of " << def.urlAndVersion() << " not found in group (" << def.resourceGroup()->id()
                        << "): " << baseDefinition;
            mVerfied = false;
        }
    }
    else
    {
        if (def.derivation() != FhirStructureDefinition::Derivation::basetype)
        {
            TLOG(ERROR) << "Profile has derivation '" << def.derivation()
                        << "', but baseDefinition is not defined: " << def.url() << '|' << def.version();
            mVerfied = false;
        }
    }
    for (const auto& element : def.elements())
    {
        FPExpect3(element != nullptr, "ElementDefinition must not be null.", std::logic_error);
        verifyElement(def, *element);
    }
}

//NOLINTNEXTLINE(misc-no-recursion)
void FhirResourceViewVerifier::verifyElement(const FhirStructureDefinition& def, const FhirElement& element)
{
    using boost::starts_with;
    if (element.name() != def.typeId())
    {
        if (! element.contentReference().empty())
        {
            verifyContentReference(def, element);
        }
        const auto& elementType = element.typeId();
        if (not elementType.empty() && not starts_with(elementType, "http://hl7.org/fhirpath/System."))
        {
            const auto* typeDefinition = mRepo.findTypeById(elementType);
            if (! typeDefinition)
            {
                missingType.insert(elementType);
                TLOG(ERROR) << "Failed to resolve type for " << def.url() << '|' << def.version() << "@"
                            << element.originalName() << ": " << elementType;
            }
        }
    }
    FPExpect3(def.resourceGroup(), "group not set in element: " + def.urlAndVersion() + '.' + element.originalName(),
              std::logic_error);
    verifyElementProfiles(def, element);
    verifyBinding(element, def.resourceGroup());
    verifySlicing(def, element);
    parseConstraints(def, element);
    if (mView)
    {
        verifyFixedCodeSystems(element, def.url());
    }
}

void FhirResourceViewVerifier::verifyElementProfiles(const FhirStructureDefinition& def, const FhirElement& element)
{
    for (const auto& profile : element.profiles())
    {
        verifyProfileExists(*def.resourceGroup(), profile,
                            "profile type for " + def.urlAndVersion() + "@" + element.originalName(), true);
    }
    for (const auto& profile : element.referenceTargetProfiles())
    {
        verifyProfileExists(*def.resourceGroup(), profile,
                            "targetProfile type for " + def.urlAndVersion() + "@" + element.originalName(), false);
    }
}

void FhirResourceViewVerifier::verifyProfileExists(const FhirResourceGroup& group, const std::string& profile,
                                                   const std::string& context, bool mustResolve)
{
    if (mView)
    {
        verifyProfileExistsInView(profile, context);
    }
    else if (mustResolve)
    {
        verifyProfileExistsInGroup(group, profile, context);
    }
}

void FhirResourceViewVerifier::verifyProfileExistsInGroup(const FhirResourceGroup& group, const std::string& profile,
                                                          const std::string& context)
{
    auto profileKey = group.find(DefinitionKey{profile}).first;
    if (profileKey.version.has_value())
    {
        if (! mRepo.findDefinition(profileKey.url, *profileKey.version))
        {
            unresolvedProfiles.emplace(profile);
            TLOG(ERROR) << "Failed to resolve " << context << ": " << profile;
            mVerfied = false;
        }
    }
    else
    {
        unresolvedProfiles.emplace(profile);
        TLOG(ERROR) << context << " not found in group (" << group.id() << "): " << profile;
        mVerfied = false;
    }
}

void fhirtools::FhirResourceViewVerifier::verifyProfileExistsInView(const std::string& profile,
                                                                    const std::string& context)
{
    if (! mView->findStructure(DefinitionKey{profile}))
    {
        unresolvedProfiles.emplace(profile);
        TLOG(ERROR) << "Failed to resolve " << context << " in view " << mView->id() << ": " << profile;
        mVerfied = false;
    }
}

void FhirResourceViewVerifier::verifyBinding(const FhirElement& element,
                                             const std::shared_ptr<const fhirtools::FhirResourceGroup>& group)
{
    using namespace std::string_literals;
    if (element.hasBinding())
    {
        auto bindingKey = group->find(element.binding().key).first;
        if (tryResolve(group, bindingKey, element.binding().strength))
        {
            requiredValueSets.emplace(element.binding().key);
        }
    }
}

const FhirValueSet*
FhirResourceViewVerifier::tryResolve(const std::shared_ptr<const fhirtools::FhirResourceGroup>& group,
                                     const DefinitionKey& key, FhirElement::BindingStrength bindingStrength)
{
    if (mView)
    {
        const FhirValueSet* valueSet = nullptr;
        valueSet = mView->findValueSet(key);
        if (! valueSet)
        {
            std::stringstream msg;
            msg << "Failed to resolve " << magic_enum::enum_name(bindingStrength) << " binding '" << to_string(key)
                << "'";
            switch (bindingStrength)
            {
                case FhirElement::BindingStrength::required:
                    TLOG(WARNING) << msg.str();
                    unresolvedBindings.insert(to_string(key));
                    mVerfied = false;
                    break;
                case FhirElement::BindingStrength::example:
                    TVLOG(2) << msg.str();
                    break;
                case FhirElement::BindingStrength::extensible:
                case FhirElement::BindingStrength::preferred:
                    TLOG(INFO) << msg.str();
                    break;
            }
        }
        return valueSet;
    }
    auto fullKey = group->find(key).first;
    return fullKey.version.has_value() ? mRepo.findValueSet(fullKey.url, *fullKey.version) : nullptr;
}

void FhirResourceViewVerifier::verifyContentReference(const FhirStructureDefinition& def, const FhirElement& element)
{
    if (element.contentReference().empty())
    {
        elementsWithUnresolvedType.emplace(def.url() + "@" + element.name());
        TLOG(ERROR) << "Failed to resolve element type for " << def.url() << '|' << def.version() << "@"
                    << element.originalName();
    }
    else
    {
        try
        {
            (void) mRepo.resolveContentReference(*def.resourceGroup(), element);
        }
        catch (const std::logic_error&)
        {
            TLOG(ERROR) << "Failed to resolve element type for " << def.url() << '|' << def.version() << "@"
                        << element.originalName() << ": " + element.contentReference();
            elementsWithUnresolvedType.emplace(def.url() + "@" + element.name());
        }
    }
}

//NOLINTNEXTLINE(misc-no-recursion)
void FhirResourceViewVerifier::verifySlicing(const FhirStructureDefinition& def, const FhirElement& element)
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
                (void) disc.condition(*mRepo.defaultView(), &profile);
                TVLOG(4) << "Verified " << def.url() << '|' << def.version() << ":" << element.originalName() << ":"
                         << slice.name() << " discriminator: " << magic_enum::enum_name(disc.type()) << '@'
                         << disc.path();
            }
            catch (const std::exception& ex)
            {
                mVerfied = false;
                TLOG(ERROR) << "Slice verification failed " << def.url() << '|' << def.version() << ":"
                            << element.originalName() << ":" << slice.name() << ": " << ex.what();
            }
        }
        for (const auto& sliceElement : profile.elements())
        {
            verifyElement(profile, *sliceElement);
        }
    }
}

void FhirResourceViewVerifier::parseConstraints(const FhirStructureDefinition& def, const FhirElement& element)
{
    for (const auto& constraint : element.getConstraints())
    {
        try
        {
            (void) constraint.parsedExpression(*mRepo.defaultView(), &mExpressionCache);
        }
        catch (const std::exception& ex)
        {
            TLOG(ERROR) << ex.what() << ":" << def.url() << '|' << def.version() << "@" << element.originalName()
                        << ": " << constraint.getExpression();
            mVerfied = false;
        }
    }
}

void FhirResourceViewVerifier::verifyFixedCodeSystems(const FhirElement& element, const std::string& context)
{
    if (element.pattern() != nullptr)
    {
        ValueElement valueElement(mRepo.defaultView(), element.pattern());
        verifyFixedCodeSystems(element, valueElement, context);
    }
    if (element.fixed() != nullptr)
    {
        ValueElement valueElement(mRepo.defaultView(), element.fixed());
        verifyFixedCodeSystems(element, valueElement, context);
    }
}

void FhirResourceViewVerifier::verifyFixedCodeSystems(const FhirElement& element,
                                                      const fhirtools::ValueElement& fixedOrPattern,
                                                      const std::string& context)
{
    if (element.typeId() == "CodeableConcept")
    {
        auto fixedCodings = fixedOrPattern.subElements("coding");
        for (const auto& fixedCoding : fixedCodings)
        {
            auto codingSystems = fixedCoding->subElements("system");
            if (codingSystems.size() == 1)
            {
                verifyFixedCodeSystems(codingSystems[0]->asString(),
                                       context + '@' + element.originalName() + ".coding.system");
            }
        }
    }
    else if (element.typeId() == "Coding")
    {
        auto codingSystems = fixedOrPattern.subElements("system");
        if (codingSystems.size() == 1)
        {
            verifyFixedCodeSystems(codingSystems[0]->asString(), context + '@' + element.originalName() + ".system");
        }
    }
    else if (element.name().ends_with("coding.system"))
    {
        verifyFixedCodeSystems(fixedOrPattern.asString(), context + '@' + element.originalName() + ".coding.system");
    }
}

void FhirResourceViewVerifier::verifyFixedCodeSystems(const std::string& codeSystemUrl, const std::string& context)
{
    FPExpect3(mView, "must only be called when validating view", std::logic_error);
    const auto* codeSystem = mView->findCodeSystem(DefinitionKey{codeSystemUrl});
    if (! codeSystem)
    {
        TLOG(INFO) << context << " references unknown CodeSystem: " << codeSystemUrl;
        unresolvedCodeSystems.insert(codeSystemUrl);
    }
    else if (codeSystem->isSynthesized())
    {
        TVLOG(2) << context << " references synthesized CodeSystem: " << codeSystemUrl;
    }
}

void FhirResourceViewVerifier::verifyValueSetsRequiredOnly()
{
    if (! mView)
    {
        return;
    }
    for (const auto& valueSetKey : requiredValueSets)
    {
        const auto* valueSet = mView->findValueSet(valueSetKey);
        if (valueSet)
        {
            if (valueSet->hasErrors())
            {
                TLOG(WARNING) << "Required Valueset " << valueSetKey << " has errors: " << valueSet->getWarnings();
                mVerfied = false;
            }
            verifyValueSet(valueSetKey, *valueSet);
        }
        else
        {
            unresolvedValueSets.insert(valueSetKey);
        }
    }
}

void FhirResourceViewVerifier::verifyValueSet(const DefinitionKey& valueSetKey, const FhirValueSet& valueSet)
{
    verifyValueSetIncludes(valueSetKey, valueSet);
    verifyValueSetExcludes(valueSetKey, valueSet);
}

void FhirResourceViewVerifier::verifyValueSetExcludes(const DefinitionKey& valueSetKey, const FhirValueSet& valueSet)
{
    using namespace std::string_literals;
    const auto& group = valueSet.resourceGroup();
    FPExpect3(group != nullptr, "missing resource group in Value Set: " + to_string(valueSetKey), std::logic_error);
    for (const auto& exclude : valueSet.getExcludes())
    {
        if (exclude.codeSystemKey.has_value())
        {
            auto codeSystemKey = group->find(*exclude.codeSystemKey).first;
            FPExpect3(codeSystemKey.version.has_value(),
                      "CodeSystem not found in group ("s.append(group->id()) + "): " + exclude.codeSystemKey->url,
                      std::logic_error);
            const auto* codeSystem = mRepo.findCodeSystem(codeSystemKey.url, *codeSystemKey.version);
            verifyValueSetCodeSystem(valueSetKey, codeSystem, exclude.codes, *exclude.codeSystemKey);
        }
        FPExpect3(exclude.valueSets.empty(), "Not implemented: ValueSet.compose.exclude.valueSet", std::logic_error);
    }
}

void FhirResourceViewVerifier::verifyValueSetIncludes(const DefinitionKey& valueSetKey, const FhirValueSet& valueSet)
{
    const auto& group = valueSet.resourceGroup();
    FPExpect3(group != nullptr, "missing resource group in Value Set: " + to_string(valueSetKey), std::logic_error);
    using namespace std::string_literals;
    for (const auto& include : valueSet.getIncludes())
    {
        if (include.codeSystemKey.has_value())
        {
            auto codeSystemKey = group->find(*include.codeSystemKey).first;
            const auto* codeSystem =
                codeSystemKey.version ? mRepo.findCodeSystem(codeSystemKey.url, *codeSystemKey.version) : nullptr;
            verifyValueSetCodeSystem(valueSetKey, codeSystem, include.codes, *include.codeSystemKey);
        }
        for (auto referencedValueSet : include.valueSets)
        {
            referencedValueSet = group->find(referencedValueSet).first;
            if (! referencedValueSet.version ||
                ! mRepo.findValueSet(referencedValueSet.url, *referencedValueSet.version))
            {
                unresolvedValueSets.insert(referencedValueSet);
            }
        }
    }
}

void FhirResourceViewVerifier::verifyValueSetCodeSystem(const DefinitionKey& valueSetKey,
                                                        const FhirCodeSystem* codeSystem,
                                                        const std::set<std::string>& codes,
                                                        const DefinitionKey& codeSystemKey)
{
    if (codeSystem != nullptr)
    {
        for (const auto& code : codes)
        {
            if (! codeSystem->containsCode(code))
            {
                if (codeSystem->getContentType() == FhirCodeSystem::ContentType::complete)
                {
                    TLOG(ERROR) << "code " << code << " in ValueSet " << valueSetKey
                                << " not contained in referenced CodeSystem " << codeSystemKey;
                }
            }
        }
    }
    else
    {
        if (codes.empty())
        {
            TLOG(INFO) << "no codes in ValueSet " << valueSetKey << " referencing unknown CodeSystem " << codeSystemKey;
            unresolvedCodeSystems.insert(to_string(codeSystemKey));
        }
    }
}
}
