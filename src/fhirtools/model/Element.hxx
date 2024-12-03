/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef FHIR_TOOLS_SRC_FHIR_PATH_MODEL_ELEMENT_HXX
#define FHIR_TOOLS_SRC_FHIR_PATH_MODEL_ELEMENT_HXX


#include "fhirtools/model/DateTime.hxx"
#include "fhirtools/model/DecimalType.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/typemodel/ProfiledElementTypeInfo.hxx"
#include "fhirtools/validator/ValidationResult.hxx"

#include <magic_enum/magic_enum.hpp>
#include <compare>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace fhirtools
{
class FhirCodeSystem;

/// @brief This is the FHIR-validators internal data model interface.
/// A implementation for the ERP-Model (based on rapidjson) exists under erp/ErpElement.xxx
class Element : public std::enable_shared_from_this<Element>
{
public:
    class QuantityType;
    enum class Type
    {
        Integer,
        Decimal,
        String,
        Boolean,
        Date,
        DateTime,
        Time,
        Quantity,
        Structured
    };
    struct IsContextElementTag {
    };
    // http://www.hl7.org/fhir/http.html#general
    struct Identity {
        struct Scheme : std::string {
            using std::string::string;
            bool isHttpLike() const;
            // http and https are equal
            std::weak_ordering operator<=>(const Scheme&) const;
            bool operator==(const Scheme&) const;
        };

        bool empty() const;
        std::string url() const;

        std::optional<Scheme> scheme{};
        std::string pathOrId{};
        std::optional<std::string> containedId{};
        // NOLINTNEXTLINE(hicpp-use-nullptr,modernize-use-nullptr)
        std::weak_ordering operator<=>(const Identity&) const = default;
        bool operator==(const Identity&) const;
    };

    struct IdentityAndResult {
        static IdentityAndResult fromReferenceString(std::string_view referenceString,
                                                     std::string_view elementFullPath);
        Identity identity{};
        ValidationResults result{};

        auto operator<=>(const IdentityAndResult&) const = delete;
        auto operator==(const IdentityAndResult&) const = delete;
    };

    Element(const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository,
            std::weak_ptr<const Element> parent, ProfiledElementTypeInfo mDefinitionPointer);
    Element(const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository,
            std::weak_ptr<const Element> parent, const std::string& elementId);
    virtual ~Element() = default;

    void setIsContextElement(const std::shared_ptr<IsContextElementTag>& tag) const;
    [[nodiscard]] bool isContextElement() const;

    [[nodiscard]] Type type() const;
    [[nodiscard]] std::shared_ptr<const Element> parent() const;
    [[nodiscard]] virtual std::string resourceType() const = 0;
    [[nodiscard]] virtual std::vector<std::string_view> profiles() const = 0;
    [[nodiscard]] virtual bool hasValue() const = 0;
    [[nodiscard]] bool isResource() const;
    [[nodiscard]] bool isContainerResource() const;
    [[nodiscard]] bool isDataAbsent() const;

    [[nodiscard]] virtual std::string asRaw() const = 0;

    // Explicit conversion functions:
    [[nodiscard]] virtual int32_t asInt() const = 0;
    [[nodiscard]] virtual DecimalType asDecimal() const = 0;
    [[nodiscard]] virtual bool asBool() const = 0;
    [[nodiscard]] virtual std::string asString() const = 0;
    [[nodiscard]] virtual Date asDate() const = 0;
    [[nodiscard]] virtual Time asTime() const = 0;
    [[nodiscard]] virtual DateTime asDateTime() const = 0;
    [[nodiscard]] virtual QuantityType asQuantity() const = 0;

    [[nodiscard]] virtual bool hasSubElement(const std::string& name) const = 0;
    [[nodiscard]] virtual std::vector<std::string> subElementNames() const = 0;
    [[nodiscard]] virtual std::vector<std::shared_ptr<const Element>> subElements(const std::string& name) const = 0;
    [[nodiscard]] std::shared_ptr<const Element> resourceRoot() const;
    [[nodiscard]] std::shared_ptr<const Element> containerResource() const;
    [[nodiscard]] std::shared_ptr<const Element> containingBundle() const;
    [[nodiscard]] std::shared_ptr<const Element> parentResource() const;
    [[nodiscard]] std::shared_ptr<const Element> documentRoot() const;

    [[nodiscard]] static Type GetElementType(const FhirStructureRepository* fhirStructureRepository,
                                             const ProfiledElementTypeInfo& definitionPointer);
    [[nodiscard]] static Type GetElementType(const FhirStructureRepository* fhirStructureRepository,
                                             const FhirStructureDefinition* structureDefinition,
                                             const std::string& elementId);
    [[nodiscard]] const std::shared_ptr<const fhirtools::FhirStructureRepository>& getFhirStructureRepository() const;
    [[nodiscard]] const FhirStructureDefinition* getStructureDefinition() const;
    [[nodiscard]] const ProfiledElementTypeInfo& definitionPointer() const;

    [[nodiscard]] IdentityAndResult resourceIdentity(std::string_view elementFullPath,
                                                     bool allowResourceId = true) const;
    [[nodiscard]] IdentityAndResult referenceTargetIdentity(std::string_view elementFullPath) const;

    [[nodiscard]] std::tuple<std::shared_ptr<const Element>, ValidationResults>
    resolveReference(std::string_view elementFullPath) const;
    [[nodiscard]] std::tuple<std::shared_ptr<const Element>, ValidationResults>
    resolveReference(const Identity& reference, std::string_view elementFullPath) const;

    // like operator<=>, but can return {} in special cases, see http://hl7.org/fhirpath/#equality
    // and http://hl7.org/fhirpath/#comparison
    [[nodiscard]] std::optional<std::strong_ordering> compareTo(const Element& rhs) const;
    [[nodiscard]] std::optional<bool> equals(const Element& rhs) const;
    [[nodiscard]] bool matches(const Element& pattern) const;

    size_t subElementLevel() const;

    std::ostream& json(std::ostream&) const;

protected:
    // the element, that contains the current resource
    [[nodiscard]] std::shared_ptr<const Element> resourceRootParent() const;
    [[nodiscard]] std::tuple<std::shared_ptr<const Element>, ValidationResults>
    resolveContainedReference(std::string_view containedId) const;
    [[nodiscard]] std::tuple<std::shared_ptr<const Element>, ValidationResults>
    resolveUrlReference(const Identity& urlIdentity, std::string_view elementFullPath) const;
    [[nodiscard]] std::tuple<std::shared_ptr<const Element>, ValidationResults>
    resolveBundleReference(std::string_view fullUrl, std::string_view elementFullPath) const;
    [[nodiscard]] std::tuple<std::shared_ptr<const Element>, ValidationResults>
    resolveGenericReference(const Identity& urlIdentity, std::string_view elementFullPath) const;

    std::shared_ptr<const fhirtools::FhirStructureRepository> mFhirStructureRepository;
    const ProfiledElementTypeInfo mDefinitionPointer;
    std::weak_ptr<const Element> mParent;
    mutable size_t mSubElementLevel;

private:
    [[nodiscard]] IdentityAndResult bundledResourceIdentity(std::string_view elementFullPath) const;
    [[nodiscard]] IdentityAndResult metaSourceIdentity(std::string_view elementFullPath) const;
    [[nodiscard]] IdentityAndResult containedIdentity(bool allowResourceId, std::string_view elementFullPath) const;
    [[nodiscard]] IdentityAndResult resourceTypeIdIdentity() const;

    [[nodiscard]] IdentityAndResult referenceTargetIdentity(IdentityAndResult reference,
                                                            std::string_view elementFullPath) const;
    [[nodiscard]] IdentityAndResult
    relativeReferenceTargetIdentity(IdentityAndResult&& reference,
                                    const std::shared_ptr<const Element>& currentResoureRoot,
                                    std::string_view elementFullPath) const;
    Type mType;
    mutable std::weak_ptr<IsContextElementTag> mIsContextElementTag;
};

class Element::QuantityType
{
public:
    QuantityType(DecimalType value, const std::string_view& unit);
    explicit QuantityType(const std::string_view& valueAndUnit);

    [[nodiscard]] const DecimalType& value() const
    {
        return mValue;
    }
    [[nodiscard]] const std::string& unit() const
    {
        return mUnit;
    }

    [[nodiscard]] std::optional<std::strong_ordering> compareTo(const QuantityType& rhs) const;
    [[nodiscard]] std::optional<bool> equals(const QuantityType& rhs) const;

private:
    QuantityType convertToUnit(const std::string& unit) const;
    bool convertibleToUnit(const std::string& unit) const;

    DecimalType mValue;
    std::string mUnit;
};


/// @brief Implementation of the Element interface for the 'primitive' types
///        boolean, string, integer, decimal, date-time, date and quantity
class PrimitiveElement : public Element
{
public:
    using ValueType = std::variant<int32_t, DecimalType, bool, std::string, Date, DateTime, Time, QuantityType>;
    explicit PrimitiveElement(const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository,
                              Type type, ValueType&& value);
    [[nodiscard]] std::string resourceType() const override;
    [[nodiscard]] std::vector<std::string_view> profiles() const override;

    [[nodiscard]] std::string asRaw() const override;

    [[nodiscard]] int32_t asInt() const override;
    [[nodiscard]] std::string asString() const override;
    [[nodiscard]] DecimalType asDecimal() const override;
    [[nodiscard]] bool asBool() const override;
    [[nodiscard]] Date asDate() const override;
    [[nodiscard]] Time asTime() const override;
    [[nodiscard]] DateTime asDateTime() const override;
    [[nodiscard]] QuantityType asQuantity() const override;

    [[nodiscard]] bool hasSubElement(const std::string& name) const override;
    [[nodiscard]] std::vector<std::string> subElementNames() const override;
    [[nodiscard]] bool hasValue() const final;
    [[nodiscard]] std::vector<std::shared_ptr<const Element>> subElements(const std::string& name) const override;

private:
    static const FhirStructureDefinition* GetStructureDefinition(const FhirStructureRepository* fhirStructureRepository,
                                                                 Type type);
    std::string decimalAsString(const DecimalType& dec) const;

    ValueType mValue;
};

[[nodiscard]] bool isImplicitConvertible(Element::Type from, Element::Type to);

std::ostream& operator<<(std::ostream& os, Element::Type type);
std::ostream& operator<<(std::ostream& os, const Element& element);

std::ostream& operator<<(std::ostream& os, const Element::Identity&);
std::string to_string(const Element::Identity&);
}
#endif//FHIR_TOOLS_SRC_FHIR_PATH_MODEL_ELEMENT_HXX
