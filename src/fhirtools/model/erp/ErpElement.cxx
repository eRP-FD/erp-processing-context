/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/model/NumberAsStringParserWriter.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <utility>

using fhirtools::Element;
using fhirtools::FhirStructureDefinition;
using fhirtools::FhirStructureRepository;
using fhirtools::MutableElement;
using fhirtools::PrimitiveElement;
using fhirtools::ProfiledElementTypeInfo;

ErpElement::ErpElement(const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository,
                       std::weak_ptr<const Element> parent, const std::string& elementId, const rapidjson::Value* value,
                       const rapidjson::Value* primitiveTypeObject)
    : ErpElement(fhirStructureRepository, std::move(parent),
                 ProfiledElementTypeInfo{fhirStructureRepository, elementId}, value, primitiveTypeObject)
{
}

ErpElement::ErpElement(const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository,
                       std::weak_ptr<const Element> parent, ProfiledElementTypeInfo defPtr,
                       const rapidjson::Value* value, const rapidjson::Value* primitiveTypeObject)
    : MutableElement(fhirStructureRepository, std::move(parent), std::move(defPtr))
    , mValue(value)
    , mPrimitiveTypeObject(primitiveTypeObject)
{
    FPExpect(mValue || mPrimitiveTypeObject, "Element must have either value or primitive type object");
    FPExpect(! mPrimitiveTypeObject || mPrimitiveTypeObject->IsObject(), "primitive type object is not an object.");
}

ErpElement::ErpElement(const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository,
                       std::weak_ptr<const Element> parent, const FhirStructureDefinition* structureDefinition,
                       const std::string& elementId, const rapidjson::Value* value,
                       const rapidjson::Value* primitiveTypeObject)
    : ErpElement(fhirStructureRepository, std::move(parent),
                 ProfiledElementTypeInfo{structureDefinition, structureDefinition->findElement(elementId)}, value,
                 primitiveTypeObject)
{
}

ErpElement::ErpElement(const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository,
                       std::weak_ptr<const Element> parent, fhirtools::ProfiledElementTypeInfo defPtr,
                       model::NumberAsStringParserDocument* document, rapidjson::Value* value,
                       rapidjson::Value* primitiveTypeObject)
    : MutableElement(fhirStructureRepository, std::move(parent), std::move(defPtr))
    , mValue(value)
    , mPrimitiveTypeObject(primitiveTypeObject)
    , mValueMutable(value)
    , mPrimitiveTypeObjectMutable(primitiveTypeObject)
    , mDocumentMutable(document)
{
    FPExpect(mValue || mPrimitiveTypeObject, "Element must have either value or primitive type object");
    FPExpect(mDocumentMutable, "Document must be given for mutable elements");
    FPExpect(! mPrimitiveTypeObject || mPrimitiveTypeObject->IsObject(), "primitive type object is not an object.");
}

std::string ErpElement::resourceType(const rapidjson::Value& resource)
{
    static const rapidjson::Pointer ptr("/resourceType");
    return std::string(model::NumberAsStringParserDocument::getOptionalStringValue(resource, ptr).value_or(""));
}

std::string ErpElement::resourceType() const
{
    FPExpect3(mValue || mPrimitiveTypeObject, "Element has no value", std::logic_error);
    return mValue ? resourceType(*mValue) : resourceType(*mPrimitiveTypeObject);
}

std::vector<std::string_view> ErpElement::profiles() const
{
    FPExpect3(mValue || mPrimitiveTypeObject, "Element has no value", std::logic_error);
    return mValue ? profiles(*mValue) : profiles(*mPrimitiveTypeObject);
}

std::string ErpElement::asRaw() const
{
    FPExpect3(mValue, "Element has no value", std::logic_error);
    switch (type())
    {
        case Type::Boolean:
            return std::string{mValue->GetBool() ? "true" : "false"};
        case Type::Structured:
        case Type::Quantity:
            return model::NumberAsStringParserDocument::serializeToJsonString(*mValue);
        case Type::Integer:
        case Type::Decimal:
        case Type::String:
        case Type::Date:
        case Type::DateTime:
        case Type::Time:
            break;
    }
    return std::string{model::NumberAsStringParserDocument::valueAsRaw(*mValue)};
}

int32_t ErpElement::asInt() const
{
    return asPrimitiveElement().asInt();
}

fhirtools::DecimalType ErpElement::asDecimal() const
{
    return asPrimitiveElement().asDecimal();
}

bool ErpElement::asBool() const
{
    return asPrimitiveElement().asBool();
}

