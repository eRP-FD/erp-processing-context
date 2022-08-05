/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "fhirtools/repository/FhirSlicing.hxx"

#include <algorithm>
#include <variant>

#include "fhirtools/FPExpect.hxx"
#include "fhirtools/model/Element.hxx"
#include "fhirtools/model/ValueElement.hxx"
#include "fhirtools/repository/FhirElement.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/typemodel/ProfiledElementTypeInfo.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"

using fhirtools::Element;
using fhirtools::ProfiledElementTypeInfo;
using fhirtools::FhirSlice;
using fhirtools::FhirSliceDiscriminator;
using fhirtools::FhirSlicing;
using fhirtools::FhirStructureRepository;
using fhirtools::ValueElement;

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
    bool test(const ::Element& element) const override
    {
        return std::ranges::all_of(mConditions, [&](auto cond) {
            return cond->test(element);
        });
    }
    std::vector<std::shared_ptr<Condition>> mConditions;
};
}

std::shared_ptr<FhirSlicing::Condition>
FhirSlice::condition(const FhirStructureRepository& repo, const std::list<FhirSliceDiscriminator>& discriminators) const
{
    auto cond = std::make_shared<SliceCondition>();
    cond->mConditions.reserve(discriminators.size());
    std::ranges::transform(discriminators, std::back_inserter(cond->mConditions),
                           [&](const FhirSliceDiscriminator& disc) {
                               return disc.condition(repo, &*mImpl->mProfile);
                           });
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
    explicit DiscriminatorCondition(std::string path)
        : mPath{std::move(path)}
    {
    }
    bool test(const ::Element& element) const override
    {
        return test(element, mPath);
    }
    //NOLINTNEXTLINE(misc-no-recursion)
    bool test(const ::Element& element, std::string_view path) const
    {
        using std::min;
        using namespace std::string_view_literals;
        if (path.empty() || path == "$this"sv)
        {
            return testPathElement(element);
        }
        size_t dot = path.find('.');
        auto name = path.substr(0, dot);
        auto rest = path.substr(min(path.size(), name.size() + 1));
        //NOLINTNEXTLINE(misc-no-recursion)
        return std::ranges::any_of(element.subElements(std::string{name}), [&](const auto& subElement) {
            return test(*subElement, rest);
        });
    }

    virtual bool testPathElement(const ::Element&) const = 0;

    std::string mPath;
};

class DiscriminatorValueCondition : public DiscriminatorCondition
{
public:
    explicit DiscriminatorValueCondition(const FhirStructureRepository& repo, std::list<ProfiledElementTypeInfo> defPtrs,
                                         std::string path)
        : DiscriminatorCondition{std::move(path)}
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
            return ValueElement{&repo, dp.element()->fixed()} != fixedValue;
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
    bool testPathElement(const ::Element& element) const override
    {
        const auto* repo = element.getFhirStructureRepository();
        return (element == ValueElement{repo, mFixed});
    }
    std::shared_ptr<const fhirtools::FhirValue> mFixed;
};

class DiscriminatorPatternCondition : public DiscriminatorCondition
{
public:
    explicit DiscriminatorPatternCondition(const FhirStructureRepository& repo,
                                           std::list<ProfiledElementTypeInfo> defPtrs, std::string path)
        : DiscriminatorCondition{std::move(path)}
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
                if (patternValue == existingPatValue)
                {
                    pattern.reset();
                    break;
                }
                if (! patternValue.matches(existingPatValue))
                {
                    LOG(ERROR) << "pattern " << existingPatValue << " contradicts " << patternValue;
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
    bool testPathElement(const ::Element& element) const override
    {
        const auto* repo = element.getFhirStructureRepository();
        return std::ranges::all_of(mPattern, [&](auto pattern) {
            return element.matches(ValueElement{repo, pattern});
        });
    }
    std::vector<std::shared_ptr<const fhirtools::FhirValue>> mPattern;
};


class DiscriminatorExistsCondition : public DiscriminatorCondition
{
public:
    explicit DiscriminatorExistsCondition(const std::list<ProfiledElementTypeInfo>& defPtrs, std::string path)
        : DiscriminatorCondition{std::move(path)}
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
    bool test(const ::Element& element) const override
    {
        return DiscriminatorCondition::test(element) == mShouldExist;
    }
    bool testPathElement(const ::Element&) const override
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
                                        const std::list<ProfiledElementTypeInfo>& defPtrs, std::string path)
        : DiscriminatorCondition{std::move(path)}
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
    bool testPathElement(const ::Element& element) const override
    {
        return element.definitionPointer().element()->typeId() == mType;
    }
    std::string mType;
};

