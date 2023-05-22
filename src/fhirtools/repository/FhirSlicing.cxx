/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/repository/FhirSlicing.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/model/Element.hxx"
#include "fhirtools/model/ValueElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"
#include "fhirtools/repository/FhirElement.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/typemodel/ProfiledElementTypeInfo.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "fhirtools/validator/Severity.hxx"

#include <algorithm>
#include <variant>

using fhirtools::Element;
using fhirtools::FhirSlice;
using fhirtools::FhirSliceDiscriminator;
using fhirtools::FhirSlicing;
using fhirtools::FhirStructureRepository;
using fhirtools::ProfiledElementTypeInfo;
using fhirtools::ValueElement;

static constexpr std::string_view resolveFunc = "resolve()";
static constexpr std::string_view dollarThis = "$this";

FhirSlicing::FhirSlicing() = default;
FhirSlicing::FhirSlicing(const FhirSlicing&) = default;
FhirSlicing::FhirSlicing(FhirSlicing&&) noexcept = default;


FhirSlicing::~FhirSlicing() = default;


bool FhirSlicing::ordered() const
{
    return mOrdered;
}

FhirSlicing::SlicingRules FhirSlicing::slicingRules() const
{
    return mSlicingRules;
}

struct FhirSlice::Impl {
    std::string mName;
    std::shared_ptr<FhirStructureDefinition> mProfile;
    mutable std::shared_ptr<FhirSlicing::Condition> mCondition;
};

FhirSlice::FhirSlice(const FhirSlice& other)
    : mImpl{std::make_unique<Impl>(*other.mImpl)}
{
}

FhirSlice::FhirSlice(FhirSlice&& other) noexcept = default;

namespace
{
class SliceCondition : public FhirSlicing::Condition
{
public:
    bool test(const ::Element& element, const fhirtools::ValidatorOptions& opt) const override
    {
        return std::ranges::all_of(mConditions, [&](auto cond) {
            return cond->test(element, opt);
        });
    }
    std::vector<std::shared_ptr<Condition>> mConditions;
};
}

std::shared_ptr<FhirSlicing::Condition>
FhirSlice::condition(const FhirStructureRepository& repo, const std::list<FhirSliceDiscriminator>& discriminators) const
{
    if (mImpl->mCondition)
    {
        return mImpl->mCondition;
    }
    auto cond = std::make_shared<SliceCondition>();
    cond->mConditions.reserve(discriminators.size());
    std::ranges::transform(discriminators, std::back_inserter(cond->mConditions),
                           [&](const FhirSliceDiscriminator& disc) {
                               return disc.condition(repo, &*mImpl->mProfile);
                           });
    mImpl->mCondition = cond;
    return cond;
}


const fhirtools::FhirStructureDefinition& FhirSlice::profile() const
{
    return *mImpl->mProfile;
}

const std::string& FhirSlice::name() const
{
    return mImpl->mName;
}

FhirSliceDiscriminator::DiscriminatorType FhirSliceDiscriminator::type() const
{
    return mType;
}


namespace
{

class DiscriminatorCondition : public FhirSlicing::Condition
{
protected:
    explicit DiscriminatorCondition(fhirtools::ExpressionPtr pathExpression)
        : mPathExpression{std::move(pathExpression)}
    {
    }
    bool test(const fhirtools::Element& element, const fhirtools::ValidatorOptions& opt) const override
    {
        const auto& resultCollection = mPathExpression->eval({element.shared_from_this()});
        return std::ranges::any_of(resultCollection, [&](const auto& subElement) {
            return testPathElement(*subElement, opt);
        });
    }

    virtual bool testPathElement(const ::Element&, const fhirtools::ValidatorOptions&) const = 0;

