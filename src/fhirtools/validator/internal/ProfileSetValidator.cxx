// (C) Copyright IBM Deutschland GmbH 2022
// (C) Copyright IBM Corp. 2022

#include "fhirtools/validator/internal/ProfileSetValidator.hxx"

#include <ranges>
#include <type_traits>

#include "fhirtools/FPExpect.hxx"
#include "fhirtools/model/Element.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "fhirtools/validator/internal/SlicingChecker.hxx"

using namespace fhirtools;


ProfileSetValidator::ProfileSetValidator(ProfileSetValidator&&) noexcept = default;
ProfileSetValidator& ProfileSetValidator::operator=(ProfileSetValidator&&) noexcept = default;

fhirtools::ProfileSetValidator::ProfileSetValidator(fhirtools::ProfiledElementTypeInfo rootPointer,
                                                    const std::set<ProfiledElementTypeInfo>& defPointers,
                                                    const FhirPathValidator& validator)
    : mRootValidator(rootPointer)
    , mValidator{validator}
{
    for (const auto& defPtr : defPointers)
    {
        ProfileValidator::MapKey mapKey{defPtr};
        mProfileValidators.emplace(mapKey, ProfileValidator{defPtr});
        mIncludeInResult.emplace(std::move(mapKey));
    }
    ProfileValidator::MapKey mapKey{mRootValidator.definitionPointer()};
    mProfileValidators.emplace(mapKey, ProfileValidator{std::move(rootPointer)});
    mIncludeInResult.emplace(std::move(mapKey));
}

ProfileSetValidator::ProfileSetValidator(ProfileSetValidator* parent, ProfiledElementTypeInfo rootPointer)
    : mParent(parent)
    , mRootValidator{std::move(rootPointer)}
    , mValidator{parent->mValidator}
{
}


ProfileSetValidator::~ProfileSetValidator() = default;


ProfiledElementTypeInfo ProfileSetValidator::rootPointer() const
{
    return mRootValidator.definitionPointer();
}


bool ProfileSetValidator::isResource() const
{
    return rootPointer().isResource();
}

bool ProfileSetValidator::isArray() const
{
    return mElementInParent && mElementInParent->isArray();
}

void ProfileSetValidator::typecast(const FhirStructureRepository& repo, const FhirStructureDefinition* structDef)
{
    mRootValidator.typecast(repo, structDef);
    for (auto&& profVal : mProfileValidators)
    {
        const auto& defPtr = profVal.second.definitionPointer();
        if (! defPtr.element()->isRoot() || defPtr.profile()->isDerivedFrom(repo, *structDef))
        {
            continue;
        }
        profVal.second.typecast(repo, structDef);
    }
}

void ProfileSetValidator::addProfiles(const FhirStructureRepository& repo,
                                      const std::set<const FhirStructureDefinition*>& profiles)
{
    Expect3(rootPointer().element()->isRoot(), "cannot add profiles to non-root: " + to_string(rootPointer()),
            std::logic_error);
    for (const auto* prof : profiles)
    {
        Expect3(prof != nullptr, "profiles set must not contain null", std::logic_error);
        if (prof->isDerivedFrom(repo, *rootPointer().profile()))
        {
            ProfiledElementTypeInfo defPtr{prof};
            ProfileValidator::MapKey mapKey{defPtr};
            mIncludeInResult.emplace(mapKey);
            mProfileValidators.emplace(std::move(mapKey), ProfileValidator{defPtr});
        }
    }
}

std::shared_ptr<ProfileSetValidator> ProfileSetValidator::subField(const FhirStructureRepository& repo,
                                                                   const std::string& name)
{
    auto rootList = rootPointer().subDefinitions(repo, name);
    FPExpect(! rootList.empty(), rootPointer().profile()->url() + '|' + rootPointer().profile()->version() +
                                     " field resolution failed: " + rootPointer().element()->name() + '.' + name);
    auto result = std::shared_ptr<ProfileSetValidator>(new ProfileSetValidator{this, rootList.back()});
    auto subField = rootPointer().subField(name);
    Expect3(subField.has_value(), "no such field: " + name, std::logic_error);
    result->mParent = this;
    result->mElementInParent = subField->element();
    auto originalName = subField->element()->originalName();
    for (auto& profVal : mProfileValidators)
    {
        auto subVal = profVal.second.subFieldValidators(repo, name);
        result->mProfileValidators.merge(std::move(subVal));
    }
    result->createCounters(result->mProfileValidators);
    result->createSliceCheckersAndCounters();
    return result;
}

