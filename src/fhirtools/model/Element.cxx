/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "fhirtools/model/Element.hxx"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <utility>
#include <variant>

#include "fhirtools/FPExpect.hxx"
#include "fhirtools/model/Collection.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"

using fhirtools::Element;
using fhirtools::PrimitiveElement;

Element::Element(const FhirStructureRepository* fhirStructureRepository, std::weak_ptr<const Element> parent,
                 ProfiledElementTypeInfo definitionPointer)
    : mFhirStructureRepository(fhirStructureRepository)
    , mDefinitionPointer(std::move(definitionPointer))
    , mParent(std::move(parent))
    , mType(GetElementType(fhirStructureRepository, mDefinitionPointer))
{
}

Element::Element(const FhirStructureRepository* fhirStructureRepository, std::weak_ptr<const Element> parent,
                 const std::string& elementId)
    : mFhirStructureRepository(fhirStructureRepository)
    , mDefinitionPointer(fhirStructureRepository, elementId)
    , mParent(std::move(parent))
    , mType(GetElementType(fhirStructureRepository, mDefinitionPointer))
{
}


Element::Type Element::GetElementType(const FhirStructureRepository* fhirStructureRepository,
                                      const ProfiledElementTypeInfo& definitionPointer)
{
    auto GetElementType = [](const FhirStructureDefinition* structureDefinition) {
        switch (structureDefinition->kind())
        {
            case FhirStructureDefinition::Kind::primitiveType:
            case FhirStructureDefinition::Kind::resource:
            case FhirStructureDefinition::Kind::logical:
            case FhirStructureDefinition::Kind::slice:
                return Type::Structured;
            case FhirStructureDefinition::Kind::complexType:
                if (structureDefinition->typeId() == "Quantity")
                {
                    return Type::Quantity;
                }
                else
                {
                    return Type::Structured;
                }
                break;
            case FhirStructureDefinition::Kind::systemBoolean:
                return Type::Boolean;
            case FhirStructureDefinition::Kind::systemString:
                return Type::String;
            case FhirStructureDefinition::Kind::systemDouble:
                return Type::Decimal;
            case FhirStructureDefinition::Kind::systemInteger:
                return Type::Integer;
            case FhirStructureDefinition::Kind::systemDate:
                return Type::Date;
            case FhirStructureDefinition::Kind::systemTime:
                return Type::Time;
            case FhirStructureDefinition::Kind::systemDateTime:
                return Type::DateTime;
        }
        FPFail("unhandled kind " + std::to_string(uintmax_t(structureDefinition->kind())));
    };

    const auto& element = definitionPointer.element();
    if (element && ! definitionPointer.profile()->isSystemType())
    {
        const auto* typeStructure = fhirStructureRepository->findTypeById(element->typeId());
        if (element && typeStructure->kind() == FhirStructureDefinition::Kind::primitiveType)
        {
            auto value = typeStructure->findElement(element->typeId() + ".value");
            FPExpect(value, "value element not found for primitive type " + element->typeId());
            const auto* valueTypeStructure = fhirStructureRepository->findTypeById(value->typeId());
            FPExpect(valueTypeStructure, "structure definition not found for " + value->typeId());
            return GetElementType(valueTypeStructure);
        }
        return GetElementType(typeStructure);
    }
    return GetElementType(definitionPointer.profile());
}

Element::Type Element::GetElementType(const FhirStructureRepository* fhirStructureRepository,
                                      const FhirStructureDefinition* structureDefinition, const std::string& elementId)
{
    auto fhirElement = structureDefinition->findElement(elementId);
    FPExpect(fhirElement != nullptr,
             structureDefinition->url() + '|' + structureDefinition->version() + " no such element: " + elementId);
    return GetElementType(fhirStructureRepository, ProfiledElementTypeInfo{structureDefinition, fhirElement});
}


const fhirtools::FhirStructureRepository* Element::getFhirStructureRepository() const
{
    return mFhirStructureRepository;
}

const fhirtools::FhirStructureDefinition* Element::getStructureDefinition() const
{
    return mDefinitionPointer.profile();
}

const fhirtools::ProfiledElementTypeInfo& Element::definitionPointer() const
{
    return mDefinitionPointer;
}

