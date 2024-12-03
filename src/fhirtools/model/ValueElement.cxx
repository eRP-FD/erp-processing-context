// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
//
// non-exclusively licensed to gematik GmbH

#include "fhirtools/model/ValueElement.hxx"
#include "fhirtools/FPExpect.hxx"

#include <charconv>

using fhirtools::Element;
using fhirtools::FhirValue;
using fhirtools::PrimitiveElement;
using fhirtools::ValueElement;

ValueElement::ValueElement(const std::shared_ptr<const fhirtools::FhirStructureRepository>& repo,
                           std::shared_ptr<const FhirValue> value, std::weak_ptr<const Element> parent)
    : Element(repo, std::move(parent), value->elementId())
    , mValue(std::move(value))
{
}

ValueElement::ValueElement(const std::shared_ptr<const fhirtools::FhirStructureRepository>& repo,
                           std::weak_ptr<const Element> parent, std::shared_ptr<const FhirValue> value,
                           ProfiledElementTypeInfo defPtr)
    : Element(repo, std::move(parent), std::move(defPtr))
    , mValue(std::move(value))
{
}


std::vector<std::shared_ptr<const Element>> ValueElement::subElements(const std::string& name) const
{
    auto attributes = mValue->attributes();
    auto valueIter = attributes.find(name);
    auto subDefPtr = mDefinitionPointer.subDefinitions(*mFhirStructureRepository, name);
    if (valueIter != attributes.end())
    {
        auto elementType = GetElementType(mFhirStructureRepository.get(), subDefPtr.back());
        return {asPrimitiveElement(elementType, valueIter->second, {})};
    }
    const auto& children = mValue->children();
    std::vector<std::shared_ptr<const Element>> result;
    result.reserve(mValue->children().size());
    for (auto child = children.lower_bound(name); child != children.end() && child->first == name; ++child)
    {
        result.emplace_back(std::make_shared<ValueElement>(mFhirStructureRepository, weak_from_this(), child->second,
                                                           subDefPtr.back()));
    }
    return result;
}

bool fhirtools::ValueElement::hasValue() const
{
    if (type() == Type::Structured)
    {
        return false;
    }
    return mValue->attributes().contains("value");
}

bool fhirtools::ValueElement::hasSubElement(const std::string& name) const
{
    return mValue->children().contains(name) || mValue->attributes().contains(name);
}

std::vector<std::string> ValueElement::subElementNames() const
{
    bool isPrimitive = (mDefinitionPointer.profile()->kind() == FhirStructureDefinition::Kind::primitiveType);
    std::vector<std::string> result;
    result.reserve(mValue->children().size() + mValue->attributes().size());
    for (const auto& attr : mValue->attributes())
    {
        if (isPrimitive && attr.first == "value")
        {
            continue;
        }
        result.emplace_back(attr.first);
    }
    for (const auto& child : mValue->children())
    {
        if (result.empty() || result.back() != child.first)
        {
            result.emplace_back(child.first);
        }
    }
    return result;
}

std::string fhirtools::ValueElement::asRaw() const
{
    const auto& attributeMap = mValue->attributes();
    auto valueIter = attributeMap.find("value");
    Expect(valueIter != attributeMap.end(), "Missing value attribute: " + mValue->elementId());
    return valueIter->second;
}


Element::QuantityType ValueElement::asQuantity() const
{
    return asPrimitiveElement()->asQuantity();
}

fhirtools::DateTime ValueElement::asDateTime() const
{
    return asPrimitiveElement()->asDateTime();
}

fhirtools::Time ValueElement::asTime() const
{
    return asPrimitiveElement()->asTime();
}

fhirtools::Date ValueElement::asDate() const
{
    return asPrimitiveElement()->asDate();
}

std::string ValueElement::asString() const
{
    return asPrimitiveElement()->asString();
}

bool ValueElement::asBool() const
{
    return asPrimitiveElement()->asBool();
}

fhirtools::DecimalType ValueElement::asDecimal() const
{
    return asPrimitiveElement()->asDecimal();
}

int32_t ValueElement::asInt() const
{
    return asPrimitiveElement()->asInt();
}