void ProfileSetValidator::process(const Element& element, std::string_view elementFullPath)
{
    using namespace std::string_literals;
    using namespace std::string_view_literals;
    TVLOG(3) << "pre - " << (mElementInParent ? mElementInParent->originalName() : "root") << ": "
             << mProfileValidators.size() << " profiles in set";
    for (const auto& prof : mProfileValidators)
    {
        TVLOG(4) << prof.first << (prof.second.failed() ? " (failed)" : " (good)");
    }
    mRootValidator.process(element, elementFullPath);
    if (element.hasValue())
    {
        ++mChildCounters[{"value", {}}].count;
    }
    ProfileValidator::Map toValidate;
    toValidate.swap(mProfileValidators);
    while (! toValidate.empty())
    {
        ProfileValidator::Map addedProfiles;
        for (auto&& profValid : toValidate)
        {
            std::ranges::move(process(profValid, element, elementFullPath),
                              std::inserter(addedProfiles, addedProfiles.end()));
        }
        ProfileValidator::Map forMerge;
        forMerge.swap(toValidate);
        mProfileValidators.merge(std::move(forMerge));
        toValidate = std::move(addedProfiles);
    }
    incrementCounters();
}

void fhirtools::ProfileSetValidator::createCounters(const ProfileValidator::Map& profileValidatorMap)
{
    if (mParent)
    {
        for (const auto& profVal : profileValidatorMap)
        {
            const auto& defPtr = profVal.second.definitionPointer();
            if (! defPtr.element()->cardinality().isConstraint(defPtr.element()->isArray()))
            {
                continue;
            }
            const auto& counterKey = profVal.second.counterKey();
            for (const auto& pk : profVal.second.parentKey())
            {
                mParent->mChildCounters[counterKey].elementMap.emplace(pk, defPtr);
            }
        }
    }
}

void fhirtools::ProfileSetValidator::incrementCounters()
{
    if (mParent)
    {
        std::set<CounterKey> incrementedCounters;
        for (const auto& profVal : mProfileValidators)
        {
            CounterKey key{profVal.second.counterKey()};
            auto counter = mParent->mChildCounters.find(key);
            if (counter == mParent->mChildCounters.end() || ! incrementedCounters.insert(key).second)
            {
                continue;
            }
            ++counter->second.count;
        }
    }
}

void fhirtools::ProfileSetValidator::createSliceCheckersAndCounters()
{
    using namespace std::string_view_literals;
    if (mParent)
    {
        const bool checkExtension =
            options().reportUnknownExtensions && rootPointer() == mValidator.get().extensionRootDefPtr();
        bool extensionChecked = false;
        std::optional<FhirSlicing::SlicingRules> ruleOverride;
        if (checkExtension)
        {
            ruleOverride = FhirSlicing::SlicingRules::reportOther;
        }
        for (auto& profVal : mProfileValidators)
        {
            const auto& defPtr = profVal.second.definitionPointer();
            if (defPtr.element()->hasSlices())
            {
                extensionChecked = true;
                const auto& slicing = defPtr.element()->slicing();
                auto [it, ins] = mParent->mSliceCheckers.try_emplace(defPtr, defPtr.profile(), *slicing, ruleOverride);
                TVLOG(3) << "Adding Slicechecker for " << defPtr.profile()->url() << '|' << defPtr.profile()->version()
                         << "@" << defPtr.element()->name();
                for (const auto& pk : profVal.second.parentKey())
                {
                    it->second.addAffectedValidator(pk);
                }
                if (ins)
                {
                    createSliceCounters(profVal.second, *slicing);
                }
            }
        }
        if (! extensionChecked && checkExtension)
        {
            // no extensions defined for this Extension-Element - set reportOther using base definition
            const auto& extensionDef = mValidator.get().elementExtensionDefPtr();
            const auto& slicing = extensionDef.element()->slicing();
            auto [it, ins] = mParent->mSliceCheckers.try_emplace(rootPointer(), rootPointer().profile(),
                                                                 slicing.value(), ruleOverride);
            it->second.addAffectedValidator(mParent->mRootValidator.key());
            TVLOG(3) << "Adding Extension-Slicechecker";
        }
    }
}

void fhirtools::ProfileSetValidator::createSliceCounters(const ProfileValidator& profVal, const FhirSlicing& slicing)
{
    const auto& defPtr = profVal.definitionPointer();
    bool isArray = defPtr.element()->isArray();
    for (const auto& slice : slicing.slices())
    {
        TVLOG(4) << "    with slice:" << slice.name();
        static_assert(std::is_reference_v<decltype(slice.profile())>);
        ProfiledElementTypeInfo slicePtr{&slice.profile()};
        for (auto pk : profVal.parentKey())
        {
            if (slicePtr.element()->cardinality().isConstraint(isArray))
            {
                auto originalFieldName = slicePtr.element()->originalFieldName();
                CounterKey ckey{std::string{originalFieldName}, slice.name()};
                mParent->mChildCounters[ckey].elementMap.emplace(std::move(pk), slicePtr);
            }
        }
    }
}