    fhirtools::ExpressionPtr mPathExpression;
};

class DiscriminatorValueCondition : public DiscriminatorCondition
{
public:
    explicit DiscriminatorValueCondition(const FhirStructureRepository& repo,
                                         std::list<ProfiledElementTypeInfo> defPtrs,
                                         fhirtools::ExpressionPtr pathExpression)
        : DiscriminatorCondition{std::move(pathExpression)}
        , mFixed{}
    {
        defPtrs.remove_if([](const ProfiledElementTypeInfo& defPtr) {
            return defPtr.element()->fixed() == nullptr;
        });

        Expect3(! defPtrs.empty(), "could not determine fixed value for discriminator", std::logic_error);
        //NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer)
        mFixed = defPtrs.front().element()->fixed();
        defPtrs.pop_front();
        ValueElement fixedValue{&repo, mFixed};
        bool ambiguous = std::ranges::any_of(defPtrs, [&](auto dp) {
            return ValueElement{&repo, dp.element()->fixed()}.equals(fixedValue) != true;
        });
        if (ambiguous)
        {
            std::ostringstream values;
            values << "[ " << fixedValue << ", ";
            for (const auto& dp : defPtrs)
            {
                values << ", " << ValueElement{&repo, dp.element()->fixed()};
            }
            values << "]";
            Fail2("ambiguous fixed values in: " + values.str(), std::logic_error);
        }
    }
    bool testPathElement(const ::Element& element, const fhirtools::ValidatorOptions&) const override
    {
        const auto* repo = element.getFhirStructureRepository();
        return (element.equals(ValueElement{repo, mFixed}) == true);
    }
    std::shared_ptr<const fhirtools::FhirValue> mFixed;
};

class DiscriminatorPatternCondition : public DiscriminatorCondition
{
public:
    explicit DiscriminatorPatternCondition(const FhirStructureRepository& repo,
                                           std::list<ProfiledElementTypeInfo> defPtrs,
                                           fhirtools::ExpressionPtr pathExpression)
        : DiscriminatorCondition{std::move(pathExpression)}
    {
        defPtrs.remove_if([](const ProfiledElementTypeInfo& defPtr) {
            return ! defPtr.element()->pattern();
        });
        Expect3(! defPtrs.empty(), "could not determine pattern value for discriminator", std::logic_error);
        mPattern.reserve(defPtrs.size());
        bool contradiction = false;
        for (const auto& dp : defPtrs)
        {
            auto pattern = dp.element()->pattern();
            ValueElement patternValue{&repo, pattern};
            for (const auto& existingPat : mPattern)
            {
                ValueElement existingPatValue{&repo, existingPat};
                if (patternValue.equals(existingPatValue) == true)
                {
                    pattern.reset();
                    break;
                }
                if (! patternValue.matches(existingPatValue))
                {
                    TLOG(ERROR) << "pattern " << existingPatValue << " contradicts " << patternValue;
                    contradiction = true;
                }
            }
            if (! pattern)
            {
                continue;
            }
            mPattern.emplace_back(pattern);
        }
        Expect3(! contradiction, "Contradicting pattern.", std::logic_error);
        mPattern.shrink_to_fit();
    }
    bool testPathElement(const ::Element& element, const fhirtools::ValidatorOptions&) const override
    {
        const auto* repo = element.getFhirStructureRepository();
        return std::ranges::all_of(mPattern, [&](auto pattern) {
            return element.matches(ValueElement{repo, pattern});
        });
    }
    std::vector<std::shared_ptr<const fhirtools::FhirValue>> mPattern;
};


class DiscriminatorBindingConditionBase : public DiscriminatorCondition
{
public:
    explicit DiscriminatorBindingConditionBase(const FhirStructureRepository& repo,
                                               std::list<ProfiledElementTypeInfo> elementInfos,
                                               fhirtools::ExpressionPtr pathExpression)
        : DiscriminatorCondition{std::move(pathExpression)}
    {
        using namespace fhirtools;
        using namespace std::string_literals;
        (void) repo;
        elementInfos.remove_if([](const ProfiledElementTypeInfo& defPtr) {
            return ! defPtr.element()->hasBinding();
        });
        Expect3(! elementInfos.empty(), "could not determine binding value for discriminator", std::logic_error);
        bool contradiction = false;
        std::optional<std::tuple<std::string, std::optional<Version>>> binding;
        for (const auto& info : elementInfos)
        {
            const auto& elementBinding = info.element()->binding();
            if (binding)
            {
                if (*binding != std::make_tuple(elementBinding.valueSetUrl, elementBinding.valueSetVersion))
                {
                    TLOG(ERROR) << "binding " << get<0>(*binding) << '|'
                               << (get<1>(*binding) ? *get<1>(*binding) : "<no-ver>"s) << " contradicts "
                               << elementBinding.valueSetUrl << '|'
                               << (elementBinding.valueSetVersion
                                       ? static_cast<const std::string&>(*elementBinding.valueSetVersion)
                                       : "<no-ver>"s);
                    contradiction = true;
                }
            }
            else
            {
                binding = std::make_tuple(elementBinding.valueSetUrl, elementBinding.valueSetVersion);
            }
        }
        Expect3(! contradiction, "Contradicting binding.", std::logic_error);
        Expect3(binding.has_value(), "Failed to determine Binding", std::logic_error);
        mValueSet = repo.findValueSet(get<0>(*binding), get<1>(*binding));
        Expect3(mValueSet != nullptr,
                "Failed to find ValueSet: " + get<0>(*binding) + '|' +
                    (get<1>(*binding) ? *get<1>(*binding) : "<no-ver>"s),
                std::logic_error);
    }

protected:
    const fhirtools::FhirValueSet* mValueSet = nullptr;
};

class DiscriminatorPrimitiveBindingCondition : public DiscriminatorBindingConditionBase
{
public:
    using DiscriminatorBindingConditionBase::DiscriminatorBindingConditionBase;
    bool testPathElement(const ::Element& element, const fhirtools::ValidatorOptions&) const override
    {
        return mValueSet->containsCode(element.asString());
    }
};

class DiscriminatorCodingBindingCondition : public DiscriminatorBindingConditionBase
{
public:
    using DiscriminatorBindingConditionBase::DiscriminatorBindingConditionBase;
    bool testPathElement(const ::Element& element, const fhirtools::ValidatorOptions&) const override
    {
        auto systemSubElement = element.subElements("system");
        auto codeSubElement = element.subElements("code");
        return (systemSubElement.size() == 1 && codeSubElement.size() == 1) &&
               mValueSet->containsCode(codeSubElement[0]->asString(), systemSubElement[0]->asString());
    }
};

class DiscriminatorCodeableConceptBindingCondition : public DiscriminatorCodingBindingCondition
{
public:
    using DiscriminatorCodingBindingCondition::DiscriminatorCodingBindingCondition;
    bool testPathElement(const ::Element& element, const fhirtools::ValidatorOptions& opt) const override
    {
        auto codingSubElements = element.subElements("coding");
        for (const auto& codingSubElement : codingSubElements)
        {
            if (! DiscriminatorCodingBindingCondition::testPathElement(*codingSubElement, opt))
            {
                return false;
            }
        }
        return true;
    }
};

class DiscriminatorExistsCondition : public DiscriminatorCondition
{
public:
    explicit DiscriminatorExistsCondition(const std::list<ProfiledElementTypeInfo>& defPtrs,
                                          fhirtools::ExpressionPtr pathExpression)
        : DiscriminatorCondition{std::move(pathExpression)}
    {
        std::optional<bool> shouldExist;
        for (const auto& defPtr : defPtrs)
        {
            const auto& c = defPtr.element()->cardinality();
            if (c.min == 0)
            {
                if (c.max.has_value() && c.max.value() == 0)
                {
                    Expect3(! shouldExist.has_value() || ! shouldExist.value(), "Ambiguous exist definitions.",
                            std::logic_error);
                    shouldExist.emplace(false);
                }
            }
            else
            {
                Expect3(! shouldExist.has_value() || shouldExist.value(), "Ambiguous exist definitions.",
                        std::logic_error);
                shouldExist.emplace(true);
            }
        }
        Expect3(shouldExist.has_value(), "undefined value for exists discriminator", std::logic_error);
        mShouldExist = *shouldExist;
    }
    bool test(const ::Element& element, const fhirtools::ValidatorOptions& opt) const override
    {
        return DiscriminatorCondition::test(element, opt) == mShouldExist;
    }
    bool testPathElement(const ::Element&, const fhirtools::ValidatorOptions&) const override
    {
        return true;
    }

private:
    bool mShouldExist;
};

class DiscriminatorTypeCondition : public DiscriminatorCondition
{
public:
    explicit DiscriminatorTypeCondition(const FhirStructureRepository& repo,
                                        const std::list<ProfiledElementTypeInfo>& defPtrs,
                                        fhirtools::ExpressionPtr pathExpression)
        : DiscriminatorCondition{std::move(pathExpression)}
    {
        std::set<std::string> types;
        for (const auto& dp : defPtrs)
        {
            if (! dp.element()->typeId().empty())
            {
                types.insert(dp.element()->typeId());
            }
            else if (dp.element()->contentReference().empty())
            {
                types.insert(dp.profile()->typeId());
            }
            else
            {
                auto resolv =
                    get<ProfiledElementTypeInfo>(repo.resolveBaseContentReference(dp.element()->contentReference()));
                types.insert(resolv.element()->typeId());
            }
        }
        Expect3(types.size() == 1, "multiple types", std::logic_error);
        mType = *types.begin();
    }
    bool testPathElement(const ::Element& element, const fhirtools::ValidatorOptions&) const override
    {
        return element.definitionPointer().element()->typeId() == mType;
    }
    std::string mType;
};

class DiscriminatorProfileCondition : public DiscriminatorCondition
{
public:
    explicit DiscriminatorProfileCondition(const std::list<ProfiledElementTypeInfo>& defPtrs,
                                           fhirtools::ExpressionPtr pathExpression)
        : DiscriminatorCondition{std::move(pathExpression)}
    {
        for (const auto& dp : defPtrs)
        {
            std::ranges::copy(dp.element()->profiles(), std::inserter(mProfiles, mProfiles.end()));
        }
        Expect(! mProfiles.empty(), "no profile");
    }
    bool testPathElement(const ::Element& element, const fhirtools::ValidatorOptions& opt) const override
    {
        try
        {
            auto options = opt;
            options.validateReferences = false;
            auto res = fhirtools::FhirPathValidator::validateWithProfiles(
                element.shared_from_this(), element.definitionPointer().element()->originalName(), mProfiles, options);
            return res.highestSeverity() < fhirtools::Severity::error;
        }
        catch (const std::exception& ex)
        {
            return false;
        }
    }
    std::set<std::string> mProfiles;
};

}// anonymous namespace

