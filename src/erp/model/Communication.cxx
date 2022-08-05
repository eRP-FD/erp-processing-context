/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Communication.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/UrlHelper.hxx"
#include "erp/util/Uuid.hxx"
#include "erp/util/String.hxx"

#include <rapidjson/writer.h>
#include <map>
#include <regex>
#include <tuple>

using namespace model;
using namespace model::resource;
using fhirtools::Timestamp;

namespace rj = rapidjson;

const rj::Pointer Communication::resourceTypePointer(ElementName::path(elements::resourceType));
const rj::Pointer Communication::idPointer(ElementName::path(elements::id));
const rj::Pointer Communication::metaProfile0Pointer(ElementName::path(elements::meta, elements::profile, 0));
const rj::Pointer Communication::basedOn0ReferencePointer(ElementName::path(elements::basedOn, 0, elements::reference));
const rj::Pointer Communication::sentPointer(ElementName::path(elements::sent));
const rj::Pointer Communication::receivedPointer(ElementName::path(elements::received));
const rj::Pointer Communication::senderIdentifierSystemPointer(ElementName::path(elements::sender, elements::identifier, elements::system));
const rj::Pointer Communication::senderIdentifierValuePointer(ElementName::path(elements::sender, elements::identifier, elements::value));
const rj::Pointer Communication::recipientPointer(ElementName::path(elements::recipient));
const rj::Pointer Communication::recipient0IdentifierSystemPointer(ElementName::path(elements::recipient, 0, elements::identifier, elements::system));
const rj::Pointer Communication::recipient0IdentifierValuePointer(ElementName::path(elements::recipient, 0, elements::identifier, elements::value));
const rj::Pointer Communication::payloadPointer(ElementName::path(elements::payload));

const rj::Pointer Communication::identifierSystemPointer(ElementName::path(elements::identifier, elements::system));
const rj::Pointer Communication::identifierValuePointer(ElementName::path(elements::identifier, elements::value));

const std::map<Communication::MessageType, std::string_view> MessageTypeToString = {
    { Communication::MessageType::InfoReq,          "InfoReq"          },
    { Communication::MessageType::ChargChangeReq,   "ChargChangeReq"   },
    { Communication::MessageType::ChargChangeReply, "ChargChangeReply" },
    { Communication::MessageType::Reply,            "Reply"            },
    { Communication::MessageType::DispReq,          "DispReq"          },
    { Communication::MessageType::Representative,   "Representative"   }
};

int8_t model::Communication::messageTypeToInt(Communication::MessageType messageType)
{
    return magic_enum::enum_integer(messageType);
}

const std::map<std::string_view, Communication::MessageType> StringToMessageType = {
    {"InfoReq",           Communication::MessageType::InfoReq          },
    {"ChargChangeReq",    Communication::MessageType::ChargChangeReq   },
    {"ChargChangeReply",  Communication::MessageType::ChargChangeReply },
    {"Reply",             Communication::MessageType::Reply            },
    {"DispReq",           Communication::MessageType::DispReq          },
    {"Representative",    Communication::MessageType::Representative   }
};

const std::map<Communication::MessageType, std::string_view> MessageTypeToProfileUrl = {
    { Communication::MessageType::InfoReq,          structure_definition::communicationInfoReq          },
    { Communication::MessageType::ChargChangeReq,   structure_definition::communicationChargChangeReq   },
    { Communication::MessageType::ChargChangeReply, structure_definition::communicationChargChangeReply },
    { Communication::MessageType::Reply,            structure_definition::communicationReply            },
    { Communication::MessageType::DispReq,          structure_definition::communicationDispReq          },
    { Communication::MessageType::Representative,   structure_definition::communicationRepresentative   }
};

const std::map<std::string_view, Communication::MessageType> ProfileUrlToMessageType = {
    {structure_definition::communicationInfoReq,          Communication::MessageType::InfoReq          },
    {structure_definition::communicationChargChangeReq,   Communication::MessageType::ChargChangeReq   },
    {structure_definition::communicationChargChangeReply, Communication::MessageType::ChargChangeReply },
    {structure_definition::communicationReply,            Communication::MessageType::Reply            },
    {structure_definition::communicationDispReq,          Communication::MessageType::DispReq          },
    {structure_definition::communicationRepresentative,   Communication::MessageType::Representative   }
};

const std::map<Communication::MessageType, bool> MessageTypeHasPrescriptonId = {
    { Communication::MessageType::InfoReq,          false },
    { Communication::MessageType::ChargChangeReq,   true  },
    { Communication::MessageType::ChargChangeReply, true  },
    { Communication::MessageType::Reply,            false },
    { Communication::MessageType::DispReq,          true  },
    { Communication::MessageType::Representative,   true  }
};

