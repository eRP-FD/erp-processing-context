/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/enrolment/EnrolmentModel.hxx"

#include "shared/util/Base64.hxx"
#include "shared/util/TLog.hxx"

#include <rapidjson/error/en.h>
#include <rapidjson/pointer.h>
#include <rapidjson/writer.h>


namespace
{
    std::vector<size_t> convertToSizeTVector (const rapidjson::Value& value)
    {
        ErpExpect(value.IsArray(), HttpStatus::BadRequest, "value is not an array");
        const auto array = value.GetArray();
        std::vector<size_t> result;
        result.reserve(array.Size());
        for (const rapidjson::Value& item : array)
        {
            ErpExpect(item.IsInt64(), HttpStatus::BadRequest, "array item is not an int64");
            result.emplace_back(item.GetInt64());
        }
        return result;
    }
}


void EnrolmentModel::set (std::string_view key, std::string_view stringValue)
{
    const auto pointer = rapidjson::Pointer(rapidjson::StringRef(key.data(), key.size()));
    pointer.Set(
        mDocument,
        rapidjson::Value(rapidjson::StringRef(stringValue.data(), stringValue.size()), mDocument.GetAllocator()),
        mDocument.GetAllocator());
}


void EnrolmentModel::set (std::string_view key, std::int64_t numberValue)
{
    const auto pointer = rapidjson::Pointer(rapidjson::StringRef(key.data(), key.size()));
    pointer.Set(
            mDocument,
            rapidjson::Value(numberValue),
            mDocument.GetAllocator());
}

void EnrolmentModel::set(std::string_view key, const rapidjson::Value& value)
{
    const auto pointer = rapidjson::Pointer(rapidjson::StringRef(key.data(), key.size()));
    pointer.Set(mDocument, value, mDocument.GetAllocator());
}

bool EnrolmentModel::hasValue(std::string_view key) const
{
    const auto pointer = ::rapidjson::Pointer(::rapidjson::StringRef(key.data(), key.size()));
    return (pointer.Get(mDocument) != nullptr);
}

const rapidjson::Value& EnrolmentModel::getValue (std::string_view key) const
{
    const auto pointer = rapidjson::Pointer(rapidjson::StringRef(key.data(), key.size()));
    const auto* value = pointer.Get(mDocument);
    ErpExpect(value!=nullptr, HttpStatus::BadRequest, "value for key '" + std::string(key) + "' is missing");
    return *value;
}


const rapidjson::Value* EnrolmentModel::getOptionalValue (std::string_view key) const
{
    const auto pointer = rapidjson::Pointer(rapidjson::StringRef(key.data(), key.size()));
    return pointer.Get(mDocument);
}


std::string EnrolmentModel::getString (std::string_view key) const
{
    const auto& value = getValue(key);
    ErpExpect(value.IsString(), HttpStatus::BadRequest, "value is not a string");
    return {value.GetString()};
}

std::optional<std::string> EnrolmentModel::getOptionalString(std::string_view key) const
{
    std::optional<std::string> retVal;
    const auto* value = getOptionalValue(key);
    if (value)
    {
        ErpExpect(value->IsString(), HttpStatus::BadRequest, "value is not a string");
        retVal = value->GetString();
    }

    return retVal;
}


std::optional<std::string> EnrolmentModel::getOptionalDecodedString (std::string_view key) const
{
    const auto* value = getOptionalValue(key);
    if (value == nullptr)
        return std::nullopt;
    else
    {
        // The presence of the value is optional. But if it is there, its type has to be string.
        ErpExpect(value->IsString(), HttpStatus::BadRequest, "value is not a string");
        return Base64::decodeToString(std::string_view(value->GetString(), value->GetStringLength()));
    }
}


std::string EnrolmentModel::getDecodedString (std::string_view key) const
{
    const auto& value = getValue(key);
    ErpExpect(value.IsString(), HttpStatus::BadRequest, "value is not a string");
    return Base64::decodeToString(std::string_view(value.GetString(), value.GetStringLength()));
}


SafeString EnrolmentModel::getSafeString (std::string_view key) const
{
    const auto& value = getValue(key);
    ErpExpect(value.IsString(), HttpStatus::BadRequest, "value is not a string");
    return SafeString(value.GetString());
}


int64_t EnrolmentModel::getInt64 (std::string_view key) const
{
    const auto& value = getValue(key);
    ErpExpect(value.IsInt64(), HttpStatus::BadRequest, "value is not an int64");
    return value.GetInt64();
}

::std::optional<int64_t> EnrolmentModel::getOptionalInt64(::std::string_view key) const
{
    const auto* value = getOptionalValue(key);
    if (value)
    {
        ErpExpect(value->IsInt64(), ::HttpStatus::BadRequest, "value is not an int64");
        return value->GetInt64();
    }

    return {};
}


std::vector<size_t> EnrolmentModel::getSizeTVector (std::string_view key) const
{
    const auto& value = getValue(key);
    return convertToSizeTVector(value);
}


std::optional<std::vector<size_t>> EnrolmentModel::getOptionalSizeTVector (std::string_view key) const
{
    const auto* value = getOptionalValue(key);
    if (value == nullptr)
        return std::nullopt;
    else
        return convertToSizeTVector(*value);
}


ErpVector EnrolmentModel::getErpVector (std::string_view key) const
{
    const auto& value = getValue(key);
    ErpExpect(value.IsString(), HttpStatus::BadRequest, "value is not a string");
    const auto* data = reinterpret_cast<const uint8_t*>(value.GetString());
    return {data, data + value.GetStringLength()};
}


ErpVector EnrolmentModel::getDecodedErpVector (std::string_view key) const
{
    const auto& value = getValue(key);
    ErpExpect(value.IsString(), HttpStatus::BadRequest, "value is not a string");
    return ErpVector::create(Base64::decodeToString(std::string_view(value.GetString(), value.GetStringLength())));
}


EnrolmentModel::EnrolmentModel (void)
    : mDocument()
{
}


EnrolmentModel::EnrolmentModel (const std::string_view jsonText)
    : mDocument()
{
    parse(jsonText);
}

void EnrolmentModel::parse(const std::string_view jsonText)
{
    rapidjson::MemoryStream s(jsonText.data(), jsonText.size());
    rapidjson::ParseResult result = mDocument.ParseStream(s);
    if (mDocument.HasParseError())
    {
        TVLOG(1) << "JSON parser error: " << rapidjson::GetParseError_En(mDocument.GetParseError()) << " @ " << result.Offset();
        TVLOG(1) << "json text is '" << jsonText << "'";
        ErpExpect( ! mDocument.HasParseError(), HttpStatus::BadRequest, "text is not valid JSON");
    }
}

std::string EnrolmentModel::serializeToString (void) const
{
    return serializeToString("");
}

std::string EnrolmentModel::serializeToString(std::string_view key) const
{
    const auto* value = rapidjson::Pointer(rapidjson::StringRef(key.data(), key.size())).Get(mDocument);
    ErpExpect(value != nullptr, HttpStatus::BadRequest, "missing key " + std::string{key});
    if (value->IsNull())
        return "";
    rapidjson::StringBuffer buffer;
    buffer.Clear();

    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    value->Accept(writer);

    return {buffer.GetString()};
}

rapidjson::Document& EnrolmentModel::document()
{
    return mDocument;
}