std::shared_ptr<FhirSlicing::Condition> FhirSliceDiscriminator::condition(const FhirStructureRepository& repo,
                                                                          const FhirStructureDefinition* def) const
{
    auto elementInfos = collectElements(repo, ProfiledElementTypeInfo{def}, mPath);
    Expect(! elementInfos.empty(), "no matching elements for descriptor found: " + mPath);
    auto pathExpression = FhirPathParser::parse(&repo, mPath);
    try
    {
        switch (mType)
        {
            using enum DiscriminatorType;
            case exists:
                return std::make_shared<DiscriminatorExistsCondition>(std::move(elementInfos),
                                                                      std::move(pathExpression));
            case profile:
                return std::make_shared<DiscriminatorProfileCondition>(std::move(elementInfos),
                                                                       std::move(pathExpression));
            case type:
                return std::make_shared<DiscriminatorTypeCondition>(repo, std::move(elementInfos),
                                                                    std::move(pathExpression));
            case value:
            case pattern:
                return valueishCondition(repo, elementInfos, def, std::move(pathExpression));
        }
    }
    catch (const std::logic_error& ex)
    {
        Fail2(mPath + ": " + ex.what(), std::logic_error);
    }
    Fail2("Unexpected value for DiscriminatorType: " + std::to_string(int(mType)), std::logic_error);
}
std::tuple<bool, bool, bool> fhirtools::FhirSliceDiscriminator::getvalueishConditionProperties(
    const std::list<ProfiledElementTypeInfo>& elementInfos) const
{
    bool pattern = false;
    bool fixed = false;
    bool binding = false;
    for (const auto& info : elementInfos)
    {
        const auto& fhirElement = info.element();
        if (fhirElement->pattern())
        {
            pattern = true;
        }
        if (fhirElement->fixed())
        {
            fixed = true;
        }
        if (fhirElement->hasBinding() && ! fhirElement->pattern() && ! fhirElement->fixed())
        {
            binding = true;
        }
    }
    return std::make_tuple(pattern, fixed, binding);
}
std::shared_ptr<FhirSlicing::Condition> fhirtools::FhirSliceDiscriminator::valueishCondition(
    const fhirtools::FhirStructureRepository& repo, std::list<ProfiledElementTypeInfo> elementInfos,
    const fhirtools::FhirStructureDefinition* def, ExpressionPtr pathExpression) const
{
    auto [pattern, fixed, binding] = getvalueishConditionProperties(elementInfos);

    if (pattern && fixed)
    {
        TLOG(INFO) << "found both pattern and fixed: " << def->url() << '|' << def->version()
                   << " - slicing definition might be ambiguous - using "
                   << (mType == DiscriminatorType::value ? "fixed" : "pattern");
        pattern = mType == DiscriminatorType::pattern;
        fixed = mType == DiscriminatorType::value;
    }
    if (fixed && ! pattern)
    {
        if (mType == DiscriminatorType::pattern)
        {
            TLOG(WARNING) << "Discriminator Type is 'pattern' but only fixed is set: " << def->url() << '|'
                          << def->version() << " - treating as 'value'";
        }
        return std::make_shared<DiscriminatorValueCondition>(repo, std::move(elementInfos), std::move(pathExpression));
    }
    else if (pattern && ! fixed)
    {
        if (mType == DiscriminatorType::value)
        {
            TLOG(INFO) << "Discriminator Type is 'value' but only pattern is set: " << def->url() << '|'
                       << def->version() << " - treating as 'pattern'";
        }
        return std::make_shared<DiscriminatorPatternCondition>(repo, std::move(elementInfos),
                                                               std::move(pathExpression));
    }
    FPExpect3(binding, "Failed to determine value for Discriminator", std::logic_error);
    FPExpect3(! pattern && ! fixed, "pattern of fixed should not be set when treating as binding", std::logic_error);
    const auto& typeId = elementInfos.front().element()->typeId();
    if (typeId == "Coding")
    {
        return std::make_shared<DiscriminatorCodingBindingCondition>(repo, std::move(elementInfos),
                                                                     std::move(pathExpression));
    }
    else if (typeId == "CodeableConcept")
    {
        return std::make_shared<DiscriminatorCodeableConceptBindingCondition>(repo, std::move(elementInfos),
                                                                              std::move(pathExpression));
    }
    else
    {
        const auto* baseDef = repo.findTypeById(typeId);
        Expect3(baseDef != nullptr, "type not found: " + typeId, std::logic_error);
        bool primitive = baseDef->kind() == FhirStructureDefinition::Kind::primitiveType || baseDef->isSystemType();
        Expect3(primitive, "cannot handle binding discriminator for type: " + typeId, std::logic_error);
        return std::make_shared<DiscriminatorPrimitiveBindingCondition>(repo, std::move(elementInfos),
                                                                        std::move(pathExpression));
    }
}


