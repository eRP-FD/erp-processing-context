// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
//
// non-exclusively licensed to gematik GmbH

#ifndef FHIRTOOLS_PROFILEVALIDATOR_H
#define FHIRTOOLS_PROFILEVALIDATOR_H


#include <compare>
#include <map>
#include <tuple>
#include <unordered_set>

#include "fhirtools/repository/FhirElement.hxx"
#include "fhirtools/typemodel/ProfiledElementTypeInfo.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "fhirtools/validator/internal/ProfileSolver.hxx"

namespace fhirtools
{

class Element;
class FhirStructureRepository;
class ProfileSetValidator;
class ProfileValidator;
class ProfileValidatorCounterData;
class ProfileValidatorCounterKey;
class FhirStructureDefinition;
class FhirValueSet;
class ValidationData;

/**
 * @brief Key for the Counter map in ProfileSetValidator
 */
class ProfileValidatorCounterKey
{
public:
    ProfileValidatorCounterKey(std::string initName, std::string initSlice);
    const std::string name;
    const std::string slice{};
    //NOLINTNEXTLINE(hicpp-use-nullptr,modernize-use-nullptr)
    auto operator<=>(const ProfileValidatorCounterKey&) const = default;
    bool operator==(const ProfileValidatorCounterKey&) const = default;
};

/**
 * @brief Validates a given element to a single Profile
 *
 * Like ProfileSetValidator the ProfileValidator checks a single Element only.
 *
 * Sub elements are then Validated using the ProfileValidator instance provided by subFieldValidators
 * ProfileValidator has a reference to the ProfileValidator, that created it and propagates any Errors found when then
 * finalize function is called.
 *
 * The map of ProfileValidators returned by subFieldValidators may contain dependent validators like ProfileValidators
 * that validate Profiles specified in Element.type.profile
 * (https://simplifier.net/packages/hl7.fhir.r4.core/4.0.1/files/81633)
 * The finalize function will query these ProfileValidators, when the finalize function is called using the
 * ProfileSolver class.
 */
class ProfileValidator
{
public:
    using MapKey = ProfiledElementTypeInfo;
    using CounterKey = ProfileValidatorCounterKey;
    using CounterData = ProfileValidatorCounterData;

    explicit ProfileValidator(const ProfileValidator&) = delete;
    ProfileValidator(ProfileValidator&&) noexcept;
    ProfileValidator& operator=(const ProfileValidator&) = delete;
    ProfileValidator& operator=(ProfileValidator&&) noexcept;
    virtual ~ProfileValidator() = default;

    explicit ProfileValidator(ProfiledElementTypeInfo defPtr, const ProfileSetValidator&);
    void merge(ProfileValidator&&);

    class Map : public std::map<MapKey, ProfileValidator>
    {
    public:
        Map(const Map&) = delete;
        Map(Map&&) = default;
        Map& operator=(const Map&) = delete;
        Map& operator=(Map&&) = default;
        using map::map;
        void merge(Map);
    };
    Map subFieldValidators(const FhirStructureRepository&, std::string_view name);
    void typecast(const FhirStructureRepository& repo, const FhirStructureDefinition* structDef);
    struct ProcessingResult
    {
        std::list<const FhirStructureDefinition*> sliceProfiles;
        Map extraValidators;
    };
    ProcessingResult process(const Element& element, std::string_view elementFullPath);
    void appendResults(ValidationResults results);
    void finalize();
    ValidationResults results() const;

    void notifyFailed(const MapKey& key);
    bool failed() const;
    const ProfiledElementTypeInfo& definitionPointer() const;
    const MapKey& key() const;
    std::list<MapKey> parentKey() const;
    CounterKey counterKey() const;
    std::shared_ptr<const ValidationData> validationData() const;

protected:
    ProfileValidator(MapKey mapKey, std::set<std::shared_ptr<ValidationData>> parentData,
                     ProfiledElementTypeInfo defPtr, std::string sliceName, const ProfileSetValidator&);
    void checkBinding(const Element& element, std::string_view elementFullPath);
    void validateBinding(const fhirtools::Element& element, const FhirElement::Binding& binding,
                         const FhirValueSet* bindingValueSet, std::string_view elementFullPath);
    void checkCodingBinding(const Element& codingElement, const FhirValueSet* fhirValueSet,
                            std::string_view elementFullPath, Severity errorSeverity);
    void checkConstraints(const Element& element, std::string_view elementFullPath);
    void checkValue(const Element& element, std::string_view elementFullPath);
    void checkValueMaxLength(const Element& element, std::string_view elementFullPath);
    void checkValueMinValue(const Element& element, std::string_view elementFullPath);
    void checkValueMaxValue(const Element& element, std::string_view elementFullPath);
    void checkCoding(const Element& element, std::string_view elementFullPath);
    void checkValueNotEmpty(const Element& element, std::string_view elementFullPath);


    std::shared_ptr<ValidationData> mData;
    std::set<std::shared_ptr<ValidationData>> mParentData;
    ProfiledElementTypeInfo mDefPtr;
    std::string mSliceName;
    ProfileSolver mSolver;
    std::reference_wrapper<const ProfileSetValidator> mSetValidator;
};

class ProfileValidatorCounterData
{
public:

    std::map<ProfiledElementTypeInfo, ProfiledElementTypeInfo> elementMap;
    uint32_t count = 0;
    void check(ProfileValidator::Map& profMap, const ProfileValidatorCounterKey& cKey,
               std::string_view elementFullPath) const;
};

std::ostream& operator<<(std::ostream& out, const ProfileValidator::CounterKey& key);
std::string to_string(const ProfileValidator::CounterKey& key);


}

#endif// FHIRTOOLS_PROFILEVALIDATOR_H