std::string ErpElement::asString() const
{
    FPExpect3(mValue, "Element has no value", std::logic_error);
    if (mValue->IsString())
    {
        return std::string{model::NumberAsStringParserDocument::getStringValueFromValue(mValue)};
    }
    return asPrimitiveElement().asString();
}

fhirtools::Date ErpElement::asDate() const
{
    return asPrimitiveElement().asDate();
}

fhirtools::Time ErpElement::asTime() const
{
    return asPrimitiveElement().asTime();
}

fhirtools::DateTime ErpElement::asDateTime() const
{
    return asPrimitiveElement().asDateTime();
}

Element::QuantityType ErpElement::asQuantity() const
{
    return asPrimitiveElement().asQuantity();
}

const rapidjson::Value* ErpElement::asJson() const
{
    return mValue;
}

bool ErpElement::hasSubElement(const std::string& name) const
{

    const auto* obj = mValue->IsObject() ? mValue : mPrimitiveTypeObject;
    if (obj && obj->IsObject())
    {
        for (auto mem = obj->MemberBegin(); mem != obj->MemberEnd(); ++mem)
        {
            std::string_view memberName{mem->name.GetString(), mem->name.GetStringLength()};
            if ((memberName == name) || (! memberName.empty() && memberName[0] == '_' && memberName.substr(1) == name))
            {
                return true;
            }
        }
    }
    return false;
}

std::vector<std::string> ErpElement::subElementNames() const
{
    std::set<std::string_view> haveName{"resourceType"};
    auto IterateObject = [&](const model::NumberAsStringParserDocument::ConstObject& obj) {
        std::vector<std::string> ret;
        ret.reserve(obj.MemberCount());
        for (const auto& item : obj)
        {
            std::string_view name{item.name.GetString(), item.name.GetStringLength()};
            FPExpect(! name.empty(), "Empty name in document.");
            if (name[0] == '_')
            {
                name = std::string_view{item.name.GetString() + 1, item.name.GetStringLength() - 1};
                FPExpect(! name.empty(), "invalid field name: '_'");
            }
            if (haveName.insert(name).second)
            {
                ret.emplace_back(name);
            }
        }
        return ret;
    };

    if (mPrimitiveTypeObject)
    {
        FPExpect(mPrimitiveTypeObject->IsObject(), "primitive type object is not an object.");
        return IterateObject(mPrimitiveTypeObject->GetObject());
    }
    if (mValue && mValue->IsObject())
    {
        return IterateObject(mValue->GetObject());
    }
    return {};
}

bool ErpElement::hasValue() const
{
    return mValue && ! mValue->IsNull() && ! mValue->IsObject();
}

std::shared_ptr<ErpElement> ErpElement::createElement(ProfiledElementTypeInfo defPtr, bool isResource,
                                                      const rapidjson::Value* val, const rapidjson::Value* primitiveVal,
                                                      rapidjson::Value* valMutable,
                                                      rapidjson::Value* primitiveValMutable,
                                                      const std::string& elementName,
                                                      std::optional<size_t> arrayIndex) const
{
    if (isResource)
    {
        const auto& structureDef = mFhirStructureRepository->findTypeById(resourceType(*val));
        defPtr.typecast(*mFhirStructureRepository, structureDef);
    }
    std::shared_ptr<ErpElement> subElement;
    if (! mDocumentMutable)
    {
        subElement =
            std::make_shared<ErpElement>(mFhirStructureRepository, weak_from_this(), defPtr, val, primitiveVal);
    }
    else
    {
        subElement = std::make_shared<ErpElement>(mFhirStructureRepository, weak_from_this(), defPtr, mDocumentMutable,
                                                  valMutable, primitiveValMutable);
    }
    subElement->mElementName = elementName;
    subElement->mArrayIndex = arrayIndex;
    return subElement;
}

