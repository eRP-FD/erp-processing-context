/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/model/NumberAsStringParserWriter.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/String.hxx"

#include <rapidjson/ostreamwrapper.h>
#include <regex>

#ifdef _WIN32
#pragma warning(disable : 4127) // suppress warning C4127: Conditional expression is constant (rapidjson::Pointer::Stringify)
#endif

using namespace model;
namespace rj = rapidjson;

bool NumberAsStringParserDocument::valueIsObject(const rj::Value& value)
{
    return value.IsObject();
}

bool NumberAsStringParserDocument::valueIsArray(const rj::Value& value)
{
    return value.IsArray();
}

bool NumberAsStringParserDocument::valueIsNull(const rj::Value& value)
{
    return value.IsNull();
}

bool NumberAsStringParserDocument::valueIsBool(const rj::Value& value)
{
    return value.IsBool();
}

bool NumberAsStringParserDocument::valueIsFalse(const rj::Value& value)
{
    return value.IsFalse();
}

bool NumberAsStringParserDocument::valueIsTrue(const rj::Value& value)
{
    return value.IsTrue();
}

bool NumberAsStringParserDocument::valueIsNumber(const rj::Value& value)
{
    if (value.IsString())
    {
        ModelExpect(value.GetStringLength() > 0, "at least prefix character expected");
        return (value.GetString()[0] == prefixNumber);
    }
    return false;
}

bool NumberAsStringParserDocument::valueIsString(const rj::Value& value)
{
    if (value.IsString())
    {
        ModelExpect(value.GetStringLength() > 0, "at least prefix character expected");
        return (value.GetString()[0] == prefixString);
    }
    return false;
}

rj::Value NumberAsStringParserDocument::createEmptyObject()
{
    rj::Value object(rj::kObjectType);
    object.SetObject();
    return object;
}

rj::Value model::NumberAsStringParserDocument::makeBool(bool value)
{
    return rj::Value{value};
}

rj::Value model::NumberAsStringParserDocument::makeNumber(const std::string_view& number)
{
    // regular expression for decimal: http://hl7.org/fhir/R4/datatypes.html#decimal
    static std::string numberRegexStr{R"(-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?)"};
    std::regex numberRegex{numberRegexStr};
    ModelExpect(regex_match(number.begin(), number.end(), numberRegex), "Invalid number format.");
    return makeValue(prefixNumber, number);
}

rj::Value model::NumberAsStringParserDocument::makeString(const std::string_view& str)
{
    return makeValue(prefixString, str);
}

model::NumberAsStringParserDocument model::NumberAsStringParserDocument::fromJson(std::string_view json)
{
    NumberAsStringParserDocument document;
    rj::StringStream s(json.data());
    document.ParseStream<rj::kParseNumbersAsStringsFlag, rj::CustomUtf8>(s);
    ModelExpect(!document.HasParseError(), "can not parse json string");
    return document;
}

std::string model::NumberAsStringParserDocument::serializeToJsonString() const
{
    rj::StringBuffer buffer;
    NumberAsStringParserWriter<rj::StringBuffer> writer(buffer);
    Accept(writer);
    return {buffer.GetString()};
}

std::string NumberAsStringParserDocument::pointerToString(const rj::Pointer& pointer)
{
    std::ostringstream oss;
    rj::BasicOStreamWrapper wrapper(oss);
    pointer.Stringify(wrapper);
    return oss.str();
}

std::string_view NumberAsStringParserDocument::getStringValueFromValue(const rj::Value* value)
{
    Expect3(value != nullptr, "value is nullptr", std::logic_error);
    ModelExpect(value->IsString(), "value is not a string");
    ModelExpect(value->GetStringLength() > 0, "at least prefix character expected");
    ModelExpect(value->GetString()[0] == prefixString || value->GetString()[0] == prefixNumber,
        "string values must start with prefix character");
    return &value->GetString()[1];
}

