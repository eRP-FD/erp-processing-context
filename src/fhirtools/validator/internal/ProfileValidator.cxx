// (C) Copyright IBM Deutschland GmbH 2022
// (C) Copyright IBM Corp. 2022

#include "fhirtools/validator/internal/ProfileValidator.hxx"
#include "erp/util/ExceptionWrapper.hxx"
#include "erp/util/TLog.hxx"

#include <utility>

#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/Functions.hxx"
#include "fhirtools/model/ValueElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"
#include "fhirtools/repository/FhirElement.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/validator/internal/ProfileSetValidator.hxx"
#include "fhirtools/validator/internal/ValidationData.hxx"

using namespace fhirtools;
fhirtools::ProfileValidatorCounterKey::ProfileValidatorCounterKey(std::string initName, std::string initSlice)
    : name(std::move(initName))
    , slice(std::move(initSlice))
{
}
void fhirtools::ProfileValidatorCounterData::check(ProfileValidator::Map& profMap,
                                                   const fhirtools::ProfileValidatorCounterKey& cKey,
                                                   std::string_view elementFullPath) const
{
    using namespace std::string_view_literals;
    for (const auto& element : elementMap)
    {
        std::ostringstream subElementPath;
        subElementPath << elementFullPath << '.' << cKey.name;
        if (element.second.element()->isArray())
        {
            subElementPath << "[*]"sv;
        }
        if (! cKey.slice.empty())
        {
            subElementPath << ':' << cKey.slice;
        }
        profMap.at(element.first)
            .appendResults(
                element.second.element()->cardinality().check(count, subElementPath.str(), element.second.profile()));
    }
}

void ProfileValidator::Map::merge(ProfileValidator::Map other)
{
    for (auto&& profVal : other)
    {
        auto [iter, inserted] = try_emplace(profVal.first, std::move(profVal.second));
        if (! inserted)
        {
            iter->second.merge(std::move(profVal.second));
        }
    }
}


ProfileValidator::ProfileValidator(ProfileValidator&&) noexcept = default;
ProfileValidator& ProfileValidator::operator=(ProfileValidator&&) noexcept = default;


ProfileValidator::ProfileValidator(MapKey mapKey, std::set<std::shared_ptr<ValidationData>> parentData,
                                   fhirtools::ProfiledElementTypeInfo defPtr, std::string sliceName)
    : mData{std::make_shared<ValidationData>(std::make_unique<MapKey>(std::move(mapKey)))}
    , mParentData{std::move(parentData)}
    , mDefPtr{std::move(defPtr)}
    , mSliceName{std::move(sliceName)}
{
}

ProfileValidator::ProfileValidator(ProfiledElementTypeInfo defPtr)
    : ProfileValidator{{defPtr}, {}, std::move(defPtr), {}}
{
}

ProfileValidator::Map ProfileValidator::subFieldValidators(const fhirtools::FhirStructureRepository& repo,
                                                           std::string_view name)
{
    using namespace std::string_literals;
    Expect3(mData != nullptr, "mData must not be null", std::logic_error);
    auto subField = mDefPtr.subField(name);
    ProfileValidator::Map result;
    if (! subField)
    {
        if (! mDefPtr.element()->isBackbone())
        {
            TVLOG(3) << "abandoning profile " << mDefPtr.profile()->url() << mDefPtr.profile()->version() << "@"
                     << mDefPtr.element()->name() << "." << name;
            return {};
        }
        std::ostringstream baseElementName;
        baseElementName << mDefPtr.element()->name() << '.' << name;
        ProfiledElementTypeInfo basePtr(&repo, baseElementName.str());
        if (! mSliceName.empty())
        {
            baseElementName << ':' << mSliceName;
        }
        auto element = FhirElement::Builder{*basePtr.element()}
                           .originalName(baseElementName.str())
                           .cardinalityMin(0)
                           .cardinalityMax(0)
                           .getAndReset();
        ProfiledElementTypeInfo zeroCardinalityPtr{basePtr.profile(), element};
        MapKey key{mDefPtr};
        ProfileValidator validator{key, {mData}, zeroCardinalityPtr, mSliceName};
        result.emplace(std::move(key), std::move(validator));
        return result;
    }
    std::map<MapKey, std::shared_ptr<ValidationData>> profilesKeys;
    for (const auto& url : subField->element()->profiles())
    {
        const auto* prof = repo.findDefinitionByUrl(url);
        Expect3(prof != nullptr, "failed to resolve profile: " + url, std::logic_error);
        ProfiledElementTypeInfo defPtr{prof};
        MapKey key{defPtr};
        ProfileValidator validator{key, {}, defPtr, {}};
        profilesKeys.emplace(key, validator.mData);
        TVLOG(4) << "adding sub-validator profile: " << url;
        result.emplace(std::move(key), std::move(validator));
    }
    for (const auto& defPtr : mDefPtr.subDefinitions(repo, name))
    {
        MapKey key{defPtr};
        ProfileValidator validator{key, {mData}, defPtr, {}};
        if (! profilesKeys.empty())
        {
            validator.mSolver.requireOne(profilesKeys);
        }
        result.emplace(std::move(key), std::move(validator));
    }

    return result;
}