const std::string& FhirSliceDiscriminator::path() const
{
    return mPath;
}

// NOLINTNEXTLINE(misc-no-recursion)
std::list<ProfiledElementTypeInfo> FhirSliceDiscriminator::collectElements(const FhirStructureRepository& repo,
                                                                           const ProfiledElementTypeInfo& parentDef,
                                                                           std::string_view path)
{
    std::list<ProfiledElementTypeInfo> result;
    for (const auto& profileUrl : parentDef.element()->profiles())
    {
        const auto* profile = repo.findDefinitionByUrl(profileUrl);
        Expect3(profile != nullptr, "missing profile: " + profileUrl, std::logic_error);
        result.splice(result.end(), collectSubElements(repo, ProfiledElementTypeInfo{profile}, path));
    }
    result.splice(result.end(), collectSubElements(repo, parentDef, path));
    return result;
}

//NOLINTNEXTLINE(misc-no-recursion)
std::list<ProfiledElementTypeInfo> FhirSliceDiscriminator::collectSubElements(const FhirStructureRepository& repo,
                                                                           const ProfiledElementTypeInfo& parentDef,
                                                                           std::string_view path)
{
    using namespace std::string_literals;
    std::list<ProfiledElementTypeInfo> result;
    if (path.empty())
    {
        return {parentDef};
    }
    size_t dotPos = path.find('.');
    bool hasRest = dotPos != std::string_view::npos;
    std::string_view prefix = path.substr(0, dotPos);
    std::string_view rest = hasRest ? path.substr(dotPos + 1) : std::string_view{};
    if (prefix == resolveFunc)
    {
        return collectFromResolve(repo, parentDef, rest);
    }
    Expect3(!prefix.ends_with("()"), "unsupported funtion in discriminator path: "s.append(prefix), std::logic_error);
    if (prefix == dollarThis)
    {
        return {parentDef};
    }
    for (const auto& subDef : parentDef.subDefinitions(repo, prefix))
    {
        result.splice(result.end(), collectElements(repo, subDef, rest));
    }
    return result;
}


