// (C) Copyright IBM Deutschland GmbH 2022
// (C) Copyright IBM Corp. 2022

#ifndef FHIRPATH_VALUEELEMENT_H
#define FHIRPATH_VALUEELEMENT_H

#include <map>
#include <memory>

#include "fhirtools/model/Element.hxx"

namespace fhirtools
{
class FhirValue;
class ProfiledElementTypeInfo;
class FhirStructureRepository;

/// @brief implementation of the Element interface for nodes of the structure definition containing
///        fixed values or pattern values.
class ValueElement : public Element
{
public:
    ValueElement(const FhirStructureRepository* repo, std::shared_ptr<const FhirValue> value,
                 std::weak_ptr<const Element> parent = {});
    ValueElement(const FhirStructureRepository* repo, std::weak_ptr<const Element> parent,
                 std::shared_ptr<const FhirValue> value, ProfiledElementTypeInfo defPtr);

    [[nodiscard]] std::vector<std::shared_ptr<const Element>> subElements(const std::string& name) const override;
    [[nodiscard]] bool hasValue() const override;
    [[nodiscard]] bool hasSubElement(const std::string& name) const override;
    [[nodiscard]] std::vector<std::string> subElementNames() const override;
    [[nodiscard]] Element::QuantityType asQuantity() const override;
    [[nodiscard]] DateTime asDateTime() const override;
    [[nodiscard]] Time asTime() const override;
    [[nodiscard]] Date asDate() const override;
    [[nodiscard]] std::string asString() const override;
    [[nodiscard]] bool asBool() const override;
    [[nodiscard]] DecimalType asDecimal() const override;
    [[nodiscard]] int32_t asInt() const override;
    [[nodiscard]] std::vector<std::string_view> profiles() const override;
    [[nodiscard]] std::string resourceType() const override;

    [[nodiscard]] const std::shared_ptr<const FhirValue>& value() const
    {
        return mValue;
    };

private:
    [[nodiscard]] std::shared_ptr<PrimitiveElement> asPrimitiveElement() const;
    [[nodiscard]] std::shared_ptr<PrimitiveElement> asPrimitiveElement(Type type, const std::string& value,
                                                                       const std::string& unit) const;
    std::shared_ptr<const FhirValue> mValue;
};

class FhirValue
{
public:
    [[nodiscard]] std::string elementId() const;
    [[nodiscard]] const std::string& name() const;
    [[nodiscard]] std::shared_ptr<FhirValue> parent() const;
    [[nodiscard]] const std::multimap<std::string, std::shared_ptr<const FhirValue>>& children() const;
    [[nodiscard]] const std::map<std::string, std::string>& attributes() const;

    class Builder;

private:
    std::string mName;
    std::weak_ptr<FhirValue> mParent;
    std::multimap<std::string, std::shared_ptr<const FhirValue>> mChildren;
    std::map<std::string, std::string> mAttributes;
};

class FhirValue::Builder
{
public:
    Builder(std::string name, Builder& parentBuilder);
    Builder(std::string name);
    Builder& addAttribute(std::string name, std::string value);
    operator std::shared_ptr<const FhirValue>() &&;

private:
    std::shared_ptr<FhirValue> mFhirValue;
};
}

#endif// FHIRPATH_VALUEELEMENT_H
