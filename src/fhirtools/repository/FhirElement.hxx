/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef FHIR_TOOLS_FHIRELEMENT_HXX
#define FHIR_TOOLS_FHIRELEMENT_HXX

#include "shared/util/Version.hxx"
#include "fhirtools/model/DecimalType.hxx"
#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/FhirConstraint.hxx"
#include "fhirtools/repository/FhirSlicing.hxx"
#include "fhirtools/repository/FhirVersion.hxx"

#include <iosfwd>
#include <memory>
#include <optional>
#include <set>
#include <variant>
#include <vector>

namespace fhirtools {
class FhirStructureDefinition;
class FhirValue;
class ValidationResults;

/// @brief stores information of an Element in the StructureDefinition
///
/// XPath: StructureDefinition/snapshot/element
class FhirElement
{
public:
    class Builder;
    enum class Representation
    {
        xmlAttr,
        xmlText,
        typeAttr,
        cdaText,
        xhtml,
        element,
    };
    struct Cardinality {
        uint32_t min = 0;
        std::optional<uint32_t> max = std::nullopt;
        bool isConstraint(bool isArray);
        auto operator<=>(const Cardinality&) const = default;
        ValidationResults check(uint32_t count, std::string_view elementFullPath, const FhirStructureDefinition*,
                                const std::shared_ptr<const Element>& element,
                                const std::string& typeId) const;
        bool restricts(const Cardinality&) const;
    private:
        bool isArrayConstraint() const;
        bool isFieldConstraint() const;
    };
    enum class BindingStrength
    {
        required,
        extensible,
        preferred,
        example
    };
    struct Binding
    {
        Binding() noexcept = default;
        BindingStrength strength{};
        DefinitionKey key{"unset-binding", std::nullopt};
    };

    FhirElement();
    virtual ~FhirElement();


    const std::string& name() const { return mName; }
    std::string_view fieldName() const;
    std::string_view originalFieldName() const;
    const std::string& originalName() const { return mOriginalName.empty()?mName:mOriginalName; }
    const std::string& typeId() const { return isRoot()?mName: mTypeId; }
    std::list<std::string> profiles() const {return mProfiles; }
    const std::string& contentReference() const { return mContentReference; }
    Representation representation() const { return mRepresentation; }
    const std::vector<FhirConstraint>& getConstraints() const { return mConstraints; }
    bool isArray() const { return mIsArray; }
    [[nodiscard]] bool isBackbone() const { return mIsBackbone; }
    bool isRoot() const { return mIsRoot; }

    const std::optional<FhirSlicing>& slicing() const;
    bool hasSlices() const { return mSlicing.has_value() && !mSlicing->slices().empty();}
    const std::shared_ptr<const FhirValue>& pattern() const { return mPattern; }
    const std::shared_ptr<const FhirValue>& fixed() const { return mFixed; }

    Cardinality cardinality() const { return mCardinality; }

    bool hasBinding() const {return mBinding.has_value();}
    const Binding& binding() const {return *mBinding;}

    bool hasMaxLength() const;
    size_t maxLength() const;

    std::optional<int> minValueInteger() const;
    std::optional<int> maxValueInteger() const;
    std::optional<DecimalType> minValueDecimal() const;
    std::optional<DecimalType> maxValueDecimal() const;

    const std::set<std::string>& referenceTargetProfiles() const;

    // immutable:
    FhirElement(FhirElement&&) = default;
    FhirElement(const FhirElement&) = default;
    FhirElement& operator=(FhirElement&&) = delete;
    FhirElement& operator=(const FhirElement&) = delete;

private:
    bool mIsRoot = false;
    std::string mName;
    std::string mOriginalName;
    std::string mTypeId;
    std::list<std::string> mProfiles;
    std::string mContentReference;
    Representation mRepresentation = Representation::element;
    std::vector<FhirConstraint> mConstraints;
    bool mIsArray = false;
    bool mIsBackbone = false;
    std::optional<FhirSlicing> mSlicing;
    std::shared_ptr<const FhirValue> mPattern;
    std::shared_ptr<const FhirValue> mFixed;
    Cardinality mCardinality;
    std::optional<Binding> mBinding;
    std::optional<size_t> mMaxLength;
    using MinMaxValueType = std::variant<int, DecimalType>;
    std::optional<MinMaxValueType> mMinValue;
    std::optional<MinMaxValueType> mMaxValue;
    std::set<std::string> mReferenceTargetProfiles;
};

std::ostream& operator << (std::ostream&, const FhirElement::Cardinality&);
std::string to_string(const FhirElement::Cardinality&);

class FhirElement::Builder
{
public:
    Builder();

    explicit Builder(FhirElement elementTemplate);

    Builder& isRoot(bool newIsRoot);

    Builder& name(std::string name_);

    Builder& originalName(std::string orig);

    Builder& typeId(std::string type_);

    Builder& addProfile(std::string profile);

    Builder& setProfiles(std::list<std::string> profiles);

    Builder& contentReference(std::string contentReference_);

    Builder& representation(Representation representation_);

    Builder& isArray(bool isArray_);

    Builder& isBackbone(bool isBackbone_);

    Builder& addConstraint(FhirConstraint key);

    Builder& slicing(FhirSlicing&&);

    Builder& pattern(std::shared_ptr<const FhirValue> value);

    Builder& fixed(std::shared_ptr<const FhirValue> value);

    Builder& cardinalityMin(uint32_t);

    Builder& cardinalityMax(std::optional<uint32_t>);

    Builder& bindingStrength(const std::string& strength);

    Builder& bindingValueSet(const std::string& canonical);

    Builder& maxLength(size_t maxLength);

    Builder& minValueInteger(int minValue);

    Builder& maxValueInteger(int maxValue);

    Builder& minValueDecimal(const std::string& decimalAsString);

    Builder& maxValueDecimal(const std::string& decimalAsString);

    Builder& addTargetProfile(std::string&& targetProfile);

    std::shared_ptr<const FhirElement> getAndReset();

    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;
    Builder(Builder&&) = default;
    Builder& operator=(Builder&&) = default;
private:
    std::shared_ptr<FhirElement> mFhirElement;
};

std::ostream& operator<<(std::ostream&, const FhirElement&);

FhirElement::Representation stringToRepresentation(const std::string_view& str);
const std::string& to_string(FhirElement::Representation);
std::ostream& operator<<(std::ostream&, FhirElement::Representation);

}

#endif// FHIR_TOOLS_FHIRELEMENT_HXX
