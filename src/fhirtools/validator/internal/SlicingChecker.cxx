// (C) Copyright IBM Deutschland GmbH 2022
// (C) Copyright IBM Corp. 2022

#include "fhirtools/validator/internal/SlicingChecker.hxx"
#include "erp/util/TLog.hxx"

#include "fhirtools/validator/FhirPathValidator.hxx"
#include "fhirtools/validator/internal/ProfileSetValidator.hxx"

using fhirtools::SlicingChecker;
fhirtools::SlicingChecker::SlicingChecker(const fhirtools::FhirStructureDefinition* initBaseProfile,
                                          const fhirtools::FhirSlicing& slicing,
                                          std::optional<FhirSlicing::SlicingRules> ruleOverride)
    : mOrdered{slicing.ordered()}
    , mRules{ruleOverride ? *ruleOverride : slicing.slicingRules()}
    , mBaseProfile{initBaseProfile}
{
    for (const auto& slice : slicing.slices())
    {
        mSingleSlices.emplace_back(SliceData{&slice.profile()});
    }
}

SlicingChecker::~SlicingChecker() = default;

void fhirtools::SlicingChecker::foundSliced(const fhirtools::FhirStructureDefinition* sliceProfile,
                                            std::string_view fullElementName)
{
    using namespace std::string_literals;
    auto data = std::ranges::find_if(mSingleSlices, [sliceProfile](const SliceData& d) {
        return d.profile == sliceProfile;
    });
    if (data == mSingleSlices.end())
    {
        return;
    }
    ++data->count;
    if (mRules == FhirSlicing::SlicingRules::openAtEnd && mDone)
    {
        mResult.add(Severity::error,
                    "element matching slice "s.append(sliceProfile->getName()) + " after unmatched element " +
                        mUnmatchedFullName + " in Slicing with rule openAtEnd",
                    std::string{fullElementName}, sliceProfile);
    }
    size_t idx = data - mSingleSlices.begin();
    if (mOrdered && idx < mLastIdx)
    {
        mResult.add(Severity::error, "slicing out of order", std::string{fullElementName}, mBaseProfile);
    }
    mLastIdx = idx;
}

void SlicingChecker::foundUnsliced(std::string_view fullElementName)
{
    mUnmatchedFullName = fullElementName;
    switch (mRules)
    {
        using enum FhirSlicing::SlicingRules;
        case open:
            return;
        case reportOther:
            mResult.add(Severity::unslicedWarning, "element doesn't belong to any slice.", std::string{fullElementName},
                        mBaseProfile);
            break;
        case closed:
            mResult.add(Severity::error, "element doesn't match any slice in closed slicing",
                        std::string{fullElementName}, mBaseProfile);
            break;
        case openAtEnd:
            break;
    }
    mDone = true;
}

void SlicingChecker::finalize(std::string_view elementFullPath)
{
    for (const auto& slice : mSingleSlices)
    {
        const auto& rootElement = slice.profile->rootElement();
        std::ostringstream name;
        name << elementFullPath << '.' << rootElement->fieldName();
        mResult.append(rootElement->cardinality().check(slice.count, name.str(), slice.profile));
    }
}

fhirtools::ValidationResultList fhirtools::SlicingChecker::results() &&
{
    return std::move(mResult);
}

const fhirtools::ValidationResultList& fhirtools::SlicingChecker::results() const&
{
    return mResult;
}

void SlicingChecker::addAffectedValidator(const fhirtools::ProfiledElementTypeInfo& key)
{
    mAffectedValidators.emplace(key);
}

const std::set<fhirtools::ProfiledElementTypeInfo>& SlicingChecker::affectedValidators() const
{
    return mAffectedValidators;
}
