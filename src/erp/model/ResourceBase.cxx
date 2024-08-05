/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/ResourceBase.hxx"
#include "erp/fhir/Fhir.hxx"
#include "erp/fhir/FhirCanonicalizer.hxx"
#include "erp/model/ResourceNames.hxx"


#ifdef _WIN32
#pragma warning(                                                                                                       \
    disable : 4127)// suppress warning C4127: Conditional expression is constant (rapidjson::Pointer::Stringify)
#endif

namespace model
{

namespace rj = rapidjson;

namespace
{
const auto profilePointer = ::rapidjson::Pointer{"/meta/profile/0"};

rj::GenericStringRef<std::string_view::value_type> stringRefFrom(const std::string_view& s)
{
    return rj::StringRef(s.data(), s.size());
}

}// namespace

ResourceBase::ResourceBase()
{
    mJsonDocument.SetObject();
}

ResourceBase::ResourceBase(const ::rapidjson::Document& resourceTemplate)
{
    mJsonDocument.CopyFrom(resourceTemplate, mJsonDocument.GetAllocator());
}

std::string ResourceBase::serializeToJsonString() const
{
    return mJsonDocument.serializeToJsonString();
}


std::string ResourceBase::serializeToCanonicalJsonString() const
{
    return serializeToCanonicalJsonString(jsonDocument());
}

std::string ResourceBase::serializeToCanonicalJsonString(const rj::Value& value)
{
    return FhirCanonicalizer::serialize(value);
}

std::string ResourceBase::serializeToXmlString() const
{
    return Fhir::instance().converter().jsonToXmlString(mJsonDocument);
}

const NumberAsStringParserDocument& ResourceBase::jsonDocument() const&
{
    return mJsonDocument;
}

NumberAsStringParserDocument&& model::ResourceBase::jsonDocument() &&
{
    return std::move(mJsonDocument);
}

ResourceBase::ResourceBase(NumberAsStringParserDocument&& document)
    : mJsonDocument(std::move(document))
{
}

std::string ResourceBase::pointerToString(const rapidjson::Pointer& pointer)
{
    return NumberAsStringParserDocument::pointerToString(pointer);
}

rj::Value ResourceBase::createEmptyObject()
{
    return NumberAsStringParserDocument::createEmptyObject();
}

void ResourceBase::setKeyValue(rj::Value& object, const rapidjson::Pointer& key, const std::string_view& value)
{
    mJsonDocument.setKeyValue(object, key, value);
}

void ResourceBase::setKeyValue(rj::Value& object, const rapidjson::Pointer& key, const rj::Value& value)
{
    mJsonDocument.setKeyValue(object, key, value);
}

void ResourceBase::setKeyValue(rj::Value& object, const rapidjson::Pointer& key, int value)
{
    mJsonDocument.setKeyValue(object, key, value);
}

void ResourceBase::setKeyValue(rj::Value& object, const rapidjson::Pointer& key, unsigned int value)
{
    mJsonDocument.setKeyValue(object, key, value);
}

void ResourceBase::setKeyValue(rj::Value& object, const rapidjson::Pointer& key, int64_t value)
{
    mJsonDocument.setKeyValue(object, key, value);
}

void ResourceBase::setKeyValue(rj::Value& object, const rapidjson::Pointer& key, uint64_t value)
{
    mJsonDocument.setKeyValue(object, key, value);
}

void ResourceBase::setKeyValue(rj::Value& object, const rapidjson::Pointer& key, double value)
{
    mJsonDocument.setKeyValue(object, key, value);
}

void ResourceBase::setValue(const rapidjson::Pointer& pointerToEntry, const std::string_view& value)
{
    mJsonDocument.setValue(pointerToEntry, value);
}

void ResourceBase::setValue(const rapidjson::Pointer& pointerToEntry, const rapidjson::Value& value)
{
    mJsonDocument.setValue(pointerToEntry, value);
}

void ResourceBase::setValue(const rapidjson::Pointer& pointerToEntry, int value)
{
    mJsonDocument.setValue(pointerToEntry, value);
}

void ResourceBase::setValue(const rapidjson::Pointer& pointerToEntry, unsigned int value)
{
    mJsonDocument.setValue(pointerToEntry, value);
}

void ResourceBase::setValue(const rapidjson::Pointer& pointerToEntry, int64_t value)
{
    mJsonDocument.setValue(pointerToEntry, value);
}

void ResourceBase::setValue(const rapidjson::Pointer& pointerToEntry, uint64_t value)
{
    mJsonDocument.setValue(pointerToEntry, value);
}

void ResourceBase::setValue(const rapidjson::Pointer& pointerToEntry, double value)
{
    mJsonDocument.setValue(pointerToEntry, value);
}

void ResourceBase::removeElement(const rapidjson::Pointer& pointerToEntry)
{
    pointerToEntry.Erase(mJsonDocument);
}

rapidjson::Value ResourceBase::copyValue(const rapidjson::Value& value)
{
    rapidjson::Value v;
    v.CopyFrom(value, mJsonDocument.GetAllocator());
    return v;
}

std::size_t ResourceBase::addToArray(const rapidjson::Pointer& pointerToArray, std::string_view str)
{
    return mJsonDocument.addToArray(pointerToArray, mJsonDocument.makeString(str));
}

std::size_t ResourceBase::addToArray(const rj::Pointer& pointerToArray, rapidjson::Value&& object)
{
    return mJsonDocument.addToArray(pointerToArray, std::move(object));
}

std::size_t ResourceBase::addToArray(rapidjson::Value& array, rapidjson::Value&& object)
{
    return mJsonDocument.addToArray(array, std::move(object));
}

std::size_t ResourceBase::addStringToArray(rapidjson::Value& array, std::string_view str)
{
    return addToArray(array, mJsonDocument.makeString(str));
}

void ResourceBase::addMemberToArrayEntry(const ::rapidjson::Pointer& pointerToArray, ::std::size_t index,
                                         ::std::string_view key, ::std::string_view value)
{
    mJsonDocument.addMemberToArrayEntry(pointerToArray, index, ::rapidjson::Value{stringRefFrom(key)},
                                        mJsonDocument.makeString(value));
}

void ResourceBase::addMemberToArrayEntry(const ::rapidjson::Pointer& pointerToArray, ::std::size_t index,
                                         ::std::string_view key, ::std::initializer_list<::std::string_view> values)
{
    auto array = ::rapidjson::Value{::rapidjson::Type::kArrayType};
    for (const auto& value : values)
    {
        array.PushBack(mJsonDocument.makeString(value), mJsonDocument.GetAllocator());
    }

    mJsonDocument.addMemberToArrayEntry(pointerToArray, index, ::rapidjson::Value{stringRefFrom(key)},
                                        ::std::move(array));
}

void ResourceBase::removeFromArray(const rj::Pointer& pointerToArray, std::size_t index)
{
    return mJsonDocument.removeFromArray(pointerToArray, index);
}

void ResourceBase::clearArray(const rj::Pointer& pointerToArray)
{
    return mJsonDocument.clearArray(pointerToArray);
}

std::string_view ResourceBase::getStringValue(const rapidjson::Pointer& pointerToEntry) const
{
    const auto str = getOptionalStringValue(pointerToEntry);
    ModelExpect(str.has_value(), pointerToString(pointerToEntry) + ": entry not present or not a string");
    return str.value();
}

std::optional<std::string_view> ResourceBase::getOptionalStringValue(const rapidjson::Pointer& pointerToEntry) const
{
    return mJsonDocument.getOptionalStringValue(pointerToEntry);
}

std::string_view ResourceBase::getStringValue(const rj::Value& object, const rj::Pointer& key) const
{
    const auto str = getOptionalStringValue(object, key);
    ModelExpect(str.has_value(), pointerToString(key) + ": entry not present or not a string");
    return str.value();
}

std::optional<std::string_view> ResourceBase::getOptionalStringValue(const rj::Value& object,
                                                                     const rj::Pointer& key) const
{
    return NumberAsStringParserDocument::getOptionalStringValue(object, key);
}

int ResourceBase::getIntValue(const rapidjson::Pointer& pointerToEntry) const
{
    const auto i = getOptionalIntValue(pointerToEntry);
    ModelExpect(i.has_value(), pointerToString(pointerToEntry) + ": entry not present or not an int");
    return i.value();
}

std::optional<int> ResourceBase::getOptionalIntValue(const rapidjson::Pointer& pointerToEntry) const
{
    return mJsonDocument.getOptionalIntValue(pointerToEntry);
}

unsigned int ResourceBase::getUIntValue(const rapidjson::Pointer& pointerToEntry) const
{
    const auto i = getOptionalUIntValue(pointerToEntry);
    ModelExpect(i.has_value(), pointerToString(pointerToEntry) + ": entry not present or not an unsigned int");
    return i.value();
}

std::optional<unsigned int> ResourceBase::getOptionalUIntValue(const rapidjson::Pointer& pointerToEntry) const
{
    return mJsonDocument.getOptionalUIntValue(pointerToEntry);
}

int64_t ResourceBase::getInt64Value(const rapidjson::Pointer& pointerToEntry) const
{
    const auto i = getOptionalInt64Value(pointerToEntry);
    ModelExpect(i.has_value(), pointerToString(pointerToEntry) + ": entry not present or not an int64");
    return i.value();
}

std::optional<int64_t> ResourceBase::getOptionalInt64Value(const rapidjson::Pointer& pointerToEntry) const
{
    return mJsonDocument.getOptionalInt64Value(pointerToEntry);
}

uint64_t ResourceBase::getUInt64Value(const rapidjson::Pointer& pointerToEntry) const
{
    const auto i = getOptionalUInt64Value(pointerToEntry);
    ModelExpect(i.has_value(), pointerToString(pointerToEntry) + ": entry not present or not an unsigned int64");
    return i.value();
}

std::optional<uint64_t> ResourceBase::getOptionalUInt64Value(const rapidjson::Pointer& pointerToEntry) const
{
    return mJsonDocument.getOptionalUInt64Value(pointerToEntry);
}

double ResourceBase::getDoubleValue(const rapidjson::Pointer& pointerToEntry) const
{
    const auto f = getOptionalDoubleValue(pointerToEntry);
    ModelExpect(f.has_value(), pointerToString(pointerToEntry) + ": entry not present or not a double");
    return f.value();
}

std::optional<double> ResourceBase::getOptionalDoubleValue(const rapidjson::Pointer& pointerToEntry) const
{
    return mJsonDocument.getOptionalDoubleValue(pointerToEntry);
}

std::optional<std::string_view> ResourceBase::getNumericAsString(const rapidjson::Pointer& pointerToEntry) const
{
    return mJsonDocument.getNumericAsString(pointerToEntry);
}

const rapidjson::Value& ResourceBase::getValueMember(const rapidjson::Value& value, std::string_view valueMemberName)
{
    const auto valueMember = value.FindMember(stringRefFrom(valueMemberName));
    ModelExpect(valueMember != value.MemberEnd(), "valueMember does not exist: " + std::string(valueMemberName));
    return valueMember->value;
}

const rapidjson::Value* ResourceBase::getValue(const rapidjson::Pointer& pointer) const
{
    return pointer.Get(mJsonDocument);
}

rapidjson::Value* ResourceBase::getValue(const rapidjson::Pointer& pointer)
{
    return pointer.Get(mJsonDocument);
}

const rapidjson::Value* ResourceBase::getValue(const rapidjson::Value& parent, const rapidjson::Pointer& pointer)
{
    return pointer.Get(parent);
}

std::optional<std::string_view> ResourceBase::findStringInArray(const rj::Pointer& arrayPointer,
                                                                const rj::Pointer& searchKey,
                                                                const std::string_view& searchValue,
                                                                const rj::Pointer& resultKeyPointer,
                                                                bool ignoreValueCase) const
{
    return mJsonDocument.findStringInArray(arrayPointer, searchKey, searchValue, resultKeyPointer, ignoreValueCase);
}

std::optional<std::tuple<const rapidjson::Value*, std::size_t>>
ResourceBase::findMemberInArray(const rapidjson::Pointer& arrayPointer, const rapidjson::Pointer& searchKey,
                                const std::string_view& searchValue, const rapidjson::Pointer& resultKeyPointer,
                                bool ignoreValueCase) const
{
    return mJsonDocument.findMemberInArray(arrayPointer, searchKey, searchValue, resultKeyPointer, ignoreValueCase);
}

rapidjson::Value* ResourceBase::findMemberInArray(const rapidjson::Pointer& arrayPointer,
                                                  const rapidjson::Pointer& searchKey,
                                                  const std::string_view& searchValue)
{
    return mJsonDocument.findMemberInArray(arrayPointer, searchKey, searchValue);
}

const rapidjson::Value* ResourceBase::findMemberInArray(const rapidjson::Pointer& arrayPointer,
                                                        const rapidjson::Pointer& searchKey,
                                                        const std::string_view& searchValue) const
{
    return mJsonDocument.findMemberInArray(arrayPointer, searchKey, searchValue);
}

const rapidjson::Value* ResourceBase::getMemberInArray(const rapidjson::Pointer& pointerToArray, size_t index) const
{
    return mJsonDocument.getMemberInArray(pointerToArray, index);
}

bool ResourceBase::hasValue(const rapidjson::Pointer& pointerToValue) const
{
    return pointerToValue.Get(mJsonDocument) != nullptr;
}

size_t ResourceBase::valueSize(const rapidjson::Pointer& pointerToEntry) const
{
    const auto* resources = pointerToEntry.Get(mJsonDocument);
    if (resources == nullptr)
        return 0;
    else
        return resources->Size();
}

}// namespace model