void Element::setIsContextElement(const std::shared_ptr<IsContextElementTag>& tag) const
{
    mIsContextElementTag = tag;
}

bool Element::isContextElement() const
{
    return mIsContextElementTag.lock() != nullptr;
}

Element::Type Element::type() const
{
    return mType;
}

std::shared_ptr<const Element> Element::parent() const
{
    return mParent.lock();
}

bool Element::isResource() const
{
    return mDefinitionPointer.isResource();
}

bool Element::isContainerResource() const
{
    return mDefinitionPointer.isResource() && hasSubElement("contained");
}

std::strong_ordering Element::operator<=>(const Element& rhs) const
{
    FPExpect(isImplicitConvertible(type(), rhs.type()), "incompatible operands for comparison");
    switch (rhs.type())
    {
        case Type::Integer:
            return asInt() <=> rhs.asInt();
        case Type::Decimal:
            if (asDecimal() < rhs.asDecimal())
            {
                return std::strong_ordering::less;
            }
            else if (asDecimal() > rhs.asDecimal())
            {
                return std::strong_ordering::greater;
            }
            else
            {
                return std::strong_ordering::equal;
            }
        case Type::String:
            return asString() <=> rhs.asString();
        case Type::Date:
            return asDate() <=> rhs.asDate();
        case Type::DateTime:
            return asDateTime() <=> rhs.asDateTime();
        case Type::Time:
            return asTime() <=> rhs.asTime();
        case Type::Quantity:
            return asQuantity() <=> rhs.asQuantity();
        case Type::Boolean:
            return asBool() <=> rhs.asBool();
        case Type::Structured:
            break;
    }
    FPFail("invalid type for comparison");
}

bool Element::operator==(const Element& rhs) const
{
    try
    {
        switch (rhs.type())
        {
            case Element::Type::Integer:
                return asInt() == rhs.asInt();
            case Element::Type::Decimal:
                return asDecimal() == rhs.asDecimal();
            case Element::Type::String:
                return asString() == rhs.asString();
            case Element::Type::Boolean:
                return asBool() == rhs.asBool();
            case Element::Type::Date:
                // fixme if necessary ERP-10543: does currently not implement the return of empty collection {} as described in
                //                     http://hl7.org/fhirpath/#datetime-equality
                return asDate() == rhs.asDate();
            case Element::Type::DateTime:
                return asDateTime() == rhs.asDateTime();
            case Element::Type::Time:
                return asTime() == rhs.asTime();
            case Element::Type::Quantity:
                return asQuantity() == rhs.asQuantity();
            case Element::Type::Structured: {
                const auto subElementsLhs = subElementNames();
                const auto subElementsRhs = rhs.subElementNames();
                if (subElementsLhs != subElementsRhs)
                {
                    return false;
                }
                return std::ranges::all_of(subElementsRhs, [&rhs, this](const auto& item) {
                    const Collection subElementLhs{subElements(item)};
                    const Collection subElementRhs{rhs.subElements(item)};
                    return subElementLhs == subElementRhs;
                });
            }
        }
    }
    catch (const std::exception&)
    {
    }
    return false;
}

//NOLINTNEXTLINE(misc-no-recursion)
bool Element::matches(const Element& pattern) const
{
    Expect3(pattern.type() == type(), "type mismatch", std::logic_error);
    if (type() == Type::Structured)
    {
        const auto& subNames = subElementNames();
        for (const auto& patternSubName : pattern.subElementNames())
        {
            if (std::ranges::find(subNames, patternSubName) == subNames.end())
            {
                return false;
            }
            for (const auto& patternSub : pattern.subElements(patternSubName))
            {
                //NOLINTNEXTLINE(misc-no-recursion)
                bool found = std::ranges::any_of(subElements(patternSubName), [&patternSub](const auto& sub) {
                    return sub->matches(*patternSub);
                });
                if (! found)
                {
                    return false;
                }
            }
        }
        return true;
    }
    return *this == pattern;
}

Element::QuantityType::QuantityType(Element::DecimalType value, const std::string_view& unit)
    : mValue(std::move(value))
    , mUnit(unit)
{
}