const std::string_view& Communication::messageTypeToString(MessageType messageType)
{
    const auto& it = MessageTypeToString.find(messageType);
    Expect3 (it != MessageTypeToString.end(), "Message type enumerator value " + std::to_string(static_cast<int>(messageType)) + " is out of range", std::logic_error);
    return it->second;
}

Communication::MessageType Communication::stringToMessageType(const std::string_view& messageType)
{
    const auto& it = StringToMessageType.find(messageType);
    ModelExpect(it != StringToMessageType.end(), "Invalid message type " + std::string(messageType));
    return it->second;
}

const std::string_view& Communication::messageTypeToProfileUrl(MessageType messageType)
{
    const auto& it = MessageTypeToProfileUrl.find(messageType);
    ModelExpect(it != MessageTypeToProfileUrl.end(), "Message type enumerator value " + std::to_string(static_cast<int>(messageType)) + " is out of range");
    return it->second;
}

Communication::MessageType Communication::profileUrlToMessageType(const std::string_view& profileUrl)
{
    const auto parts = String::split(profileUrl, '|');
    ModelExpect(!parts.empty(), "error processing profileUrl: " + std::string(profileUrl));
    const auto& it = ProfileUrlToMessageType.find(parts[0]);
    ModelExpect(it != ProfileUrlToMessageType.end(), "Could not retrieve message type from " + std::string(profileUrl));
    return it->second;
}

bool Communication::messageTypeHasPrescriptionId(MessageType messageType)
{
    const auto& it = MessageTypeHasPrescriptonId.find(messageType);
    ModelExpect(it != MessageTypeHasPrescriptonId.end(), "Message type enumerator value " + std::to_string(static_cast<int>(messageType)) + " is out of range");
    return it->second;
}

SchemaType Communication::messageTypeToSchemaType(MessageType messageType)
{
    switch (messageType)
    {
    case model::Communication::MessageType::InfoReq:
        return SchemaType::Gem_erxCommunicationInfoReq;
    case model::Communication::MessageType::ChargChangeReq:
    case model::Communication::MessageType::ChargChangeReply:
        return SchemaType::fhir;
    case model::Communication::MessageType::Reply:
        return SchemaType::Gem_erxCommunicationReply;
    case model::Communication::MessageType::DispReq:
        return SchemaType::Gem_erxCommunicationDispReq;
    case model::Communication::MessageType::Representative:
        return SchemaType::Gem_erxCommunicationRepresentative;
    }
    ErpFail(HttpStatus::InternalServerError, "Converting Communication::messageType to SchemaType failed");
}

static const std::string invalidDataMessage = "The data does not conform to that expected by the FHIR profile or is invalid";

std::string Communication::retrievePrescriptionIdFromTaskReference(const std::string_view& taskReference)
{
    // taskReference may look like:
    // InfoReq and Reply:           "Task/160.123.456.789.123.58" or
    // DispReq and Representative:  "Task/160.123.456.789.123.58/$accept?ac=777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea"
    std::string path;
    std::tie(path, std::ignore, std::ignore) = UrlHelper::splitTarget(std::string(taskReference));
    std::cmatch result;
    if (path.find("$accept") == std::string::npos)
    {
        static const std::regex pathRegexTaskId(UrlHelper::convertPathToRegex("Task/{id}"));
        bool matches = std::regex_match(path.c_str(), result, pathRegexTaskId);
        if (matches && result.size() == 2)
        {
            return result.str(1);
        }
    }
    else
    {
        static const std::regex pathRegexTaskIdAccessCode(UrlHelper::convertPathToRegex("Task/{id}/$accept"));
        bool matches = std::regex_match(path.c_str(), result, pathRegexTaskIdAccessCode);
        if (matches && result.size() == 2)
        {
            return result.str(1);
        }
    }
    ErpFail(HttpStatus::BadRequest, "Failed to parse prescription ID from task reference");
}

std::optional<std::string> Communication::retrieveAccessCodeFromTaskReference(const std::string_view& taskReference)
{
    // taskReference may look like:
    // InfoReq and Reply:           "Task/160.123.456.789.123.58" or
    // DispReq and Representative:  "Task/160.123.456.789.123.58/$accept?ac=777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea"
    // The access code starts after "$accept?ac=" and goes to the end.
    const auto& [path, query, fragment] = UrlHelper::splitTarget(std::string(taskReference));
    const std::vector<std::pair<std::string, std::string>> queryParameters = UrlHelper::splitQuery(query);
    for (const auto& queryParameter : queryParameters)
    {
        if (queryParameter.first == "ac")
        {
            return queryParameter.second;
        }
    }
    return {};
}