std::shared_ptr<const fhirtools::FhirElement> FhirSliceDiscriminator::collectElement(const FhirStructureDefinition* def,
                                                                                     std::string_view path)
{
    std::string elementId;
    if (path.empty())
    {
        return def->rootElement();
    }
    elementId.reserve(def->typeId().size() + path.size() + 1);
    elementId.append(def->typeId()).append(1, '.').append(path);
    return def->findElement(elementId);
}

std::list<std::shared_ptr<const ValueElement>>
//NOLINTNEXTLINE(misc-no-recursion)
FhirSliceDiscriminator::collectFromValues(std::shared_ptr<const ValueElement> element, std::string_view path)
{
    std::list<std::shared_ptr<const ValueElement>> result;
    if (path.empty())
    {
        return {std::move(element)};
    }
    size_t dot = path.find('.');
    const std::string_view subName = path.substr(0, dot);
    std::string_view rest;
    if (dot != std::string_view::npos)
    {
        rest = path.substr(dot + 1);
    }
    auto subNameList = element->subElementNames();
    if (std::ranges::find(subNameList, subName) != subNameList.end())
    {
        for (const auto& subElement : element->subElements(std::string{subName}))
        {
            result.splice(result.end(),
                          collectFromValues(std::dynamic_pointer_cast<const ValueElement>(subElement), rest));
        }
    }
    return result;
}

