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