Element::QuantityType::QuantityType(const std::string_view& valueAndUnit)
{
    size_t idx = 0;
    mValue = std::stod(std::string(valueAndUnit), &idx);
    mUnit = boost::trim_copy(std::string(valueAndUnit.substr(idx)));
}

std::strong_ordering Element::QuantityType::operator<=>(const Element::QuantityType& rhs) const
{
    FPExpect(convertibleToUnit(rhs.mUnit), "incompatible quantity units for comparison");
    const auto convertedUnit = convertToUnit(rhs.mUnit);
    if (convertedUnit.mValue < rhs.mValue)
    {
        return std::strong_ordering::less;
    }
    else if (convertedUnit.mValue > rhs.mValue)
    {
        return std::strong_ordering::greater;
    }
    else
    {
        return std::strong_ordering::equal;
    }
}

bool Element::QuantityType::operator==(const Element::QuantityType& rhs) const
{
    try
    {
        return this->operator<=>(rhs) == std::strong_ordering::equal;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

Element::QuantityType Element::QuantityType::convertToUnit(const std::string& unit) const
{
    FPExpect(mUnit == unit, "unit " + mUnit + " not convertible to " + unit);
    return *this;
}

bool Element::QuantityType::convertibleToUnit(const std::string& unit) const
{
    return mUnit == unit;
}

PrimitiveElement::PrimitiveElement(const FhirStructureRepository* fhirStructureRepository, Type type, ValueType&& value)
    : Element(fhirStructureRepository, {}, ProfiledElementTypeInfo{GetStructureDefinition(fhirStructureRepository, type)})
    , mValue(value)
{
}

const fhirtools::FhirStructureDefinition*
PrimitiveElement::GetStructureDefinition(const FhirStructureRepository* fhirStructureRepository, Element::Type type)
{
    switch (type)
    {
        case Type::Integer:
            return fhirStructureRepository->systemTypeInteger();
        case Type::Decimal:
            return fhirStructureRepository->systemTypeDecimal();
        case Type::String:
            return fhirStructureRepository->systemTypeString();
        case Type::Boolean:
            return fhirStructureRepository->systemTypeBoolean();
        case Type::Date:
            return fhirStructureRepository->systemTypeDate();
        case Type::DateTime:
            return fhirStructureRepository->systemTypeDateTime();
        case Type::Time:
            return fhirStructureRepository->systemTypeTime();
        case Type::Quantity:
            return fhirStructureRepository->findTypeById("Quantity");
        case Type::Structured:
            break;
    }
    FPFail("unsupported type for PrimitiveElement");
}

int32_t PrimitiveElement::asInt() const
{
    switch (type())
    {
        case Type::Integer:
            return std::get<int32_t>(mValue);
        case Type::String: {
            const auto& asString = std::get<std::string>(mValue);
            size_t idx = 0;
            auto asInt = std::stoi(asString, &idx);
            if (idx == asString.size())
                return asInt;
            break;
        }
        case Type::Boolean:
            return std::get<bool>(mValue) ? 1 : 0;
        case Type::Decimal:
        case Type::Structured:
        case Type::Date:
        case Type::DateTime:
        case Type::Time:
        case Type::Quantity:
            break;
    }
    FPFail("not convertible to int");
}

bool fhirtools::PrimitiveElement::hasSubElement(const std::string&) const
{
    return false;
}

std::vector<std::string> PrimitiveElement::subElementNames() const
{
    return {};
}

bool fhirtools::PrimitiveElement::hasValue() const
{
    return true;
}

std::vector<fhirtools::Collection::value_type> PrimitiveElement::subElements(const std::string&) const
{
    return {};
}

std::string PrimitiveElement::resourceType() const
{
    return {};
}

std::vector<std::string_view> PrimitiveElement::profiles() const
{
    return {};
}

std::string PrimitiveElement::asString() const
{
    switch (type())
    {

        case Type::Integer:
            return std::to_string(std::get<int32_t>(mValue));
        case Type::Decimal:
            return decimalAsString(std::get<DecimalType>(mValue));
        case Type::String:
            return std::get<std::string>(mValue);
        case Type::Boolean:
            return std::get<bool>(mValue) ? "true" : "false";
        case Type::Structured:
            break;
        case Type::Date:
            // fixme if necessary ERP-10543:  for partial dates and times, the result will only be specified to the level of
            //                      precision in the value being converted.
            return std::get<Timestamp>(mValue).toXsDate();
        case Type::DateTime:
            return std::get<Timestamp>(mValue).toXsDateTime();
        case Type::Time:
            return std::get<Timestamp>(mValue).toXsTime();
        case Type::Quantity: {
            const auto& q = std::get<QuantityType>(mValue);
            return decimalAsString(q.value()) + " " + q.unit();
        }
    }
    FPFail("not convertible to string");
}

std::string PrimitiveElement::decimalAsString(const DecimalType& dec) const
{
    auto trimmed =
        boost::trim_right_copy_if(dec.str(8, std::ios_base::fixed | std::ios_base::showpoint), [](const auto& c) {
            return c == '0';
        });
    if (trimmed.back() == '.')
        trimmed += '0';
    return trimmed;
}

Element::DecimalType PrimitiveElement::asDecimal() const
{
    switch (type())
    {
        case Type::Decimal:
            return std::get<DecimalType>(mValue);
        case Type::Integer:
            return std::get<int32_t>(mValue);
        case Type::Boolean:
            return std::get<bool>(mValue) ? 1.0 : 0.0;
        case Type::String:
            return DecimalType(std::get<std::string>(mValue));
        case Type::Structured:
        case Type::Date:
        case Type::DateTime:
        case Type::Time:
        case Type::Quantity:
            break;
    }
    FPFail("not convertible to decimal");
}

bool PrimitiveElement::asBool() const
{
    switch (type())
    {
        case Type::Boolean:
            return std::get<bool>(mValue);
        case Type::Integer: {
            const auto& asInt = std::get<int32_t>(mValue);
            if (asInt == 0)
                return false;
            if (asInt == 1)
                return true;
            break;
        }
        case Type::Decimal: {
            const auto& asDecimal = std::get<DecimalType>(mValue);
            if (asDecimal == 0)
                return false;
            if (asDecimal == 1)
                return true;
            break;
        }
        case Type::String: {
            auto asString = boost::to_lower_copy(std::get<std::string>(mValue));
            if (asString == "true" || asString == "t" || asString == "yes" || asString == "y" || asString == "1" ||
                asString == "1.0")
                return true;
            if (asString == "false" || asString == "f" || asString == "no" || asString == "n" || asString == "0" ||
                asString == "0.0")
                return false;
            break;
        }
        case Type::Structured:
        case Type::Date:
        case Type::DateTime:
        case Type::Time:
        case Type::Quantity:
            break;
    }
    FPFail("not convertible to bool");
}

fhirtools::Timestamp PrimitiveElement::asDate() const
{
    switch (type())
    {
        case Type::Date:
            return std::get<Timestamp>(mValue);
        case Type::DateTime:
            return Timestamp::fromXsDate(std::get<Timestamp>(mValue).toXsDate());
        case Type::String:
            return Timestamp::fromFhirDateTime(std::get<std::string>(mValue));
        case Type::Integer:
        case Type::Decimal:
        case Type::Boolean:
        case Type::Time:
        case Type::Quantity:
        case Type::Structured:
            break;
    }
    FPFail("not convertible to Date");
}

fhirtools::Timestamp PrimitiveElement::asTime() const
{
    switch (type())
    {
        case Type::Time:
            return std::get<Timestamp>(mValue);
        case Type::String:
            return Timestamp::fromFhirPathTimeLiteral(std::get<std::string>(mValue));
        case Type::Integer:
        case Type::Decimal:
        case Type::Boolean:
        case Type::Date:
        case Type::DateTime:
        case Type::Quantity:
        case Type::Structured:
            break;
    }
    FPFail("not convertible to Time");
}

fhirtools::Timestamp PrimitiveElement::asDateTime() const
{
    switch (type())
    {
        case Type::Date:
        case Type::DateTime:
            return std::get<Timestamp>(mValue);
        case Type::String:
            return Timestamp::fromFhirDateTime(std::get<std::string>(mValue));
        case Type::Integer:
        case Type::Decimal:
        case Type::Boolean:
        case Type::Time:
        case Type::Quantity:
        case Type::Structured:
            break;
    }
    FPFail("not convertible to DateTime");
}

Element::QuantityType PrimitiveElement::asQuantity() const
{
    switch (type())
    {
        case Type::Integer:
            return QuantityType(std::get<int32_t>(mValue), "");
        case Type::Decimal:
            return QuantityType(std::get<DecimalType>(mValue), "");
        case Type::String:
            return QuantityType(std::get<std::string>(mValue));
        case Type::Boolean:
        case Type::Date:
        case Type::DateTime:
        case Type::Time:
            break;
        case Type::Quantity:
            return std::get<QuantityType>(mValue);
        case Type::Structured:
            break;
    }
    FPFail("not convertible to Quantity");
}


bool fhirtools::isImplicitConvertible(Element::Type from, Element::Type to)
{
    if (from == to)
    {
        return true;
    }
    switch (from)
    {
        case Element::Type::Integer:
            return to == Element::Type::Decimal || to == Element::Type::Quantity;
        case Element::Type::Decimal:
            return to == Element::Type::Quantity;
        case Element::Type::String:
        case Element::Type::Boolean:
        case Element::Type::Structured:
            break;
        case Element::Type::Date:
            return to == Element::Type::DateTime;
        case Element::Type::DateTime:
        case Element::Type::Time:
        case Element::Type::Quantity:
            break;
    }
    return false;
}

std::ostream& fhirtools::operator<<(std::ostream& os, Element::Type type)
{
    os << magic_enum::enum_name(type);
    return os;
}

//NOLINTNEXTLINE(misc-no-recursion)
std::ostream& fhirtools::operator<<(std::ostream& os, const Element& element)
{
    os << "{";
    if (auto el = element.definitionPointer().element())
    {
        os << R"("name": ")" << el->name() << R"(", )";
    }
    os << R"("type":")" << element.type() << '"';
    if (element.definitionPointer().profile()->kind() == FhirStructureDefinition::Kind::resource &&
        element.definitionPointer().element()->isRoot())
    {
        os << R"(, "resourceType":")" << element.resourceType() << '"';
    }

    os << R"(, "value":)";
    if (element.type() != Element::Type::Structured)
    {
        os << '"' << element.asString() << '"';
    }
    else
    {
        os << "{";
        std::string_view sep;
        for (const auto& subElementName : element.subElementNames())
        {
            os << sep;
            sep = ", ";
            Collection c{element.subElements(subElementName)};
            os << '"' << subElementName << R"(":)";
            if (c.size() > 1)
            {
                os << c;
            }
            else if (! c.empty())
            {
                os << *c[0];
            }
            else
            {
                os << "{}";
            }
        }
        os << "}";
    }
    os << "}";
    return os;
}