ProfileValidator::Map fhirtools::ProfileSetValidator::process(ProfileValidator::Map::value_type& profValid,
                                                              const fhirtools::Element& element,
                                                              std::string_view elementFullPath)
{
    using namespace std::string_view_literals;
    TVLOG(3) << "processing: " << profValid.first << "(" << profValid.second.definitionPointer() << ")";
    auto profValidatorResult = profValid.second.process(element, elementFullPath);
    const auto& defPtr = profValid.second.definitionPointer();
    const auto& slicing = defPtr.element()->slicing();
    auto haveSlices = slicing && ! slicing->slices().empty();
    Expect3(profValidatorResult.sliceProfiles.empty() || haveSlices, "got slice profiles but no slicing", std::logic_error);
    if (profValidatorResult.sliceProfiles.size() > 1)
    {
        std::ostringstream msg;
        msg << "element belongs to more than one slice: [";
        std::string_view sep{};
        for (const auto& sliceProf : profValidatorResult.sliceProfiles)
        {
            msg << sep << sliceProf->getName();
            sep = ", ";
        }
        msg << "]";
        mResults.add(Severity::error, msg.str(), std::string{elementFullPath}, defPtr.profile());
    }
    ProfileValidator::Map addedProfiles;
    for (auto&& sValid : profValidatorResult.extraValidators)
    {
        if (! mProfileValidators.contains(sValid.first))
        {
            addedProfiles.emplace(std::move(sValid));
        }
    }
    if (mParent)
    {
        if (profValidatorResult.sliceProfiles.empty())
        {
            auto sliceChecker = mParent->mSliceCheckers.find(defPtr);
            if (sliceChecker != mParent->mSliceCheckers.end())
            {
                sliceChecker->second.foundUnsliced(elementFullPath);
            }
        }
        else
        {
            for (const auto* sliceProf: profValidatorResult.sliceProfiles)
            {
                mParent->mSliceCheckers.at(defPtr).foundSliced(sliceProf, elementFullPath);
            }
        }
    }
    createCounters(addedProfiles);
    return addedProfiles;
}


const ProfileValidator& ProfileSetValidator::getValidator(const ProfileValidator::MapKey& ptr) const
{
    auto result = mProfileValidators.find(ptr);
    Expect3(result != mProfileValidators.end(), "no validator for: " + to_string(ptr), std::logic_error);
    return result->second;
}

void fhirtools::ProfileSetValidator::finalize(std::string_view elementFullPath)
{
    TVLOG(4) << __PRETTY_FUNCTION__ << ": " << mRootValidator.definitionPointer() << mProfileValidators.size();
    finalizeChildCounters(elementFullPath);
    finalizeSliceCheckers(elementFullPath);
    propagateFailures();
}

void fhirtools::ProfileSetValidator::finalizeChildCounters(std::string_view elementFullPath)
{
    for (const auto& counter : mChildCounters)
    {
        counter.second.check(mProfileValidators, counter.first, elementFullPath);
    }
}


void ProfileSetValidator::finalizeSliceCheckers(std::string_view elementFullPath)
{
    for (auto& sliceChecker : mSliceCheckers)
    {
        sliceChecker.second.finalize(elementFullPath);
        auto slicingCheckResults = sliceChecker.second.results();
        for (const auto& affected : sliceChecker.second.affectedValidators())
        {
            const auto& profVal = mProfileValidators.find(affected);
            Expect3(profVal != mProfileValidators.end(), "validator not found for slice checker", std::logic_error);
            profVal->second.appendResults(slicingCheckResults);
        }
    }
}

void fhirtools::ProfileSetValidator::propagateFailures()
{
    std::set<ProfileValidator::MapKey> failed;
    for (auto& profVal : mProfileValidators)
    {
        if (profVal.second.failed())
        {
            TVLOG(3) << "failed: " << profVal.first;
            failed.insert(profVal.first);
        }
    }
    auto newFailed = failed;
    while (! newFailed.empty())
    {
        std::set<ProfileValidator::MapKey> tmp;
        tmp.swap(newFailed);
        for (const auto& f : tmp)
        {
            for (auto& profVal : mProfileValidators)
            {
                profVal.second.notifyFailed(f);
                if (profVal.second.failed() && failed.insert(profVal.first).second)
                {
                    newFailed.insert(profVal.first);
                    TVLOG(2) << profVal.first << " failed due to failure in: " << f;
                }
            }
        }
    }
    for (auto& profVal : mProfileValidators)
    {
        profVal.second.finalize();
    }
    if (! failed.empty())
    {
        std::ostringstream msg;
        msg << "Failed profiles: ";
        std::string_view sep{};
        for (const auto& f : failed)
        {
            msg << sep << f.defPtr.profile()->url() << '|' << f.defPtr.profile()->version();
            sep = ", ";
        }
        TVLOG(3) << msg.str();
    }
}

const fhirtools::ValidatorOptions& fhirtools::ProfileSetValidator::options() const
{
    return mValidator.get().options();
}


ValidationResultList ProfileSetValidator::results() const
{
    using namespace std::string_literals;
    using std::to_string;
    ValidationResultList result;
    std::set<ProfileValidator::MapKey> added;
    for (const auto& defPtr : mIncludeInResult)
    {
        if (added.insert(defPtr).second)
        {
            TVLOG(4) << "adding results from " << defPtr;
            result.append(getValidator(defPtr).results());
        }
    }
    result.append(mResults);
    return result;
}