std::string_view NumberAsStringParserDocument::getStringValueFromPointer(const rj::Pointer& pointer) const
{
    const auto* value = pointer.Get(*this);
    ModelExpect(value != nullptr, pointerToString(pointer) + " is not existing");
    ModelExpect(value->IsString(), "value is not a string");
    ModelExpect(value->GetStringLength() > 0, "at least prefix character expected");
    ModelExpect(value->GetString()[0] == prefixString || value->GetString()[0] == prefixNumber,
        "string values must start with prefix character");
    return &value->GetString()[1];
}

void NumberAsStringParserDocument::setKeyValue(rj::Value& object, const rj::Pointer& key, const rj::Value& value)
{
    if (value.IsString())
    {
        std::string prefixedValue = prefixString + std::string(value.GetString(), value.GetStringLength());
        key.Set(object, prefixedValue, GetAllocator());
    }
    else if (value.IsDouble())
    {
        std::string prefixedValue = prefixNumber + std::to_string(value.GetDouble());
        key.Set(object, prefixedValue, GetAllocator());
    }
    else if (value.IsInt() )
    {
        std::string prefixedValue = prefixNumber + std::to_string(value.GetInt());
        key.Set(object, prefixedValue, GetAllocator());
    }
    else if (value.IsUint())
    {
        std::string prefixedValue = prefixNumber + std::to_string(value.GetUint());
        key.Set(object, prefixedValue, GetAllocator());
    }
    else if (value.IsInt64())
    {
        std::string prefixedValue = prefixNumber + std::to_string(value.GetInt64());
        key.Set(object, prefixedValue, GetAllocator());
    }
    else if (value.IsUint64())
    {
        std::string prefixedValue = prefixNumber + std::to_string(value.GetUint64());
        key.Set(object, prefixedValue, GetAllocator());
    }
    else
    {
        Expect3(! value.IsNumber(), "unhandled number type", std::logic_error);
        key.Set(object, value, GetAllocator());
    }
}

void NumberAsStringParserDocument::setKeyValue(rj::Value& object, const rj::Pointer& key, const std::string_view& value)
{
    // The passed string may be marked as safe here as it will be copied anyway in the
    // common "setKeyValue" method when prefixing the string.
    setKeyValue(object, key, rj::Value(rj::StringRef(value.data())));
}

void NumberAsStringParserDocument::setKeyValue(rj::Value& object, const rj::Pointer& key, int value)
{
    setKeyValue(object, key, rj::Value(value));
}

void NumberAsStringParserDocument::setKeyValue(rj::Value& object, const rj::Pointer& key, unsigned int value)
{
    setKeyValue(object, key, rj::Value(value));
}

void NumberAsStringParserDocument::setKeyValue(rj::Value& object, const rj::Pointer& key, int64_t value)
{
    setKeyValue(object, key, rj::Value(value));
}

void NumberAsStringParserDocument::setKeyValue(rj::Value& object, const rj::Pointer& key, uint64_t value)
{
    setKeyValue(object, key, rj::Value(value));
}

void NumberAsStringParserDocument::setKeyValue(rj::Value& object, const rj::Pointer& key, double value)
{
    setKeyValue(object, key, rj::Value(value));
}

void NumberAsStringParserDocument::setValue(const rj::Pointer& pointerToEntry, const rj::Value& value)
{
    setKeyValue(*this, pointerToEntry, value);
}

void NumberAsStringParserDocument::setValue(const rj::Pointer& pointerToEntry, const std::string_view& value)
{
    // The passed string may be marked as safe here as it will be copied anyway in the
    // common "setKeyValue" method when prefixing the string.
    setKeyValue(*this, pointerToEntry, rj::Value(rj::StringRef(value.data())));
}

void NumberAsStringParserDocument::setValue(const rj::Pointer& pointerToEntry, int value)
{
    setKeyValue(*this, pointerToEntry, rj::Value(value));
}

void NumberAsStringParserDocument::setValue(const rj::Pointer& pointerToEntry, unsigned int value)
{
    setKeyValue(*this, pointerToEntry, rj::Value(value));
}