void fhirtools::ProfileValidator::typecast(const fhirtools::FhirStructureRepository& repo,
                                           const fhirtools::FhirStructureDefinition* structDef)
{
    mDefPtr.typecast(repo, structDef);
}

//NOLINTNEXTLINE(misc-no-recursion)
ProfileValidator::ProcessingResult ProfileValidator::process(const Element& element, std::string_view elementFullPath)
{
    checkContraints(element, elementFullPath);
    checkBinding(element, elementFullPath);
    checkValue(element, elementFullPath);
    const auto& slicing = mDefPtr.element()->slicing();
    if (slicing)
    {
        ProcessingResult result;
        for (const auto& slice : slicing->slices())
        {
            auto condition = slice.condition(*element.getFhirStructureRepository(), slicing->discriminators());
            Expect(condition != nullptr, "couldn't get condition for slice: " + slice.name());
            if (condition->test(element))
            {
                const auto& profile = slice.profile();
                static_assert(std::is_reference_v<decltype(slice.profile())>);
                result.sliceProfiles.emplace_back(&profile);
                ProfiledElementTypeInfo ptr{&profile, profile.rootElement()};
                MapKey profKey{ptr};
                ProfileValidator profVal{profKey, mParentData, ptr, slice.name()};
                auto subSlices = profVal.process(element, elementFullPath);
                Expect3(subSlices.sliceProfiles.empty(), "Slice rootElement cannot be sliced.", std::logic_error);
                mData->add(Severity::debug, "detected slice: " + slice.name(), std::string{elementFullPath},
                           ptr.profile());
                if (!profile.rootElement()->profiles().empty())
                {
                    TVLOG(3) << "RootElement of " << profile.url() << "|" << profile.version() << " has profiles";
                    std::map<MapKey, std::shared_ptr<ValidationData>> profilesKeys;
                    for (const auto& url : ptr.element()->profiles())
                    {
                        const auto* prof = element.getFhirStructureRepository()->findDefinitionByUrl(url);
                        Expect3(prof != nullptr, "failed to resolve profile: " + url, std::logic_error);
                        ProfiledElementTypeInfo defPtr{prof};
                        MapKey key{defPtr};
                        ProfileValidator validator{key, {}, defPtr, {}};
                        profilesKeys.emplace(key, validator.mData);
                        TVLOG(4) << "adding sub-validator profile: " << url;
                        result.extraValidators.emplace(std::move(key), std::move(validator));
                    }
                    profVal.mSolver.requireOne(profilesKeys);
                }
                result.extraValidators.emplace(std::move(profKey), std::move(profVal));
            }
        }
        return result;
    }
    return {};
}