std::list<ProfiledElementTypeInfo>
FhirSliceDiscriminator::collectFromValues(const FhirStructureRepository& repo,
                                          const std::shared_ptr<const FhirElement>& element, std::string_view path)
{
    std::list<ProfiledElementTypeInfo> result;
    if (element->pattern())
    {
        auto patternElements = collectFromValues(std::make_shared<ValueElement>(&repo, element->pattern()), path);
        for (const auto& patternElement : patternElements)
        {
            FhirElement::Builder builder{*patternElement->definitionPointer().element()};
            builder.pattern(patternElement->value());
            result.emplace_back(patternElement->definitionPointer().profile(), builder.getAndReset());
        }
    }
    if (element->fixed())
    {
        auto fixedElements = collectFromValues(std::make_shared<ValueElement>(&repo, element->fixed()), path);
        for (const auto& fixedElement : fixedElements)
        {
            FhirElement::Builder builder{*fixedElement->definitionPointer().element()};
            builder.fixed(fixedElement->value());
            result.emplace_back(fixedElement->definitionPointer().profile(), builder.getAndReset());
        }
    }
    return result;
}

std::list<ProfiledElementTypeInfo>
// NOLINTNEXTLINE(misc-no-recursion)
fhirtools::FhirSliceDiscriminator::collectFromResolve(const fhirtools::FhirStructureRepository& repo,
                                                      const fhirtools::ProfiledElementTypeInfo& parentDef,
                                                      std::string_view path)
{
    std::list<ProfiledElementTypeInfo> result;
    for (const auto& profileUrl : parentDef.element()->referenceTargetProfiles())
    {
        const auto* profile = repo.findDefinitionByUrl(profileUrl);
        Expect3(profile != nullptr, "missing profile: " + profileUrl, std::logic_error);
        auto elementDef = FhirElement::Builder{*profile->rootElement()}.addProfile(profileUrl).getAndReset();
        ProfiledElementTypeInfo resolveParent{profile, elementDef};
        result.splice(result.end(), collectElements(repo, resolveParent, path));
    }
    return result;
}