void NumberAsStringParserDocument::setValue(const rj::Pointer& pointerToEntry, int64_t value)
{
    setKeyValue(*this, pointerToEntry, rj::Value(value));
}

void NumberAsStringParserDocument::setValue(const rj::Pointer& pointerToEntry, uint64_t value)
{
    setKeyValue(*this, pointerToEntry, rj::Value(value));
}

void NumberAsStringParserDocument::setValue(const rj::Pointer& pointerToEntry, double value)
{
    setKeyValue(*this, pointerToEntry, rj::Value(value));
}

std::optional<std::string_view>
NumberAsStringParserDocument::getOptionalStringValue(const rj::Value& object, const rj::Pointer& key) const
{
    const auto* pointerValue = key.Get(object);
    if (pointerValue && pointerValue->IsString())
    {
        ModelExpect(pointerValue->GetStringLength() > 0,
            pointerToString(key) + ": at least prefix character expected");
        ModelExpect(pointerValue->GetString()[0] == prefixNumber || pointerValue->GetString()[0] == prefixString,
            pointerToString(key) + ": string values must start with prefix character");
        ModelExpect(pointerValue->GetString()[0] == prefixString,
            pointerToString(key) + ": entry is not a string");
        return &pointerValue->GetString()[1];
    }
    return {};
}

std::optional<std::string_view>
NumberAsStringParserDocument::getNumericAsString(const rapidjson::Pointer& key) const
{
    const auto* pointerValue = key.Get(*this);
    if (pointerValue && pointerValue->IsString())
    {
        ModelExpect(pointerValue->GetStringLength() > 0,
                    pointerToString(key) + ": at least prefix character expected");
        ModelExpect(pointerValue->GetString()[0] == prefixNumber || pointerValue->GetString()[0] == prefixString,
                    pointerToString(key) + ": string values must start with prefix character");
        ModelExpect(pointerValue->GetString()[0] == prefixNumber,
                    pointerToString(key) + ": entry is not a numeric type");
        return &pointerValue->GetString()[1];
    }
    return {};
}

std::optional<int>
NumberAsStringParserDocument::getOptionalIntValue(const rj::Value& object, const rj::Pointer& key) const
{
    const auto* pointerValue = key.Get(object);
    if (pointerValue && pointerValue->IsString())
    {
        ModelExpect(pointerValue->GetStringLength() > 0,
            pointerToString(key) + ": at least prefix character expected");
        ModelExpect(pointerValue->GetString()[0] == prefixNumber,
            pointerToString(key) + ": entry is not a number");
        try
        {
            std::size_t pos = 0;
            int val = std::stoi(&pointerValue->GetString()[1], &pos, 10);
            if (pos != (pointerValue->GetStringLength() - 1))
            {
                ModelFail("Failed to convert " + pointerToString(key) + " to int");
            }
            return val;
        }
        catch (const std::invalid_argument&)
        {
            ModelFail("Failed to convert " + pointerToString(key) + " to int");
        }
        catch (const std::out_of_range&)
        {
            ModelFail("Failed to convert " + pointerToString(key) + " to int");
        }
    }
    return {};
}

std::optional<unsigned int>
NumberAsStringParserDocument::getOptionalUIntValue(const rj::Value& object, const rj::Pointer& key) const
{
    const auto* pointerValue = key.Get(object);
    if (pointerValue && pointerValue->IsString())
    {
        ModelExpect(pointerValue->GetStringLength() > 0,
            pointerToString(key) + ": at least prefix character expected");
        ModelExpect(pointerValue->GetString()[0] == prefixNumber,
            pointerToString(key) + ": entry is not a number");
        try
        {
            std::size_t pos = 0;
            unsigned long val = std::stoul(&pointerValue->GetString()[1], &pos, 10);
            if (pos != (pointerValue->GetStringLength() - 1)
                || val < std::numeric_limits<unsigned int>::min()
                || val > std::numeric_limits<unsigned int>::max())
            {
                ModelFail("Failed to convert " + pointerToString(key) + " to unsigned int");
            }
            return val;
        }
        catch (const std::invalid_argument&)
        {
            ModelFail("Failed to convert " + pointerToString(key) + " to unsigned int");
        }
        catch (const std::out_of_range&)
        {
            ModelFail("Failed to convert " + pointerToString(key) + " to unsigned int");
        }
    }
    return {};
}