// NOLINTNEXTLINE(*-function-cognitive-complexity)
std::vector<std::shared_ptr<const Element>> ErpElement::subElements(const std::string& name) const
{
    auto cachedPos = mSubElementCache.lower_bound(name);
    if (cachedPos != mSubElementCache.end() && cachedPos->first == name)
    {
        return cachedPos->second;
    }
    using std::max;
    FPExpect(mDefinitionPointer.element(), "element must be set");
    if (mDefinitionPointer.isResource() && name == resourceType())
    {
        // cannot be cached, due to shared_ptr loop
        return {shared_from_this()};
    }
    const auto& elementId = mDefinitionPointer.element()->name();
    rapidjson::Pointer pointer{"/" + name};
    rapidjson::Pointer primitivePointer{"/_" + name};

    auto subPointerList = mDefinitionPointer.subDefinitions(*mFhirStructureRepository, name);
    FPExpect(! subPointerList.empty(),
             mDefinitionPointer.profile()->urlAndVersion() + " no such element: " + elementId + '.' + name);
    const auto& subPtr = subPointerList.back();
    bool isResource = subPtr.isResource();
    bool isPrimitive =
        subPtr.element()->isRoot() && subPtr.profile()->kind() == FhirStructureDefinition::Kind::primitiveType;
    const rapidjson::Value* val = pointer.Get(mPrimitiveTypeObject ? *mPrimitiveTypeObject : *mValue);
    const auto* primitiveVal = isPrimitive && mValue ? primitivePointer.Get(*mValue) : nullptr;
    rapidjson::Value* valMutable = nullptr;
    rapidjson::Value* primitiveValMutable = nullptr;
    if (mValueMutable || mPrimitiveTypeObjectMutable)
    {
        valMutable = pointer.Get(mPrimitiveTypeObjectMutable ? *mPrimitiveTypeObjectMutable : *mValueMutable);
        primitiveValMutable = isPrimitive && mValueMutable ? primitivePointer.Get(*mValueMutable) : nullptr;
    }
    if (val == nullptr)
    {
        if (primitiveVal == nullptr)
        {
            return mSubElementCache.insert(cachedPos, {name, {}})->second;
        }
        else
        {
            val = primitiveVal;
            primitiveVal = nullptr;
            valMutable = primitiveValMutable;
            primitiveValMutable = nullptr;
        }
    }
    if (val->IsArray())
    {
        if (valMutable)
        {
            return mSubElementCache
                .insert(cachedPos, {name, arraySubElements(subPtr, valMutable, primitiveValMutable, name)})
                ->second;
        }
        return mSubElementCache.insert(cachedPos, {name, arraySubElements(subPtr, val, primitiveVal, name)})->second;
    }
    else
    {
        return mSubElementCache
            .insert(cachedPos, {name,
                                {createElement(subPtr, isResource, val, primitiveVal, valMutable, primitiveValMutable,
                                               name, std::nullopt)}})
            ->second;
    }
}

template<typename ValueT>
std::vector<std::shared_ptr<const Element>> ErpElement::arraySubElements(const ProfiledElementTypeInfo& subPtr,
                                                                         ValueT* val, ValueT* primitiveVal,
                                                                         const std::string& name) const
{
    FPExpect(! primitiveVal || primitiveVal->IsArray(), "value /_" + name + " is not an array");
    auto primitiveArr = primitiveVal ? std::make_optional(primitiveVal->GetArray()) : std::nullopt;
    auto arr = val->GetArray();
    auto end = std::max(arr.Size(), primitiveArr ? primitiveArr->Size() : 0);
    std::vector<std::shared_ptr<const Element>> ret;
    for (rapidjson::SizeType i = 0; i < end; ++i)
    {
        bool havePrimitive = primitiveArr && primitiveArr->Size() > i && ! (*primitiveArr)[i].IsNull();
        auto* primitive = havePrimitive ? &((*primitiveArr)[i]) : nullptr;
        auto* arrVal = arr.Size() > i ? &arr[i] : nullptr;
        if constexpr (std::is_const_v<ValueT>)
        {
            ret.emplace_back(createElement(subPtr, subPtr.isResource(), arrVal, primitive, nullptr, nullptr, name, i));
        }
        else
        {
            ret.emplace_back(createElement(subPtr, subPtr.isResource(), arrVal, primitive, arrVal, primitive, name, i));
        }
    }
    return ret;
}


PrimitiveElement ErpElement::asPrimitiveElement() const
{
    Expect3(mValue, "Element has no value", std::logic_error);
    switch (type())
    {
        case Type::Integer:
            return PrimitiveElement(mFhirStructureRepository, type(),
                                    model::NumberAsStringParserDocument::getOptionalIntValue(*mValue, {}).value());
        case Type::Decimal:
            return PrimitiveElement(
                mFhirStructureRepository, type(),
                fhirtools::DecimalType(model::NumberAsStringParserDocument::getStringValueFromValue(mValue)));
        case Type::String:
            return PrimitiveElement(mFhirStructureRepository, type(),
                                    std::string(model::NumberAsStringParserDocument::getStringValueFromValue(mValue)));
        case Type::Boolean:
            return PrimitiveElement(mFhirStructureRepository, type(), mValue->GetBool());
        case Type::Date:
            return PrimitiveElement(
                mFhirStructureRepository, type(),
                fhirtools::Date(std::string(model::NumberAsStringParserDocument::getStringValueFromValue(mValue))));
        case Type::DateTime:
            return PrimitiveElement(
                mFhirStructureRepository, type(),
                fhirtools::DateTime(std::string(model::NumberAsStringParserDocument::getStringValueFromValue(mValue))));
        case Type::Time:
            return PrimitiveElement(
                mFhirStructureRepository, type(),
                fhirtools::Time(std::string(model::NumberAsStringParserDocument::getStringValueFromValue(mValue))));
        case Type::Structured:
            break;
        case Type::Quantity: {
            const auto* valueElement = rapidjson::Pointer("/value").Get(*mValue);
            FPExpect(valueElement, "Quantity value not defined");
            const auto value = model::NumberAsStringParserDocument::getStringValueFromValue(valueElement);
            FPExpect(! value.empty(), "Quantity value not defined");
            const auto unit =
                model::NumberAsStringParserDocument::getOptionalStringValue(*mValue, rapidjson::Pointer("/unit"));
            return PrimitiveElement(mFhirStructureRepository, type(),
                                    QuantityType(fhirtools::DecimalType(value), unit.value_or("")));
        }
    }
    FPFail("not convertible to primitive");
}