FhirSlice::FhirSlice()
    : mImpl(std::make_unique<Impl>())
{
}

FhirSlice::~FhirSlice() = default;

const std::vector<FhirSlice>& FhirSlicing::slices() const
{
    return mSlices;
}

// this allows use of a forward declaration in the header
class FhirSlicing::Builder::FhirStructureDefinitionBuilder : public FhirStructureDefinition::Builder
{
public:
    using Builder::Builder;
};

FhirSlicing::Builder::Builder()
    : mFhirSlicing{new FhirSlicing{}}
{
}

FhirSlicing::Builder::Builder(FhirSlicing::Builder&&) noexcept = default;

FhirSlicing::Builder::Builder(FhirSlicing slicingTemplate, std::string prefix)
    : mSlicePrefix{std::move(prefix)}
    , mFhirSlicing{std::make_unique<FhirSlicing>(std::move(slicingTemplate))}
{
}


FhirSlicing::Builder::~Builder() = default;

FhirSlicing::Builder& FhirSlicing::Builder::slicePrefix(std::string sp)
{
    mSlicePrefix = std::move(sp);
    return *this;
}

std::string FhirSlicing::Builder::slicePrefix() const
{
    return mSlicePrefix;
}

FhirSlicing::Builder& FhirSlicing::Builder::slicingRules(FhirSlicing::SlicingRules rules)
{
    mFhirSlicing->mSlicingRules = rules;
    return *this;
}

FhirSlicing::Builder& FhirSlicing::Builder::ordered(bool ord)
{
    mFhirSlicing->mOrdered = ord;
    return *this;
}
bool FhirSlicing::Builder::addElement(const std::shared_ptr<const FhirElement>& element,
                                      std::list<std::string> withTypes, const FhirStructureDefinition& containing)
{
    Expect(! mSlicePrefix.empty(), "Prefix not set - Cannot add element.");
    const auto& inElementName = element->name();
    Expect(inElementName.starts_with(mSlicePrefix + ':') && inElementName.size() > mSlicePrefix.size() + 1,
           "Element " + element->name() + " is not a slice of " + mSlicePrefix);
    const auto& inSliceId = sliceId(inElementName);
    Expect(! inSliceId.empty(), "sliceId not found in " + inElementName);

    if (mSliceId != inSliceId)
    {
        if (inElementName.size() > inSliceId.size() + mSlicePrefix.size() + 1)
        {
            return false;
        }
        if (mFhirStructureBuilder)
        {
            commitSlice();
        }
        mFhirStructureBuilder = std::make_unique<FhirStructureDefinitionBuilder>();
        Expect(inElementName.find_first_of(".:", mSlicePrefix.size() + 1) == std::string::npos,
               "Element " + inElementName + " is not base Element of " + mSlicePrefix);
        mSliceId = inSliceId;
        mFhirStructureBuilder->typeId(mSlicePrefix);
        mFhirStructureBuilder->url(containing.url() + ':' + inSliceId);
        mFhirStructureBuilder->version(containing.version());
        mFhirStructureBuilder->name(':' + mSliceId);
        mFhirStructureBuilder->kind(FhirStructureDefinition::Kind::slice);
        mFhirStructureBuilder->derivation(FhirStructureDefinition::Derivation::constraint);
    }
    else
    {
        Expect(inElementName.compare(mSlicePrefix.size() + 1, mSliceId.size(), mSliceId) == 0,
               inElementName + " doesn't belong to slice " + mSliceId);
    }
    std::ostringstream elementName;
    elementName << mSlicePrefix << inElementName.substr(mSlicePrefix.size() + mSliceId.size() + 1);
    mFhirStructureBuilder->addElement(FhirElement::Builder{*element}.name(elementName.str()).getAndReset(),
                                      std::move(withTypes));
    return true;
}