void fhirtools::ProfileValidator::checkContraints(const fhirtools::Element& element, std::string_view elementFullPath)
{
    for (const auto& constraint : mDefPtr.element()->getConstraints())
    {
        const auto& expression = constraint.parsedExpression(*element.getFhirStructureRepository());
        try
        {
            auto contextElementTag = std::make_shared<Element::IsContextElementTag>();
            element.setIsContextElement(contextElementTag);
            const auto evalResult = ExistenceAllTrue{element.getFhirStructureRepository()}.eval(
                expression->eval({element.shared_from_this()}));
            const auto evalResultBoolean = evalResult.boolean();
            if (evalResultBoolean && ! evalResultBoolean->asBool())
            {
                TVLOG(3) << elementFullPath << ": " << magic_enum::enum_name(constraint.getSeverity()) << ": "
                         << constraint.getKey() << "{" << constraint.getExpression() << "} ==> " << evalResult;
                mData->add(constraint, std::string{elementFullPath}, mDefPtr.profile());
            }
        }
        catch (const std::runtime_error& re)
        {

            mData->add(Severity::error,
                       constraint.getKey() + "{" + constraint.getExpression() + "} evaluation error: " + re.what(),
                       std::string{elementFullPath}, mDefPtr.profile());
        }
    }
}

void ProfileValidator::checkValue(const Element& element, std::string_view elementFullPath)
{
    if (const auto& fixed = mDefPtr.element()->fixed(); fixed)
    {
        ValueElement fixedVal{element.getFhirStructureRepository(), fixed};
        if (element != fixedVal)
        {
            std::ostringstream msg;
            msg << "value must match fixed value: ";
            fixedVal.json(msg) << " (but is ";
            element.json(msg) << ")";
            mData->add(Severity::error, msg.str(), std::string{elementFullPath}, mDefPtr.profile());
        }
    }
    if (const auto& pattern = mDefPtr.element()->pattern(); pattern)
    {
        ValueElement patternVal{element.getFhirStructureRepository(), pattern};
        if (! element.matches(patternVal))
        {
            std::ostringstream msg;
            msg << "value must match pattern value: ";
            patternVal.json(msg) << " (but is ";
            element.json(msg) << ")";
            mData->add(Severity::error, msg.str(), std::string{elementFullPath}, mDefPtr.profile());
        }
    }
}


void ProfileValidator::checkBinding(const Element& element, std::string_view elementFullPath)
{
    if (mDefPtr.element() && mDefPtr.element()->hasBinding())
    {
        const auto& binding = mDefPtr.element()->binding();
        if (binding.strength == FhirElement::BindingStrength::example)
        {
            return;
        }
        if (const auto* bindingValueSet =
                element.getFhirStructureRepository()->findValueSet(binding.valueSetUrl, binding.valueSetVersion))
        {
            const auto& warning = bindingValueSet->getWarnings();
            if (! warning.empty())
            {
                mData->add(Severity::warning, warning, std::string{elementFullPath}, mDefPtr.profile());
            }
            if (bindingValueSet->canValidate())
            {
                validateBinding(element, binding, bindingValueSet, elementFullPath);
            }
            else
            {
                mData->add(Severity::warning, "Cannot validate ValueSet binding", std::string{elementFullPath},
                           mDefPtr.profile());
            }
        }
        else
        {
            mData->add(Severity::warning, "Unresolved ValueSet binding: " + binding.valueSetUrl,
                       std::string{elementFullPath}, mDefPtr.profile());
        }
    }
}

void fhirtools::ProfileValidator::validateBinding(const fhirtools::Element& element,
                                                  const FhirElement::Binding& binding,
                                                  const FhirValueSet* bindingValueSet, std::string_view elementFullPath)
{
    auto severity = binding.strength == FhirElement::BindingStrength::required ? Severity::error : Severity::debug;
    switch (element.type())
    {
        case Element::Type::Integer:
        case Element::Type::Decimal:
        case Element::Type::String:
        case Element::Type::Boolean:
        case Element::Type::Date:
        case Element::Type::DateTime:
        case Element::Type::Time:
        case Element::Type::Quantity:
            if (! bindingValueSet->containsCode(element.asString()))
            {
                mData->add(severity,
                           "Value " + element.asString() + " not allowed for ValueSet binding, allowed are " +
                               bindingValueSet->codesToString(),
                           std::string{elementFullPath}, mDefPtr.profile());
            }
            break;
        case Element::Type::Structured: {
            if (mDefPtr.element()->typeId() == "CodeableConcept")
            {
                auto codingSubElements = element.subElements("coding");
                for (const auto& codingSubElement : codingSubElements)
                {
                    checkCodingBinding(*codingSubElement, bindingValueSet, elementFullPath, severity);
                }
                break;
            }
            else if (mDefPtr.element()->typeId() == "Coding")
            {
                checkCodingBinding(element, bindingValueSet, elementFullPath, severity);
            }
            else
            {
                FPFail("Unsupported Structured type for Binding: " + mDefPtr.element()->typeId());
            }
        }
    }
}


