// (C) Copyright IBM Deutschland GmbH 2022
// (C) Copyright IBM Corp. 2022

#ifndef FHIR_TOOLS_PROFILESETVALIDATOR_HXX
#define FHIR_TOOLS_PROFILESETVALIDATOR_HXX


#include <map>
#include <memory>
#include <set>
#include <string>

#include "fhirtools/validator/internal/ProfileValidator.hxx"

namespace fhirtools
{
class FhirConstraint;
class FhirElement;
class ProfiledElementTypeInfo;
class FhirPathValidator;
class FhirSlice;
class FhirStructureDefinition;
class FhirStructureRepository;
class ProfileValidator;
class ProfileValidatorCounterData;
class ProfileValidatorCounterKey;
class SlicingChecker;
class ValidatorOptions;

/**
 * @brief Validate multiple profiles in Parallel
 *
 * During Validation FHIR-Elements sometimes must follow multiple Profiles.
 * Each instance of ProfileSetValidator handles a single Element. For each of the sub-elements a new instance is
 * created using ProfileSetValidator::subField.
 *
 * The instance of ProfileSetValidator that creates a new instance via ProfileSetValidator::subField also records the
 * following information of the created sub-ProfileSetValidators.
 *  * Counter
 *  * SliceChecker
 *
 * Counters record the number of occurrences of a sub-field including the number of sliced sub-fields.
 * SliceCheckers check, that sliced elements follow the slicing-rules (open, closed, openAtEnd) and order.
 */
class ProfileSetValidator
{
public:
    explicit ProfileSetValidator(ProfiledElementTypeInfo rootPointer, const std::set<ProfiledElementTypeInfo>& defPointers,
                                 const FhirPathValidator& validator);
    ProfileSetValidator(const ProfileSetValidator&) = delete;
    ProfileSetValidator(ProfileSetValidator&&) noexcept;
    ProfileSetValidator& operator=(const ProfileSetValidator&) = delete;
    ProfileSetValidator& operator=(ProfileSetValidator&&) noexcept;

    virtual ~ProfileSetValidator();

    bool isResource() const;
    bool isArray() const;

    ProfiledElementTypeInfo rootPointer() const;

    void typecast(const FhirStructureRepository& repo, const FhirStructureDefinition* structDef);

    void addProfiles(const FhirStructureRepository& repo, const std::set<const FhirStructureDefinition*>& profiles);

    std::shared_ptr<ProfileSetValidator> subField(const FhirStructureRepository&, const std::string& name);

    void process(const Element& element, std::string_view elementFullPath);

    const ProfileValidator& getValidator(const ProfileValidator::MapKey&) const;

    ValidationResultList results() const;

    void finalize(std::string_view elementFullPath);

    const ValidatorOptions& options() const;

private:
    explicit ProfileSetValidator(ProfileSetValidator* parent, ProfiledElementTypeInfo rootPointer);
    void createCounters(const ProfileValidator::Map& profileValidatorMap);
    void incrementCounters();
    void createSliceCheckersAndCounters();
    void createSliceCounters(const ProfileValidator& profVal, const FhirSlicing& slicing);
    ProfileValidator::Map process(ProfileValidator::Map::value_type& profVal, const Element& element,
                                  std::string_view elementFullPath);
    void finalizeChildCounters(std::string_view elementFullPath);
    void finalizeSliceCheckers(std::string_view elementFullPath);
    void propagateFailures();

    using CounterKey = ProfileValidatorCounterKey;
    using CounterData = ProfileValidatorCounterData;

    ProfileSetValidator* mParent = nullptr;
    std::map<ProfileValidatorCounterKey, CounterData> mChildCounters;
    ProfileValidator mRootValidator;
    ProfileValidator::Map mProfileValidators;
    std::set<ProfileValidator::MapKey> mIncludeInResult;
    std::map<ProfiledElementTypeInfo, SlicingChecker> mSliceCheckers;
    std::shared_ptr<const FhirElement> mElementInParent;
    ValidationResultList mResults;
    std::reference_wrapper<const FhirPathValidator> mValidator;
};


}

#endif// FHIR_TOOLS_PROFILESETVALIDATOR_HXX
