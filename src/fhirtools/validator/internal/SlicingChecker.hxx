// (C) Copyright IBM Deutschland GmbH 2022
// (C) Copyright IBM Corp. 2022

#ifndef FHIRTOOLS_SLICECHECKER_H
#define FHIRTOOLS_SLICECHECKER_H

#include "fhirtools/repository/FhirElement.hxx"
#include "fhirtools/validator/ValidationResult.hxx"

#include <optional>
#include <set>

namespace fhirtools
{
class ProfileSetValidator;
class FhirSlicing;


/**
 * @brief Checks slicing-rules and order of a FhirSlicing
 *
 * Whenever ProfileSetValidator processes an element it either calls foundSliced(...), if the element belongs to a slice
 * or foundUnsliced, if it doesn't.
 */
class SlicingChecker
{
public:
    explicit SlicingChecker(const FhirStructureDefinition* initBaseProfile,
                            const FhirSlicing& slicing,
                            std::optional<FhirSlicing::SlicingRules> ruleOverride = std::nullopt);
    ~SlicingChecker();
    void foundSliced(const FhirStructureDefinition* sliceProfile, std::string_view fullElementName);
    void foundUnsliced(std::string_view fullElementName);
    void finalize(std::string_view elementFullPath);

    ValidationResults results() &&;
    const ValidationResults& results() const &;
    [[nodiscard]] const std::set<ProfiledElementTypeInfo>& affectedValidators() const;
    void addAffectedValidator(const ProfiledElementTypeInfo&);

private:
    struct SliceData {
        const FhirStructureDefinition* profile{};
        uint32_t count = 0;
    };
    bool mOrdered = false;
    FhirSlicing::SlicingRules mRules;
    std::vector<SliceData> mSingleSlices;
    size_t mLastIdx = 0;
    bool mDone = false;
    std::string mUnmatchedFullName;
    ValidationResults mResult;
    const FhirStructureDefinition* const mBaseProfile;
    std::set<ProfiledElementTypeInfo> mAffectedValidators;
};


}

#endif// FHIRTOOLS_SLICECHECKER_H
