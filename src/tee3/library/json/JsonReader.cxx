/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/json/JsonReader.hxx"
#include "library/util/Assert.hxx"
#include "library/wrappers/RapidJson.hxx"

#include <rapidjson/schema.h>


#define AssertJson(expression) Assert(expression) << assertion::exceptionType<JsonError>()

namespace epa
{

JsonReader::JsonReader(std::string_view json)
{
    if (BinaryDocument::isBinaryDocument(json))
    {
        mBinaryDocument = std::make_unique<BinaryDocument>();
        mBinaryDocument->decode(json);
        mBinValue = mBinaryDocument->getRoot();
        return;
    }

    mDocument = std::make_unique<rapidjson::Document>();
    mDocument->Parse(json.data(), json.length());

    AssertJson(! mDocument->HasParseError()) << "JSON string could not be parsed, error: "
                                             << GetParseError_En(mDocument->GetParseError())
                                             << ", JSON string: " << logging::sensitive(json);

    // The root value is the document itself.
    mValue = mDocument.get();
}


JsonReader::~JsonReader() noexcept = default;


void JsonReader::validateSchema(std::string_view schema) const
{
    Assert(! mBinaryDocument) << "No schema support for binary document";
    AssertJson(mValue != nullptr) << "Cannot validate invalid/null JSON node";

    rapidjson::Document document;
    document.Parse(schema.data(), schema.length());
    AssertJson(! document.HasParseError())
        << "Invalid JSON schema, error: " << GetParseError_En(document.GetParseError())
        << ", schema string: " << logging::development(schema);
    const rapidjson::SchemaDocument schemaDocument{document};
    rapidjson::SchemaValidator validator{schemaDocument};
    AssertJson(mValue->Accept(validator)) << "JSON document does not conform to required schema";
}


// NOLINTNEXTLINE(misc-no-recursion) - there can't be any cycles as mParent is const
std::string JsonReader::getPath() const
{
    if (mParent == nullptr)
        return "$";

    std::string result = mParent->getPath();

    if (! mKey.empty())
    {
        result += ".";
        result += mKey;
        return result;
    }

    // This can't really happen: Both constructors for nested JsonReaders check mParent->mValue.
    if (mParent->mValue == nullptr && mParent->mBinValue == nullptr)
        return result += "<error>";

    // If we get here, we need to use mIndex. But is it an array index, or an object member index?
    // Check the type of mParent to find out:
    if (mParent->mValue)
    {
        if (mParent->mValue->IsObject())
            result += ".*~";
    }
    else
    {
        if (mParent->mBinValue->type == BinaryDocument::Type::Object)
            result += ".*~";
    }

    // Note: The key syntax seems to be a bit arcane. https://stackoverflow.com/a/52290672

    result += "[";
    result += std::to_string(mIndex);
    result += "]";
    return result;
}


std::string JsonReader::toJsonString() const
{
    Assert(! mBinValue) << "Binary values/documents can not be converted to JSON strings";

    if (! mValue)
        return "null";

    rapidjson::StringBuffer buffer;
    rapidjson::Writer writer{buffer};
    mValue->Accept(writer);
    return std::string{buffer.GetString(), buffer.GetLength()};
}


std::string JsonReader::stringValue() const
{
    return std::string{view()};
}


std::string_view JsonReader::view() const
{
    if (mBinValue)
    {
        Assert(mBinValue->type == BinaryDocument::Type::String);
        return dynamic_cast<const BinaryDocument::String&>(*mBinValue).value;
    }
    AssertJson(mValue && mValue->IsString()) << "Expected string at path: " << getPath();
    return {mValue->GetString(), mValue->GetStringLength()};
}


std::int64_t JsonReader::int64Value() const
{
    if (mBinValue)
    {
        Assert(mBinValue->type == BinaryDocument::Type::Int64);
        return dynamic_cast<const BinaryDocument::Int64&>(*mBinValue).value;
    }
    AssertJson(mValue && mValue->IsInt64()) << "Expected integer at path: " << getPath();
    return mValue->GetInt64();
}


std::uint64_t JsonReader::uint64Value() const
{
    if (mBinValue)
    {
        Assert(mBinValue->type == BinaryDocument::Type::Int64);
        return static_cast<std::uint64_t>(
            dynamic_cast<const BinaryDocument::Int64&>(*mBinValue).value);
    }
    AssertJson(mValue && mValue->IsUint64()) << "Expected unsigned integer at path: " << getPath();
    return mValue->GetUint64();
}


double JsonReader::doubleValue() const
{
    if (mBinValue)
    {
        Assert(mBinValue->type == BinaryDocument::Type::Double);
        return dynamic_cast<const BinaryDocument::Double&>(*mBinValue).value;
    }
    AssertJson(mValue && mValue->IsDouble()) << "Expected unsigned integer at path: " << getPath();
    return mValue->GetDouble();
}


bool JsonReader::boolValue() const
{
    if (mBinValue)
    {
        Assert(mBinValue->type == BinaryDocument::Type::Bool);
        return dynamic_cast<const BinaryDocument::Bool&>(*mBinValue).value;
    }
    AssertJson(mValue && mValue->IsBool()) << "Expected boolean value at path: " << getPath();
    return mValue->GetBool();
}


bool JsonReader::hasMember(std::string_view key) const
{
    if (mBinValue)
    {
        Assert(mBinValue->type == BinaryDocument::Type::Object);
        return dynamic_cast<const BinaryDocument::Object&>(*mBinValue).elements.contains(key);
    }

    AssertJson(mValue && mValue->IsObject()) << "Expected object at path: " << getPath();

    const rapidjson::Value jsonKey{key.data(), static_cast<rapidjson::SizeType>(key.length())};
    const auto iterator = mValue->FindMember(jsonKey);
    return iterator != mValue->MemberEnd() && ! iterator->value.IsNull();
}


bool JsonReader::isStringValue() const
{
    if (mBinValue)
        return mBinValue->type == BinaryDocument::Type::String;

    return mValue && mValue->IsString();
}


bool JsonReader::isArrayValue() const
{
    if (mBinValue)
        return mBinValue->type == BinaryDocument::Type::Array;

    return mValue && mValue->IsArray();
}


JsonReader::JsonReader(const JsonReader& parent, std::string_view key)
  : mParent{&parent},
    mKey{key}
{
    if (parent.mBinValue)
    {
        Assert(parent.mBinValue->type == BinaryDocument::Type::Object);
        const auto& elements =
            dynamic_cast<const BinaryDocument::Object&>(*parent.mBinValue).elements;
        auto it = elements.find(key);
        if (it != elements.end())
            mBinValue = it->second.get();
        return;
    }

    AssertJson(parent.mValue) << "Path " << getPath() << " not found; parent is missing";
    AssertJson(parent.mValue->IsObject()) << "Expected object at path: " << getPath();

    const rapidjson::Value jsonKey{key.data(), static_cast<rapidjson::SizeType>(key.length())};
    const auto iterator = parent.mValue->FindMember(jsonKey);
    if (iterator != parent.mValue->MemberEnd())
        mValue = &iterator->value;
}


JsonReader::JsonReader(const JsonReader& parent, std::size_t index)
  : mParent{&parent},
    mIndex{index}
{
    if (parent.mBinValue)
    {
        if (parent.mBinValue->type == BinaryDocument::Type::Array)
        {
            const auto& elements =
                dynamic_cast<const BinaryDocument::Array&>(*parent.mBinValue).elements;
            AssertJson(index < elements.size());
            mBinValue = elements.at(index).get();
        }
        else
        {
            Assert(parent.mBinValue->type == BinaryDocument::Type::Object);
            const auto& elements =
                dynamic_cast<const BinaryDocument::Object&>(*parent.mBinValue).elements;
            auto iterator = elements.begin();
            for (;;)
            {
                Assert(iterator != elements.end());
                if (index == 0)
                    break;
                ++iterator;
                index--;
            }
            // Oh they want to read the key, not the value :-(
            //mBinValue = iterator->second.get();
            std::string key = iterator->first;
            mBinaryDocument = std::make_unique<BinaryDocument>();
            mBinaryDocument->addString(std::move(key));
            mBinValue = mBinaryDocument->getRoot();
        }
        return;
    }

    AssertJson(parent.mValue) << "Path " << getPath() << " not found; parent is missing";

    if (parent.mValue->IsArray())
    {
        AssertJson(index < parent.mValue->GetArray().Size())
            << "Array index out of bounds: " << getPath();
        mValue = &parent.mValue->GetArray()[static_cast<rapidjson::SizeType>(index)];
    }
    else
    {
        AssertJson(parent.mValue->IsObject())
            << "Expected a container at path: " + parent.getPath();
        AssertJson(index < parent.mValue->MemberCount())
            << "Object key index out of bounds: " << getPath();
        auto iterator = parent.mValue->GetObject().MemberBegin();
        iterator += static_cast<rapidjson::Value::MemberIterator::DifferenceType>(index);
        mValue = &iterator->name;
    }
}


std::size_t JsonReader::arraySize() const
{
    if (mBinValue)
    {
        Assert(mBinValue->type == BinaryDocument::Type::Array);
        return dynamic_cast<const BinaryDocument::Array&>(*mBinValue).elements.size();
    }

    AssertJson(mValue && mValue->IsArray()) << "Expected an array at path: " << getPath();
    return mValue->GetArray().Size();
}


std::size_t JsonReader::objectSize() const
{
    if (mBinValue)
    {
        Assert(mBinValue->type == BinaryDocument::Type::Object);
        return dynamic_cast<const BinaryDocument::Object&>(*mBinValue).elements.size();
    }

    AssertJson(mValue && mValue->IsObject()) << "Expected a object at path: " << getPath();
    return mValue->MemberCount();
}

} // namespace epa