std::optional<int64_t>
NumberAsStringParserDocument::getOptionalInt64Value(const rj::Value& object, const rj::Pointer& key) const
{
    const auto* pointerValue = key.Get(object);
    if (pointerValue && pointerValue->IsString())
    {
        ModelExpect(pointerValue->GetStringLength() > 0,
            pointerToString(key) + ": at least prefix character expected");
        ModelExpect(pointerValue->GetString()[0] == prefixNumber,
            pointerToString(key) + ": entry is not a number");
        try
        {
            std::size_t pos = 0;
            int64_t val = std::stoll(&pointerValue->GetString()[1], &pos, 10);
            if (pos != (pointerValue->GetStringLength() - 1))
            {
                ModelFail("Failed to convert " + pointerToString(key) + " to int64");
            }
            return val;
        }
        catch (const std::invalid_argument&)
        {
            ModelFail("Failed to convert " + pointerToString(key) + " to int64");
        }
        catch (const std::out_of_range&)
        {
            ModelFail("Failed to convert " + pointerToString(key) + " to int64");
        }
    }
    return {};
}

std::optional<uint64_t>
NumberAsStringParserDocument::getOptionalUInt64Value(const rj::Value& object, const rj::Pointer& key) const
{
    const auto* pointerValue = key.Get(object);
    if (pointerValue && pointerValue->IsString())
    {
        ModelExpect(pointerValue->GetStringLength() > 0,
            pointerToString(key) + ": at least prefix character expected");
        ModelExpect(pointerValue->GetString()[0] == prefixNumber,
            pointerToString(key) + ": entry is not a number");
        try
        {
            std::size_t pos = 0;
            uint64_t val = std::stoull(&pointerValue->GetString()[1], &pos, 10);
            if (pos != (pointerValue->GetStringLength() - 1))
            {
                ModelFail("Failed to convert " + pointerToString(key) + " to unsigned int64");
            }
            return val;
        }
        catch (const std::invalid_argument&)
        {
            ModelFail("Failed to convert " + pointerToString(key) + " to unsigned int64");
        }
        catch (const std::out_of_range&)
        {
            ModelFail("Failed to convert " + pointerToString(key) + " to unsigned int64");
        }
    }
    return {};
}

std::optional<double>
NumberAsStringParserDocument::getOptionalDoubleValue(const rj::Value& object, const rj::Pointer& key) const
{
    const auto* pointerValue = key.Get(object);
    if (pointerValue && pointerValue->IsString())
    {
        ModelExpect(pointerValue->GetStringLength() > 0,
            pointerToString(key) + ": at least prefix character expected");
        ModelExpect(pointerValue->GetString()[0] == prefixNumber,
            pointerToString(key) + ": entry is not a number");
        try
        {
            return std::stod(&pointerValue->GetString()[1]);
        }
        catch (const std::invalid_argument&)
        {
            ModelFail("Failed to convert " + pointerToString(key) + " to double");
        }
        catch (const std::out_of_range&)
        {
            ModelFail("Failed to convert " + pointerToString(key) + " to double");
        }
    }
    return {};
}

std::optional<std::string_view>
NumberAsStringParserDocument::getOptionalStringValue(const rj::Pointer& pointerToEntry) const
{
    return getOptionalStringValue(*this, pointerToEntry);
}

std::optional<int>
NumberAsStringParserDocument::getOptionalIntValue(const rj::Pointer& pointerToEntry) const
{
    return getOptionalIntValue(*this, pointerToEntry);
}

std::optional<unsigned int>
NumberAsStringParserDocument::getOptionalUIntValue(const rj::Pointer& pointerToEntry) const
{
    return getOptionalUIntValue(*this, pointerToEntry);
}

