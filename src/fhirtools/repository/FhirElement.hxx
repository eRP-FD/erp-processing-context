/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef FHIR_TOOLS_FHIRELEMENT_HXX
#define FHIR_TOOLS_FHIRELEMENT_HXX

#include "erp/util/Version.hxx"

#include <iosfwd>
#include <memory>
#include <optional>
#include <vector>

#include "fhirtools/repository/FhirConstraint.hxx"
#include "fhirtools/repository/FhirSlicing.hxx"

namespace fhirtools {
class FhirStructureDefinition;
class FhirValue;
class ValidationResultList;

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
        ValidationResultList check(uint32_t count, std::string_view elementFullPath,
                                   const FhirStructureDefinition*) const;
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
        std::string valueSetUrl;
        std::optional<Version> valueSetVersion;
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
    /// element is a root element
    /// root elements have no \<type\>; element and carry the name of their Structure's typeId (StructureDefinition/type\@value)
    bool isRoot() const
    { return mTypeId.empty() && (mName.find('.') == std::string::npos); }

    const std::optional<FhirSlicing>& slicing() const;
    bool hasSlices() const { return mSlicing.has_value() && !mSlicing->slices().empty();}
    const std::shared_ptr<const FhirValue>& pattern() const { return mPattern; }
    const std::shared_ptr<const FhirValue>& fixed() const { return mFixed; }

    Cardinality cardinality() const { return mCardinality; }

    bool hasBinding() const {return mBinding.has_value();}
    const Binding& binding() const {return *mBinding;}

    // immutable:
    FhirElement(FhirElement&&) = default;
    FhirElement(const FhirElement&) = default;
    FhirElement& operator=(FhirElement&&) = delete;
    FhirElement& operator=(const FhirElement&) = delete;

private:
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
};

std::ostream& operator << (std::ostream&, const FhirElement::Cardinality&);
std::string to_string(const FhirElement::Cardinality&);

class FhirElement::Builder
{
public:
    Builder();

    explicit Builder(FhirElement elementTemplate);

    Builder& name(std::string name_);

    Builder& originalName(std::string orig);

    Builder& typeId(std::string type_);

    Builder& addProfile(std::string profile);

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