std::vector<std::string_view> ValueElement::profiles() const
{
    const auto& children = mValue->children();
    auto meta = children.find("meta");
    if (meta == children.end())
    {
        return {};
    }
    const auto& metaChildren = meta->second->children();
    std::vector<std::string_view> result;
    result.reserve(1);
    for (auto prof = metaChildren.lower_bound("profile"); prof != metaChildren.end() && prof->first == "profile";
         ++prof)
    {
        const auto& profAttributes = prof->second->attributes();
        auto value = profAttributes.find("value");
        if (value != profAttributes.end())
        {
            result.emplace_back(value->second);
        }
    }
    return result;
}

std::string ValueElement::resourceType() const
{
    const auto& attributes = mValue->attributes();
    auto resType = attributes.find("resourceType");
    return (resType != attributes.end()) ? resType->second : std::string{};
}

std::shared_ptr<PrimitiveElement> ValueElement::asPrimitiveElement() const
{
    const auto& attributeMap = mValue->attributes();
    const auto& value = asRaw();
    auto elementType = type();
    auto unitIter = attributeMap.find("unit");
    std::string unit = unitIter != attributeMap.end() ? unitIter->second : std::string{};
    return asPrimitiveElement(elementType, value, unit);
}

std::shared_ptr<PrimitiveElement> ValueElement::asPrimitiveElement(Element::Type elementType, const std::string& value,
                                                                   const std::string& unit) const
{
    switch (elementType)
    {
        case Type::Integer: {
            const char* const begin = value.data();
            const char* const end = begin + value.size();
            int32_t int32value{};
            auto res = std::from_chars<int32_t>(begin, end, int32value);
            Expect(res.ec == std::errc{}, std::make_error_code(res.ec).message() + ": " + value);
            Expect(res.ptr == end, "Numeric value not fully converted: " + value);
            return std::make_shared<PrimitiveElement>(mFhirStructureRepository, elementType, int32value);
        }
        case Type::Decimal:
            return std::make_shared<PrimitiveElement>(mFhirStructureRepository, elementType, DecimalType(value));
        case Type::String:
            return std::make_shared<PrimitiveElement>(mFhirStructureRepository, elementType, value);
        case Type::Boolean:
            return std::make_shared<PrimitiveElement>(mFhirStructureRepository, elementType, value == "true");
        case Type::Date:
            return std::make_shared<PrimitiveElement>(mFhirStructureRepository, elementType, Date(value));
        case Type::DateTime:
            return {std::make_shared<PrimitiveElement>(mFhirStructureRepository, elementType, DateTime(value))};
        case Type::Time:
            return std::make_shared<PrimitiveElement>(mFhirStructureRepository, elementType, Time(value));
        case Type::Structured:
            break;
        case Type::Quantity: {
            return std::make_shared<PrimitiveElement>(mFhirStructureRepository, type(),
                                                      QuantityType(DecimalType{value}, unit));
        }
    }
    Fail("not convertible to primitive");
}

//NOLINTNEXTLINE(misc-no-recursion)
std::string FhirValue::elementId() const
{
    auto parent = mParent.lock();
    if (parent)
    {
        return parent->elementId() + '.' + mName;
    }
    return mName;
}

const std::string& FhirValue::name() const
{
    return mName;
}

std::shared_ptr<FhirValue> FhirValue::parent() const
{
    auto result = mParent.lock();
    Expect(! mParent.expired(), "Attempt to access expired parent: " + mName);
    return result;
}

const std::multimap<std::string, std::shared_ptr<const FhirValue>>& FhirValue::children() const
{
    return mChildren;
}

const std::map<std::string, std::string>& FhirValue::attributes() const
{
    return mAttributes;
}

FhirValue::Builder::Builder(std::string name, FhirValue::Builder& parentBuilder)
    : Builder{std::move(name)}
{
    mFhirValue->mParent = parentBuilder.mFhirValue;
}

FhirValue::Builder::Builder(std::string name)
    : mFhirValue{std::make_shared<FhirValue>()}
{
    mFhirValue->mName = std::move(name);
}

FhirValue::Builder& FhirValue::Builder::addAttribute(std::string name, std::string value)
{
    mFhirValue->mAttributes.emplace(std::move(name), std::move(value));
    return *this;
}

FhirValue::Builder::operator std::shared_ptr<const FhirValue>() &&
{
    auto parent = mFhirValue->mParent.lock();
    if (parent)
    {
        parent->mChildren.emplace(mFhirValue->mName, mFhirValue);
    }
    return std::move(mFhirValue);
}