class DiscriminatorProfileCondition : public DiscriminatorCondition
{
public:
    explicit DiscriminatorProfileCondition(const std::list<ProfiledElementTypeInfo>& defPtrs, std::string path)
        : DiscriminatorCondition{std::move(path)}
    {
        for (const auto& dp : defPtrs)
        {
            std::ranges::copy(dp.element()->profiles(), std::inserter(mProfiles, mProfiles.end()));
        }
        Expect(! mProfiles.empty(), "no profile" + mPath);
    }
    bool testPathElement(const ::Element& element) const override
    {
        auto res = fhirtools::FhirPathValidator::validateWithProfiles(
            element.shared_from_this(), element.definitionPointer().element()->originalName(), mProfiles);
        return res.highestSeverity() < fhirtools::Severity::error;
    }
    std::set<std::string> mProfiles;
};


}

std::shared_ptr<FhirSlicing::Condition> FhirSliceDiscriminator::condition(const FhirStructureRepository& repo,
                                                                          const FhirStructureDefinition* def) const
{
    auto defPtrs = resolveElements(repo, def, mPath);
    Expect(! defPtrs.empty(), "no matching elements for descriptor found: " + mPath);
    try
    {
        switch (mType)
        {
            using enum DiscriminatorType;
            case exists:
                return std::make_shared<DiscriminatorExistsCondition>(std::move(defPtrs), mPath);
            case pattern:
                return std::make_shared<DiscriminatorPatternCondition>(repo, std::move(defPtrs), mPath);
            case profile:
                return std::make_shared<DiscriminatorProfileCondition>(std::move(defPtrs), mPath);
            case type:
                return std::make_shared<DiscriminatorTypeCondition>(repo, std::move(defPtrs), mPath);
            case value:
                return std::make_shared<DiscriminatorValueCondition>(repo, defPtrs, mPath);
        }
    }
    catch (const std::logic_error& ex)
    {
        Fail2(mPath + ": " + ex.what(), std::logic_error);
    }
    Fail2("Unexpected value for DiscriminatorType: " + std::to_string(int(mType)), std::logic_error);
}

const std::string& FhirSliceDiscriminator::path() const
{
    return mPath;
}

//NOLINTNEXTLINE(misc-no-recursion)
std::list<ProfiledElementTypeInfo> FhirSliceDiscriminator::resolveElements(const FhirStructureRepository& repo,
                                                                         const FhirStructureDefinition* def,
                                                                         std::string_view path)
{
    Expect3(def != nullptr, "def must not be nullptr", std::logic_error);
    using namespace std::string_view_literals;
    if (path == "$this"sv)
    {
        path = std::string_view{};
    }
    std::list<ProfiledElementTypeInfo> result;
    for (size_t pos = 0;; pos = path.find('.', pos + 1))
    {
        auto element = resolveElement(def, path.substr(0, pos));
        if (! element)
        {
            break;
        }

        std::string_view rest;
        if (pos == std::string_view::npos || path.empty())
        {
            rest = std::string_view{};
        }
        else if (path[pos] != '.')
        {
            rest = path.substr(pos);
        }
        else
        {
            rest = path.substr(pos + 1);
        }
        for (const auto& profileUrl : element->profiles())
        {
            const auto* profile = repo.findDefinitionByUrl(profileUrl);
            Expect3(profile != nullptr, "missing profile: " + profileUrl, std::logic_error);
            result.splice(result.end(), resolveElements(repo, profile, rest));
        }
        result.splice(result.end(), resolveFromValues(repo, element, rest));
        if (pos == std::string_view::npos)
        {
            result.emplace_back(def, element);
            break;
        }
    };
    return result;
}


std::shared_ptr<const fhirtools::FhirElement> FhirSliceDiscriminator::resolveElement(const FhirStructureDefinition* def,
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
FhirSliceDiscriminator::resolveFromValues(std::shared_ptr<const ValueElement> element, std::string_view path)
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
                          resolveFromValues(std::dynamic_pointer_cast<const ValueElement>(subElement), rest));
        }
    }
    return result;
}

std::list<ProfiledElementTypeInfo>
FhirSliceDiscriminator::resolveFromValues(const FhirStructureRepository& repo,
                                          const std::shared_ptr<const FhirElement>& element, std::string_view path)
{
    std::list<ProfiledElementTypeInfo> result;
    if (element->pattern())
    {
        auto patternElements = resolveFromValues(std::make_shared<ValueElement>(&repo, element->pattern()), path);
        for (const auto& patternElement : patternElements)
        {
            FhirElement::Builder builder{*patternElement->definitionPointer().element()};
            builder.pattern(patternElement->value());
            result.emplace_back(patternElement->definitionPointer().profile(), builder.getAndReset());
        }
    }
    if (element->fixed())
    {
        auto fixedElements = resolveFromValues(std::make_shared<ValueElement>(&repo, element->fixed()), path);
        for (const auto& fixedElement : fixedElements)
        {
            FhirElement::Builder builder{*fixedElement->definitionPointer().element()};
            builder.fixed(fixedElement->value());
            result.emplace_back(fixedElement->definitionPointer().profile(), builder.getAndReset());
        }
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