//NOLINTNEXTLINE(misc-no-recursion)
std::ostream& Element::json(std::ostream& out) const
{
    switch (mType)
    {
        using enum Type;
        case Integer:
        case Decimal:
        case Boolean:
            return out << asString();
        case Date:
        case DateTime:
        case Time:
        case Quantity:
        case String:
            return out << '"' << asString() << '"';
        case Structured: {
            out << '{';
            if (definitionPointer().isResource())
            {
                out << R"("resourceType": ")" << definitionPointer().profile()->typeId() << R"(", )";
            }
            std::string_view fieldSep;
            for (const auto& name : subElementNames())
            {
                auto defPtrs = definitionPointer().subDefinitions(*getFhirStructureRepository(), name);
                out << fieldSep;
                fieldSep = ", ";
                if (defPtrs.empty())
                {
                    out << "<no-def:" << definitionPointer() << '.' << name << ">";
                    break;
                }
                auto subList = subElements(name);
                if (defPtrs.front().element()->isArray() || subList.size() != 1)
                {
                    out << '"' << name << R"(": [)";
                    std::string_view arrSep;
                    for (auto& sub : subList)
                    {
                        out << arrSep;
                        arrSep = ", ";
                        sub->json(out);
                    }
                    out << ']';
                }
                else
                {
                    out << '"' << name << R"(": )";
                    subList.back()->json(out);
                }
            }
            out << '}';
        }
            return out;
    }
    return out << "<error>";
}