std::vector<std::string_view> ErpElement::profiles(const rapidjson::Value& resource)
{
    using namespace std::string_literals;
    static const rapidjson::Pointer ptr("/meta/profile");
    const auto* profile = ptr.Get(resource);
    if (! profile)
    {
        return {};
    }
    FPExpect(profile->IsArray(), "meta.profile is not an array.");
    const auto& profileArr = profile->GetArray();
    std::vector<std::string_view> result;
    result.reserve(profileArr.Size());
    try
    {
        for (const auto& p : profileArr)
        {
            result.emplace_back(model::NumberAsStringParserDocument::valueAsString(p));
        }
    }
    catch (const model::ModelException& m)
    {
        FPFail("meta.profile: "s + m.what());
    }
    return result;
}

const std::string& ErpElement::elementName() const
{
    return mElementName;
}

void ErpElement::setString(std::string_view stringValue) const
{
    FPExpect(mValueMutable && mDocumentMutable, "Element is not Mutable!");
    mDocumentMutable->setKeyValue(*mValueMutable, {}, stringValue);
}

void ErpElement::setDataAbsentExtension(const std::string_view& missingElementName) const
{
    FPExpect(mValueMutable && mDocumentMutable, "Element is not Mutable!");
    auto* useValue = mPrimitiveTypeObjectMutable ? mPrimitiveTypeObjectMutable : mValueMutable;
    const std::string extension = "/" + std::string{missingElementName} + "/extension";
    auto* array = rapidjson::Pointer{extension}.Get(*mDocumentMutable);
    FPExpect(! array, extension + " already exists");
    mDocumentMutable->setKeyValue(*useValue, rapidjson::Pointer{extension + "/0/url"},
                                  "http://hl7.org/fhir/StructureDefinition/data-absent-reason");
    mDocumentMutable->setKeyValue(*useValue, rapidjson::Pointer{extension + "/0/valueCode"}, "unknown");
}

void ErpElement::removeFromParent() const
{
    FPExpect(mValueMutable && mDocumentMutable, "Element is not Mutable!");
    auto parent = std::dynamic_pointer_cast<const ErpElement>(mParent.lock());
    if (parent)
    {
        parent->removeSubElement(mElementName, mArrayIndex);
    }
    else
    {
        TVLOG(1) << mElementName << ": parent has already been removed";
    }
}

void ErpElement::removeSubElement(const std::string& name, std::optional<size_t> arrayIndex) const
{
    auto* useValue = mPrimitiveTypeObjectMutable ? mPrimitiveTypeObjectMutable : mValueMutable;
    auto ptr = "/" + name;
    if (arrayIndex)
    {
        ptr += "/" + std::to_string(*arrayIndex);
    }
    for (const auto& item : subElements(name))
    {
        auto erpElement = std::dynamic_pointer_cast<const ErpElement>(item);
        if (erpElement->mArrayIndex > arrayIndex)
        {
            // refresh array indexes of sub-elements in existing weak-ptrs in validation result
            --(*erpElement->mArrayIndex);
        }
        else if (erpElement->mArrayIndex == arrayIndex)
        {
            const auto [first, last] = std::ranges::remove(mSubElementCache.at(name), item);
            FPExpect(first != last, "error");
            mSubElementCache.at(name).erase(first, last);
        }
    }
    TVLOG(1) << "removing " << ptr << " from " << mElementName;
    rapidjson::StringBuffer buffer;
    model::NumberAsStringParserWriter<rapidjson::StringBuffer> writer(buffer);
    useValue->Accept(writer);
    TVLOG(1) << buffer.GetString();
    mDocumentMutable->removeEntry(*useValue, rapidjson::Pointer{ptr});
}
