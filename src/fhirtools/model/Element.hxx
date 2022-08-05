/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef FHIR_TOOLS_SRC_FHIR_PATH_MODEL_ELEMENT_HXX
#define FHIR_TOOLS_SRC_FHIR_PATH_MODEL_ELEMENT_HXX


#include "fhirtools/model/Timestamp.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/typemodel/ProfiledElementTypeInfo.hxx"

#include <boost/multiprecision/mpfr.hpp>
#include <magic_enum.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace fhirtools {
namespace bmp = boost::multiprecision;

/// @brief This is the FHIR-validators internal data model interface.
/// A implementation for the ERP-Model (based on rapidjson) exists under erp/ErpElement.xxx
class Element : public std::enable_shared_from_this<Element>
{
public:
    // A fixed-point number type with precision 10^-8 as specified in http://hl7.org/fhirpath/#decimal
    using DecimalType = bmp::number<bmp::mpfr_float_backend<8, bmp::allocate_stack>>;
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
    struct IsContextElementTag{};
    Element(const FhirStructureRepository* fhirStructureRepository, std::weak_ptr<const Element> parent,
            ProfiledElementTypeInfo mDefinitionPointer);
    Element(const FhirStructureRepository* fhirStructureRepository, std::weak_ptr<const Element> parent,
            const std::string& elementId);
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

    // Explicit conversion functions:
    [[nodiscard]] virtual int32_t asInt() const = 0;
    [[nodiscard]] virtual DecimalType asDecimal() const = 0;
    [[nodiscard]] virtual bool asBool() const = 0;
    [[nodiscard]] virtual std::string asString() const = 0;
    [[nodiscard]] virtual Timestamp asDate() const = 0;
    [[nodiscard]] virtual Timestamp asTime() const = 0;
    [[nodiscard]] virtual Timestamp asDateTime() const = 0;
    [[nodiscard]] virtual QuantityType asQuantity() const = 0;

    [[nodiscard]] virtual bool hasSubElement(const std::string& name) const = 0;
    [[nodiscard]] virtual std::vector<std::string> subElementNames() const = 0;
    [[nodiscard]] virtual std::vector<std::shared_ptr<const Element>> subElements(const std::string& name) const = 0;

    [[nodiscard]] static Type GetElementType(const FhirStructureRepository* fhirStructureRepository,
                                             const ProfiledElementTypeInfo& definitionPointer);
    [[nodiscard]] static Type GetElementType(const FhirStructureRepository* fhirStructureRepository,
                                             const FhirStructureDefinition* structureDefinition,
                                             const std::string& elementId);
    [[nodiscard]] const FhirStructureRepository* getFhirStructureRepository() const;
    [[nodiscard]] const FhirStructureDefinition* getStructureDefinition() const;
    [[nodiscard]] const ProfiledElementTypeInfo& definitionPointer() const;

    [[nodiscard]] std::strong_ordering operator<=>(const Element& rhs) const;
    [[nodiscard]] bool operator==(const Element& rhs) const;
    [[nodiscard]] bool matches(const Element& pattern) const;


    std::ostream& json(std::ostream&) const;

protected:
    const FhirStructureRepository* mFhirStructureRepository;
    const ProfiledElementTypeInfo mDefinitionPointer;
    std::weak_ptr<const Element> mParent;

private:
    Type mType;
    mutable std::weak_ptr<IsContextElementTag> mIsContextElementTag;
};

class Element::QuantityType
{
public:
    QuantityType(Element::DecimalType value, const std::string_view& unit);
    explicit QuantityType(const std::string_view& valueAndUnit);

    [[nodiscard]] const DecimalType& value() const
    {
        return mValue;
    }
    [[nodiscard]] const std::string& unit() const
    {
        return mUnit;
    }

    [[nodiscard]] std::strong_ordering operator<=>(const QuantityType& rhs) const;
    [[nodiscard]] bool operator==(const QuantityType& rhs) const;

private:
    QuantityType convertToUnit(const std::string& unit) const;
    bool convertibleToUnit(const std::string& unit) const;

    Element::DecimalType mValue;
    std::string mUnit;
};


/// @brief Implementation of the Element interface for the 'primitive' types
///        boolean, string, integer, decimal, date-time, date and quantity
class PrimitiveElement : public Element
{
public:
    using ValueType = std::variant<int32_t, DecimalType, bool, std::string, Timestamp, QuantityType>;
    explicit PrimitiveElement(const FhirStructureRepository* fhirStructureRepository, Type type, ValueType&& value);
    [[nodiscard]] std::string resourceType() const override;
    [[nodiscard]] std::vector<std::string_view> profiles() const override;

    [[nodiscard]] int32_t asInt() const override;
    [[nodiscard]] std::string asString() const override;
    [[nodiscard]] DecimalType asDecimal() const override;
    [[nodiscard]] bool asBool() const override;
    [[nodiscard]] Timestamp asDate() const override;
    [[nodiscard]] Timestamp asTime() const override;
    [[nodiscard]] Timestamp asDateTime() const override;
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
}
#endif//FHIR_TOOLS_SRC_FHIR_PATH_MODEL_ELEMENT_HXX