std::optional<int64_t>
NumberAsStringParserDocument::getOptionalInt64Value(const rj::Pointer& pointerToEntry) const
{
    return getOptionalInt64Value(*this, pointerToEntry);
}

std::optional<uint64_t>
NumberAsStringParserDocument::getOptionalUInt64Value(const rj::Pointer& pointerToEntry) const
{
    return getOptionalUInt64Value(*this, pointerToEntry);
}

std::optional<double>
NumberAsStringParserDocument::getOptionalDoubleValue(const rj::Pointer& pointerToEntry) const
{
    return getOptionalDoubleValue(*this, pointerToEntry);
}

std::optional<std::string_view> NumberAsStringParserDocument::findStringInArray(
    const rj::Pointer& arrayPointer,
    const rj::Pointer& searchKey,
    const std::string_view& searchValue,
    const rj::Pointer& resultKeyPointer,
    bool ignoreValueCase) const
{
    const auto memberAndPosition = findMemberInArray(arrayPointer, searchKey, searchValue, resultKeyPointer, ignoreValueCase);
    if (memberAndPosition.has_value())
    {
        const auto *const member = std::get<0>(memberAndPosition.value());
        if (member != nullptr && member->IsString())
        {
            return NumberAsStringParserDocument::getStringValueFromValue(member);
        }
    }
    return {};
}

std::optional<std::tuple<const rj::Value*, std::size_t>> NumberAsStringParserDocument::findMemberInArray(
    const rj::Pointer& arrayPointer,
    const rj::Pointer& searchKey,
    const std::string_view& searchValue,
    const rj::Pointer& resultKeyPointer,
    bool ignoreValueCase) const
{
    std::optional<std::tuple<const rj::Value*, std::size_t>> ret;
    const auto* array = arrayPointer.Get(*this);
    if (array != nullptr && array->IsArray())
    {
        // TODO: Class ResourceBase still needs access to the prefixes.
        // Some helper methods are still missing in class NumberAsStringParserDocument.
        // See Jira Task : ....
        std::string prefixedSearchValue = NumberAsStringParserDocument::prefixString + std::string(searchValue);
        for (auto item = array->Begin(), end = array->End(); item != end; ++item)
        {
            const auto* member = searchKey.Get(*item);
            if (member != nullptr && member->IsString() &&
                ((!ignoreValueCase && prefixedSearchValue == member->GetString()) ||
                  (ignoreValueCase && ::String::toLower(prefixedSearchValue) == ::String::toLower(member->GetString()))))
            {
                member = resultKeyPointer.Get(*item);
                ModelExpect(!ret, "duplicate array entry for " + std::string(searchValue));
                ret = { { member, std::distance(array->Begin(), item) } };
            }
        }
    }
    return ret;
}

rj::Value* NumberAsStringParserDocument::findMemberInArray(
    const rj::Pointer& arrayPointer,
    const rj::Pointer& searchKey,
    const std::string_view& searchValue)
{
    return const_cast<rj::Value*>(
       static_cast<const NumberAsStringParserDocument*>(this)->findMemberInArray(arrayPointer, searchKey, searchValue));
}

const rj::Value* NumberAsStringParserDocument::findMemberInArray(
    const rj::Pointer& arrayPointer,
    const rj::Pointer& searchKey,
    const std::string_view& searchValue) const
{
    const auto* array = arrayPointer.Get(*this);
    const rapidjson::Value* foundItem = nullptr;
    if (array != nullptr && array->IsArray())
    {
        const std::string prefixedSearchValue = NumberAsStringParserDocument::prefixString + std::string(searchValue);
        for (auto item = array->Begin(), end = array->End(); item != end; ++item)
        {
            const auto* member = searchKey.Get(*item);
            if (member != nullptr && member->IsString() && prefixedSearchValue == member->GetString())
            {
                ModelExpect(!foundItem, "duplicate array entry for " + std::string(searchValue));
                foundItem = item;
            }
        }
    }
    return foundItem;
}

