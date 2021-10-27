/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/CommunicationPayloadItem.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/Expect.hxx"

#include "erp/util/String.hxx"

#include <map>

using namespace model;
using namespace model::resource;
using namespace rapidjson;

static const std::string invalidDataMessage = "The data does not conform to that expected by the FHIR profile or is invalid";

const std::map<CommunicationPayloadItem::Type, std::string> PayloadItemTypeToString = {
    { CommunicationPayloadItem::Type::String,     "String"     },
    { CommunicationPayloadItem::Type::Attachment, "Attachment" },
    { CommunicationPayloadItem::Type::Reference,  "Reference"  }
};

const std::string& CommunicationPayloadItem::typeToString(Type type)
{
    const auto& it = PayloadItemTypeToString.find(type);
    Expect3(it != PayloadItemTypeToString.end(),
        "Payload item type enumerator " + std::to_string(static_cast<int>(type)) + " is out of range", std::logic_error);
    return it->second;
}

const std::vector<CommunicationPayloadItem::Type> PayloadItemTypes = {
    CommunicationPayloadItem::Type::String,
    CommunicationPayloadItem::Type::Attachment,
    CommunicationPayloadItem::Type::Reference
};

const std::vector<CommunicationPayloadItem::Type>& CommunicationPayloadItem::types()
{
    return PayloadItemTypes;
}

CommunicationPayloadItem::CommunicationPayloadItem(Type type, const Value* value) :
    mType(type),
    mLength(0)
{
    ModelExpect(value != nullptr, "CommunicationPayload::ctor(value==nullptr)");
}

const std::string& CommunicationPayloadItem::typeAsString() const
{
    return typeToString(mType);
}

CommunicationPayloadItemString::CommunicationPayloadItemString(const Value* value) :
    CommunicationPayloadItem(Type::String, value)
{
    mLength = NumberAsStringParserDocument::getStringValueFromValue(value).length();
}

CommunicationPayloadItemAttachment::CommunicationPayloadItemAttachment(const Value* value) :
    CommunicationPayloadItem(Type::Attachment, value),
    mMimeType(),
    mData()
{
    ModelExpect(value->IsObject(), invalidDataMessage + " (payload.contentAttachment has invalid format).");
    const auto& object = value->GetObject();

    ModelExpect(object.HasMember(elements::data), invalidDataMessage + " (payload.contentAttachment has invalid format).");
    const auto& data = object.FindMember(elements::data);

    mData = NumberAsStringParserDocument::getStringValueFromValue(&data->value);
    mLength = mData.length();

    ModelExpect(object.HasMember(elements::contentType), invalidDataMessage + " (payload.contentAttachment has invalid format).");
    const auto& contentType = object.FindMember(elements::contentType);

    mMimeType = String::toLower(std::string(NumberAsStringParserDocument::getStringValueFromValue(&contentType->value)));
}

void CommunicationPayloadItemAttachment::verifyUrls() const
{
    ModelExpect(
        !((mData.find("http://") != std::string::npos)
        || (mData.find("https://") != std::string::npos)
        || (mData.find("ftp://") != std::string::npos)),
        "References to external URLs are not allowed.");
}

void CommunicationPayloadItemAttachment::verifyMimeType() const
{
    ModelExpect(!String::starts_with(mMimeType, "application"),
                "Attachments with mime type application are not allowed.");
}

CommunicationPayloadItemReference::CommunicationPayloadItemReference(const rapidjson::Value* value) :
    CommunicationPayloadItem(Type::Reference, value),
    mReference()
{
    ModelExpect(value->IsObject(), invalidDataMessage + " (payload.contentReference has invalid format).");

    const auto& object = value->GetObject();

    ModelExpect(object.HasMember(elements::reference), invalidDataMessage + " (payload.contentAttachment has invalid format).");
    const auto& reference = object.FindMember(elements::reference);

    mReference = String::toLower(std::string(NumberAsStringParserDocument::getStringValueFromValue(&reference->value)));
    mLength = mReference.length();
}

void CommunicationPayloadItemReference::verifyUrls() const
{
    ModelExpect(
        !((mReference.find("http://") != std::string::npos)
        || (mReference.find("https://") != std::string::npos)
        || (mReference.find("ftp://") != std::string::npos)),
        "References to external URLs are not allowed.");
}