FhirSlicing::Builder& FhirSlicing::Builder::addDiscriminator(FhirSliceDiscriminator&& discriminator)
{
    mFhirSlicing->mDiscriminators.emplace_back(std::move(discriminator));
    return *this;
}


FhirSlicing FhirSlicing::Builder::getAndReset()
{
    if (mFhirStructureBuilder)
    {
        commitSlice();
    }
    mSlicePrefix.clear();
    auto result = std::move(mFhirSlicing);
    mFhirSlicing = std::make_unique<FhirSlicing>();
    return std::move(*result);
}

void FhirSlicing::Builder::commitSlice()
{
    FhirSlice::Builder slice;
    slice.name(mSliceId);
    slice.profile(mFhirStructureBuilder->getAndReset());
    mFhirSlicing->mSlices.emplace_back(slice.getAndReset());
    mSliceId.clear();
}


std::string FhirSlicing::Builder::sliceId(const std::string& elementName)
{
    using namespace std::string_view_literals;
    auto begin = elementName.begin() + gsl::narrow<ptrdiff_t>(mSlicePrefix.size()) + 1;
    return {begin, std::find(begin, elementName.end(), '.')};
}

FhirSlice::Builder::Builder()
    : mFhirSlice{new FhirSlice{}}
{
}

FhirSlice::Builder& FhirSlice::Builder::name(std::string n)
{
    mFhirSlice->mImpl->mName = std::move(n);
    return *this;
}

FhirSlice::Builder& FhirSlice::Builder::profile(FhirStructureDefinition&& prof)
{
    mFhirSlice->mImpl->mProfile = std::make_unique<FhirStructureDefinition>(std::move(prof));
    return *this;
}


FhirSlice FhirSlice::Builder::getAndReset()
{
    Expect(mFhirSlice->mImpl->mProfile != nullptr, "Profile for slice was not set.");
    auto result = std::move(mFhirSlice);
    mFhirSlice = std::make_unique<FhirSlice>();
    return std::move(*result);
}


FhirSliceDiscriminator::Builder::Builder()
    : mSliceDiscriminator{std::make_unique<FhirSliceDiscriminator>()}
{
}

FhirSliceDiscriminator::Builder& FhirSliceDiscriminator::Builder::path(std::string p)
{
    mSliceDiscriminator->mPath = std::move(p);
    return *this;
}

FhirSliceDiscriminator::Builder& ::FhirSliceDiscriminator::Builder::type(FhirSliceDiscriminator::DiscriminatorType dt)
{
    mSliceDiscriminator->mType = dt;
    return *this;
}

FhirSliceDiscriminator FhirSliceDiscriminator::Builder::getAndReset()
{
    auto result = std::move(mSliceDiscriminator);
    mSliceDiscriminator = std::make_unique<FhirSliceDiscriminator>();
    return std::move(*result);
}

const std::list<FhirSliceDiscriminator>& FhirSlicing::discriminators() const
{
    return mDiscriminators;
}