const rj::Value* NumberAsStringParserDocument::getMemberInArray(const rj::Pointer& pointerToArray, size_t index) const
{
    const auto* array = pointerToArray.Get(*this);
    if (array == nullptr || array->Size() <= index)
    {
        return nullptr;
    }
    return &(*array)[static_cast<rj::SizeType>(index)];
}

std::size_t NumberAsStringParserDocument::addToArray(const rj::Pointer& pointerToArray, rj::Value&& object)
{
    rj::Value* array = pointerToArray.Get(*this);
    if (array == nullptr)
    {
        array = &pointerToArray.Create(*this).SetArray();
    }
    ModelExpect(array->IsArray(), pointerToString(pointerToArray) + " must be array");
    array->PushBack(std::move(object), GetAllocator());
    return array->Size() - 1;
}

std::size_t NumberAsStringParserDocument::addToArray(rj::Value& array, rj::Value&& object)
{
    ModelExpect(array.IsArray(), "Element must be array");
    array.PushBack(std::move(object), GetAllocator());
    return array.Size() - 1;
}

void NumberAsStringParserDocument::addMemberToArrayEntry(const ::rapidjson::Pointer& pointerToArray,
                                                         ::std::size_t index, ::rapidjson::Value&& key,
                                                         ::rapidjson::Value&& value)
{
    auto* array = pointerToArray.Get(*this);
    ModelExpect(array && array->IsArray(), "array not found");
    ModelExpect(array->Size() > index, "array index invalid");

    auto& arrayEntry = (*array)[static_cast<::rapidjson::SizeType>(index)];
    if (auto entry = arrayEntry.FindMember(key); entry != arrayEntry.MemberEnd())
    {
        entry->value = value;
    }
    else
    {
        arrayEntry.AddMember(key, value, GetAllocator());
    }
}

void NumberAsStringParserDocument::removeFromArray(const rj::Pointer& pointerToArray, std::size_t index)
{
    rj::Value* array = pointerToArray.Get(*this);
    ModelExpect(array != nullptr && array->IsArray() && array->Size() > index,
        pointerToString(pointerToArray) + ": entry not present or no array");
    ModelExpect(array->Size() > index, pointerToString(pointerToArray) + ": array has not enough elements");
    array->Erase(array->Begin() + index);
}

void NumberAsStringParserDocument::clearArray(const rj::Pointer& pointerToArray)
{
    rj::Value* array = pointerToArray.Get(*this);
    ModelExpect(array != nullptr && array->IsArray(),
        pointerToString(pointerToArray) + ": entry not present or no array");
    array->Clear();
}

rj::Value model::NumberAsStringParserDocument::makeValue(Ch prefix, const std::string_view& value)
{
    std::string s;
    s.reserve(value.size() + 1);
    s.push_back(prefix);
    s.append(value);
    return rj::Value{s, GetAllocator()};
}

rj::Document NumberAsStringParserDocumentConverter::copyToOriginalFormat(const rj::Document& documentNumbersAsStrings)
{
    // TODO: Long, slow way. Should be speeded up be recusively walking through the dom tree.
    rj::StringBuffer buffer;
    NumberAsStringParserWriter<rj::StringBuffer> writer(buffer);
    documentNumbersAsStrings.Accept(writer);
    rj::Document document;
    document.Parse(buffer.GetString());
    ModelExpect(!document.HasParseError(), "can not parse json string");
    return document;
}

rj::Document& NumberAsStringParserDocumentConverter::convertToNumbersAsStrings(rj::Document& document)
{
    // TODO: Long, slow way. Should be speeded up be recusively walking through the dom tree.
    rj::StringBuffer buffer;
    rj::Writer<rj::StringBuffer> writer(buffer);
    document.Accept(writer);
    document.SetObject();
    rj::StringStream s(buffer.GetString());
    document.ParseStream<rj::kParseNumbersAsStringsFlag, rj::CustomUtf8>(s);
    return document;
}