void fhirtools::ProfileValidator::checkCodingBinding(const Element& codingElement, const FhirValueSet* fhirValueSet,
                                                     std::string_view elementFullPath, Severity errorSeverity)
{
    auto systemSubElement = codingElement.subElements("system");
    auto codeSubElement = codingElement.subElements("code");
    if (systemSubElement.size() == 1 && codeSubElement.size() == 1)
    {
        if (! fhirValueSet->containsCode(codeSubElement[0]->asString(), systemSubElement[0]->asString()))
        {
            mData->add(errorSeverity,
                       "Code " + codeSubElement[0]->asString() + " with system " + systemSubElement[0]->asString() +
                           " not allowed for ValueSet binding, allowed are " + fhirValueSet->codesToString(),
                       std::string{elementFullPath}, mDefPtr.profile());
        }
    }
    else
    {
        mData->add(errorSeverity, "Expected exactly one system and one code sub-element", std::string{elementFullPath},
                   mDefPtr.profile());
    }
}

void fhirtools::ProfileValidator::appendResults(fhirtools::ValidationResultList results)
{
    mData->append(std::move(results));
}

void fhirtools::ProfileValidator::finalize()
{
    mData->append(mSolver.collectResults());
    for (const auto& pd : mParentData)
    {
        pd->merge(*mData);
    }
}

ValidationResultList ProfileValidator::results() const
{
    ValidationResultList result{mData->results()};
    return result;
}


void fhirtools::ProfileValidator::merge(fhirtools::ProfileValidator&& other)
{
    Expect3(mDefPtr.profile() == other.mDefPtr.profile(), "cannot merge ProfileValidators for different profiles",
            std::logic_error);
    Expect3(mDefPtr.element() && other.mDefPtr.element() &&
                mDefPtr.element()->name() == other.mDefPtr.element()->name(),
            "cannot merge ProfileValidators for different elements", std::logic_error);
    Expect3(mSliceName == other.mSliceName, "cannot merge ProfileValidators for different Slices", std::logic_error);
    mParentData.merge(std::move(other.mParentData));
    mSolver.merge(other.mSolver);
    mData->merge(*other.mData);
}


void ProfileValidator::notifyFailed(const MapKey& key)
{
    if (mSolver.fail(key))
    {
        mData->fail();
    }
}

bool ProfileValidator::failed() const
{
    return mData->isFailed() || mSolver.failed();
}

const ProfiledElementTypeInfo& ProfileValidator::definitionPointer() const
{
    return mDefPtr;
}

const fhirtools::ProfileValidator::MapKey& ProfileValidator::key() const
{
    return mData->mapKey();
}

std::list<ProfileValidator::MapKey> ProfileValidator::parentKey() const
{
    std::list<ProfileValidator::MapKey> result;
    for (const auto& pd : mParentData)
    {
        result.emplace_back(pd->mapKey());
    }
    return result;
}


ProfileValidator::CounterKey ProfileValidator::counterKey() const
{
    return CounterKey{std::string{mDefPtr.element()->originalFieldName()}, mSliceName};
}

std::shared_ptr<const ValidationData> fhirtools::ProfileValidator::validationData() const
{
    return mData;
}

std::string fhirtools::to_string(const ProfileValidator::CounterKey& key)
{
    std::ostringstream strm;
    strm << key;
    return strm.str();
}

std::ostream& fhirtools::operator<<(std::ostream& out, const ProfileValidator::CounterKey& key)
{
    out << key.name;
    if (! key.slice.empty())
    {

        out << ':' << key.slice;
    }
    return out;
}

std::ostream& fhirtools::operator<<(std::ostream& out, const ProfileValidator::MapKey& mapKey)
{
    out << '(' << mapKey.defPtr.profile()->url() << '|' << mapKey.defPtr.profile()->version()
        << mapKey.defPtr.element()->originalName() << ')';
    return out;
}

std::string fhirtools::to_string(const ProfileValidator::MapKey& mapKey)
{
    std::ostringstream strm;
    strm << mapKey;
    return strm.str();
}