Communication::Communication(NumberAsStringParserDocument&& document) :
    Resource<Communication>(std::move(document)),
    mPayload(getValue(payloadPointer))
{
}

Communication::MessageType Communication::messageType() const
{
    return profileUrlToMessageType(getStringValue(metaProfile0Pointer)); // throws an exception if conversion fails
}

const std::string_view& Communication::messageTypeAsString() const
{
    return messageTypeToString(messageType()); // may throw an exception if the message type is not in the body
}

const std::string_view& Communication::messageTypeAsProfileUrl() const
{
    return messageTypeToProfileUrl(messageType()); // may throw an exception if the message type is not in the body
}

int8_t model::Communication::messageTypeAsInt() const
{
    return messageTypeToInt(messageType());
}

std::optional<Uuid> Communication::id() const
{
    const auto optionalValue = getOptionalStringValue(idPointer);
    if (optionalValue.has_value())
        return Uuid(std::string(optionalValue.value()));
    else
        return {};
}

void Communication::setId(const Uuid& id)
{
    setValue(idPointer, id.toString());
}

std::optional<std::string_view> Communication::sender() const
{
    return getOptionalStringValue(senderIdentifierValuePointer);
}

void Communication::setSender(const std::string_view& sender)
{
    setValue(senderIdentifierValuePointer, sender);
    if (messageType() == MessageType::Reply)
    {
        setValue(senderIdentifierSystemPointer, naming_system::telematicID);
    }
    else
    {
        setValue(senderIdentifierSystemPointer, naming_system::gkvKvid10);
    }
}

std::optional<std::string_view> Communication::recipient() const
{
    return getOptionalStringValue(recipient0IdentifierValuePointer);
}

void Communication::setRecipient(const std::string_view& recipient)
{
    if (!hasValue(recipientPointer))
    {
        rj::Value oIdentifier(rj::kObjectType);
        const MessageType msgType = messageType();
        if (msgType == MessageType::Reply || msgType == MessageType::Representative)
        {
            setKeyValue(oIdentifier, identifierSystemPointer, naming_system::gkvKvid10);
        }
        else
        {
            setKeyValue(oIdentifier, identifierSystemPointer, naming_system::telematicID);
        }
        setKeyValue(oIdentifier, identifierValuePointer, recipient);

        rj::Value oRecipient(rj::kArrayType);
        setValue(recipientPointer, oRecipient);

        addToArray(recipientPointer, std::move(oIdentifier));
    }
    else
    {
        setValue(recipient0IdentifierValuePointer, recipient);
    }
}

std::optional<Timestamp> Communication::timeSent() const
{
    const auto optionalValue = getOptionalStringValue(sentPointer);
    if (optionalValue.has_value())
        return Timestamp::fromFhirDateTime(std::string(optionalValue.value()));
    else
        return {};
}

void Communication::setTimeSent(const Timestamp& timestamp)
{
    setValue(sentPointer, timestamp.toXsDateTime());
}

std::optional<Timestamp> Communication::timeReceived() const
{
    const auto optionalValue = getOptionalStringValue(receivedPointer);
    if (optionalValue.has_value())
        return Timestamp::fromFhirDateTime(std::string(optionalValue.value()));
    else
        return {};
}

void Communication::setTimeReceived(const Timestamp& timestamp)
{
    setValue(receivedPointer, timestamp.toXsDateTime());
}

void model::Communication::deleteTimeReceived()
{
    removeElement(receivedPointer);
}

PrescriptionId Communication::prescriptionId() const
{
    std::string_view taskReference = getStringValue(basedOn0ReferencePointer);
    const auto& prescriptionId = retrievePrescriptionIdFromTaskReference(taskReference);
    return PrescriptionId::fromString(prescriptionId);
}

std::optional<std::string> Communication::accessCode() const
{
    std::optional<std::string_view> taskReference = getOptionalStringValue(basedOn0ReferencePointer);
    if (taskReference.has_value())
    {
        return retrieveAccessCodeFromTaskReference(taskReference.value());
    }
    return {};
}

std::optional<std::string_view> Communication::contentString(uint32_t idx) const
{
    return getOptionalStringValue(rj::Pointer(ElementName::path(elements::payload, idx, elements::contentString)));
}

void Communication::verifyPayload() const
{
    mPayload.verifyLength();
    mPayload.verifyUrls();
    mPayload.verifyMimeTypes();
}
