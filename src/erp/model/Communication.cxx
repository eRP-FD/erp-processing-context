/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/fhir/Fhir.hxx"
#include "erp/model/Communication.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/String.hxx"
#include "shared/util/UrlHelper.hxx"

#include <rapidjson/writer.h>
#include <map>
#include <ranges>
#include <regex>

using namespace model;
using namespace model::resource;
using model::Timestamp;

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

namespace
{
const rj::Pointer payloadContentStringPointer(ElementName::path(elements::payload, 0, elements::contentString));

const std::map<Communication::MessageType, std::string_view> MessageTypeToString = {
    { Communication::MessageType::InfoReq,          "InfoReq"          },
    { Communication::MessageType::ChargChangeReq,   "ChargChangeReq"   },
    { Communication::MessageType::ChargChangeReply, "ChargChangeReply" },
    { Communication::MessageType::Reply,            "Reply"            },
    { Communication::MessageType::DispReq,          "DispReq"          },
    { Communication::MessageType::Representative,   "Representative"   }
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

std::string retrievePrescriptionIdFromReference(
    std::string_view reference,
    const model::Communication::MessageType messageType)
{
    // Reference may look like:
    // InfoReq and Reply:           "[baseUrl/]Task/160.123.456.789.123.58" or
    // DispReq and Representative:  "[baseUrl/]Task/160.123.456.789.123.58/$accept?ac=777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea" or
    // ChargChangeReq and ChargChangeReply: "[baseUrl/]ChargeItem/200.000.000.006.522.02"
    std::string path;
    std::tie(path, std::ignore, std::ignore) = UrlHelper::splitTarget(std::string(reference));
    std::cmatch result;
    bool matches = false;
    if (path.find("$accept") == std::string::npos)
    {
        switch(messageType)
        {
            case Communication::MessageType::ChargChangeReq:
            case Communication::MessageType::ChargChangeReply:
            {
                static const std::regex pathRegexChargeItemId(UrlHelper::convertPathToRegex(".*ChargeItem/{id}"));
                matches = std::regex_match(path.c_str(), result, pathRegexChargeItemId);
            }
            break;
            case Communication::MessageType::InfoReq:
            case Communication::MessageType::DispReq:
            case Communication::MessageType::Representative:
            case Communication::MessageType::Reply:
            {
                static const std::regex pathRegexTaskItemId(UrlHelper::convertPathToRegex(".*Task/{id}"));
                matches = std::regex_match(path.c_str(), result, pathRegexTaskItemId);
            }
            break;
        }
    }
    else
    {
        switch(messageType)
        {
            case Communication::MessageType::ChargChangeReq:
            case Communication::MessageType::ChargChangeReply:
                ModelFail("Invalid reference for this message type");
            case Communication::MessageType::InfoReq:
            case Communication::MessageType::DispReq:
            case Communication::MessageType::Representative:
            case Communication::MessageType::Reply:
            {
                static const std::regex pathRegexTaskIdAccessCode(UrlHelper::convertPathToRegex(".*Task/{id}/$accept"));
                matches = std::regex_match(path.c_str(), result, pathRegexTaskIdAccessCode);
            }
        }
    }

    if (matches && result.size() == 2)
    {
        return result.str(1);
    }

    ModelFail("Failed to parse prescription ID from reference");
}

std::optional<std::string> retrieveAccessCodeFromTaskReference(std::string_view taskReference)
{
    // taskReference may look like:
    // InfoReq and Reply:           "[baseUrl/]Task/160.123.456.789.123.58" or
    // DispReq and Representative:  "[baseUrl/]Task/160.123.456.789.123.58/$accept?ac=777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea"
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

} // anonymous namespace


Communication::Communication(NumberAsStringParserDocument&& document)
    : Resource(std::move(document))
{
}

int8_t model::Communication::messageTypeToInt(Communication::MessageType messageType)
{
    return magic_enum::enum_integer(messageType);
}

std::string_view Communication::messageTypeToString(MessageType messageType)
{
    const auto& it = MessageTypeToString.find(messageType);
    Expect3 (it != MessageTypeToString.end(), "Message type enumerator value " + std::to_string(static_cast<int>(messageType)) + " is out of range", std::logic_error);
    return it->second;
}

std::string_view Communication::messageTypeToProfileUrl(MessageType messageType) const
{
    const auto& it = MessageTypeToProfileUrl.find(messageType);
    ModelExpect(it != MessageTypeToProfileUrl.end(),
                "Message type enumerator value " + std::to_string(static_cast<int>(messageType)) + " is out of range");
    return it->second;
}

Communication::MessageType Communication::profileUrlToMessageType(std::string_view profileUrl)
{
    const auto parts = String::split(profileUrl, '|');
    ModelExpect(!parts.empty(), "error processing profileUrl: " + std::string(profileUrl));
    const auto& it = ProfileUrlToMessageType.find(parts[0]);
    ModelExpect(it != ProfileUrlToMessageType.end(), "Could not retrieve message type from " + std::string(profileUrl));
    return it->second;
}

ProfileType Communication::messageTypeToProfileType(MessageType messageType)
{
    switch (messageType)
    {
    case model::Communication::MessageType::InfoReq:
            return ProfileType::Gem_erxCommunicationInfoReq;
    case model::Communication::MessageType::ChargChangeReq:
            return ProfileType::Gem_erxCommunicationChargChangeReq;
    case model::Communication::MessageType::ChargChangeReply:
            return ProfileType::Gem_erxCommunicationChargChangeReply;
    case model::Communication::MessageType::Reply:
            return ProfileType::Gem_erxCommunicationReply;
    case model::Communication::MessageType::DispReq:
            return ProfileType::Gem_erxCommunicationDispReq;
    case model::Communication::MessageType::Representative:
            return ProfileType::Gem_erxCommunicationRepresentative;
    }
    ModelFail("Converting Communication::messageType to SchemaType failed");
}

Communication::MessageType Communication::profileTypeToMessageType(ProfileType profileType)
{
    switch (profileType)
    {
    case ProfileType::Gem_erxCommunicationInfoReq:
            return model::Communication::MessageType::InfoReq;
    case ProfileType::Gem_erxCommunicationChargChangeReq:
            return  model::Communication::MessageType::ChargChangeReq;
    case ProfileType::Gem_erxCommunicationChargChangeReply:
            return model::Communication::MessageType::ChargChangeReply;
    case ProfileType::Gem_erxCommunicationReply:
            return model::Communication::MessageType::Reply;
    case ProfileType::Gem_erxCommunicationDispReq:
            return model::Communication::MessageType::DispReq;
    case ProfileType::Gem_erxCommunicationRepresentative:
            return model::Communication::MessageType::Representative;
    case ProfileType::ActivateTaskParameters:
    case ProfileType::CreateTaskParameters:
    case ProfileType::Gem_erxAuditEvent:
    case ProfileType::Gem_erxBinary:
    case ProfileType::fhir:
    case ProfileType::Gem_erxCompositionElement:
    case ProfileType::Gem_erxDevice:
    case ProfileType::Gem_erxDigest:
    case ProfileType::GEM_ERP_PR_Medication:
    case ProfileType::GEM_ERP_PR_PAR_CloseOperation_Input:
    case ProfileType::GEM_ERP_PR_PAR_DispenseOperation_Input:
    case ProfileType::KBV_PR_ERP_Bundle:
    case ProfileType::KBV_PR_ERP_Composition:
    case ProfileType::KBV_PR_ERP_Medication_Compounding:
    case ProfileType::KBV_PR_ERP_Medication_FreeText:
    case ProfileType::KBV_PR_ERP_Medication_Ingredient:
    case ProfileType::KBV_PR_ERP_Medication_PZN:
    case ProfileType::KBV_PR_ERP_PracticeSupply:
    case ProfileType::KBV_PR_ERP_Prescription:
    case ProfileType::KBV_PR_EVDGA_Bundle:
    case ProfileType::KBV_PR_EVDGA_HealthAppRequest:
    case ProfileType::KBV_PR_FOR_Coverage:
    case ProfileType::KBV_PR_FOR_Organization:
    case ProfileType::KBV_PR_FOR_Patient:
    case ProfileType::KBV_PR_FOR_Practitioner:
    case ProfileType::KBV_PR_FOR_PractitionerRole:
    case ProfileType::GEM_ERP_PR_MedicationDispense:
    case ProfileType::GEM_ERP_PR_MedicationDispense_DiGA:
    case ProfileType::MedicationDispenseBundle:
    case ProfileType::Gem_erxReceiptBundle:
    case ProfileType::Gem_erxTask:
    case ProfileType::Gem_erxChargeItem:
    case ProfileType::Gem_erxConsent:
    case ProfileType::PatchChargeItemParameters:
    case ProfileType::DAV_DispenseItem:
    case ProfileType::Subscription:
    case ProfileType::OperationOutcome:
    case ProfileType::ProvidePrescriptionErpOp:
    case ProfileType::EPAOpRxPrescriptionERPOutputParameters:
    case ProfileType::CancelPrescriptionErpOp:
    case ProfileType::EPAOpRxDispensationERPOutputParameters:
    case ProfileType::ProvideDispensationErpOp:
    case ProfileType::OrganizationDirectory:
    case ProfileType::EPAMedicationPZNIngredient:
        ModelFail("Not a Communication Profile");
    }
    Fail2("Communication::profileTypeToMessageType: Unknown ProfileType: " +
              std::to_string(static_cast<intmax_t>(profileType)),
          std::logic_error);
}


Communication::MessageType Communication::messageType() const
{
    auto profileName = getProfileName();
    ErpExpect(profileName.has_value(), HttpStatus::BadRequest, "Profile missing in Communication message.");
    auto profileType = findProfileType(*profileName);
    ErpExpect(profileType.has_value(), HttpStatus::BadRequest, std::string{"Unknown profile: "} += *profileName);
    return profileTypeToMessageType(*profileType);
}

std::string_view Communication::messageTypeAsString() const
{
    return messageTypeToString(messageType()); // may throw an exception if the message type is not in the body
}

std::string_view Communication::messageTypeAsProfileUrl() const
{
    return messageTypeToProfileUrl(messageType()); // may throw an exception if the message type is not in the body
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

std::optional<Identity> Communication::sender() const
{
    const auto senderValue = getOptionalStringValue(senderIdentifierValuePointer);
    const auto senderSystem = getOptionalStringValue(senderIdentifierSystemPointer);
    if (!senderValue || !senderSystem)
        return {};
    if (*senderSystem == resource::naming_system::telematicID)
    {
        return TelematikId{*senderValue};
    }
    return Kvnr{*senderValue, *senderSystem};
}

bool Communication::isReply() const
{
    const auto msgType = messageType();
    return msgType == MessageType::Reply || msgType == MessageType::ChargChangeReply;
}

void Communication::setSender(const Identity& sender)
{
    const auto setter = [this](const auto& id) {
        setSender(id);
    };
    std::visit(setter, sender);
}

void model::Communication::setSender(const Kvnr& kvnr)
{
    ModelExpect(messageType() != MessageType::ChargChangeReply, "ChargChangeReply cannot be sent by insurant");
    setSender(kvnr.id(), kvnr.namingSystem());
}

void model::Communication::setSender(const TelematikId& telematikId)
{
    ModelExpect(isReply(), "Requests cannot be sent from telematik-id");
    setSender(telematikId.id(), naming_system::telematicID);
}

void Communication::setSender(std::string_view sender, std::string_view namingSystem)
{
    setValue(senderIdentifierValuePointer, sender);
    setValue(senderIdentifierSystemPointer, namingSystem);
}

Identity Communication::recipient() const
{
    const auto recipientValue = getStringValue(recipient0IdentifierValuePointer);
    const auto recipientSystem = getStringValue(recipient0IdentifierSystemPointer);
    if (recipientSystem == resource::naming_system::telematicID)
    {
        return TelematikId{recipientValue};
    }
    return Kvnr{recipientValue, recipientSystem};
}

void Communication::setRecipient(const Identity& recipient)
{
    if (std::holds_alternative<Kvnr>(recipient))
    {
        ModelExpect(isReply() || messageType() == MessageType::Representative, "Insurant cannot be recipient for requests");
        const auto& kvnr = std::get<Kvnr>(recipient);
        setRecipient(kvnr.id(), kvnr.namingSystem());
    }
    else
    {
        ModelExpect(! isReply(), "Recipient cannot be telematik-id for replies");
        const auto& telematikId = std::get<TelematikId>(recipient);
        setRecipient(telematikId.id(), naming_system::telematicID);
    }
}

void Communication::setRecipient(std::string_view recipient, std::string_view namingSystem)
{
    if (! hasValue(recipientPointer))
    {
        rj::Value oRecipient(rj::kArrayType);
        setValue(recipientPointer, oRecipient);

        rj::Value oIdentifier(rj::kObjectType);
        addToArray(recipientPointer, std::move(oIdentifier));

    }
    setValue(recipient0IdentifierValuePointer, recipient);
    setValue(recipient0IdentifierSystemPointer, namingSystem);
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
    std::string_view reference = getStringValue(basedOn0ReferencePointer);
    const auto& prescriptionId = retrievePrescriptionIdFromReference(reference, messageType());
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

std::optional<std::string_view> Communication::contentString() const
{
    return getOptionalStringValue(payloadContentStringPointer);
}

// GEMREQ-start A_19450-01
void Communication::verifyPayload(const JsonValidator& validator) const
{
    CommunicationPayload payload{getValue(payloadPointer)};
    if (!payload.empty())
    {
        payload.verifyLength();
        const auto schema = payloadSchema();
        if (schema.has_value())
        {
            payload.validateJsonSchema(validator, *schema);
        }
    }
}
// GEMREQ-end A_19450-01


std::optional<SchemaType> Communication::payloadSchema() const
{
    using enum MessageType;
    switch (messageType())
    {
        case ChargChangeReq:
        case ChargChangeReply:
        case InfoReq:
        case Representative:
            return std::nullopt;
        case DispReq:
            return SchemaType::CommunicationDispReqPayload;
        case Reply:
            return SchemaType::CommunicationReplyPayload;
    }
    Fail2("Unexpected value for 'messageType': " + std::to_string(intmax_t(messageType())), std::logic_error);
}

std::optional<model::Timestamp> model::Communication::getValidationReferenceTimestamp() const
{
    return Timestamp::now();
}

// GEMREQ-start A_19450-01#profileCheck
ProfileType model::Communication::profileType() const
{
    static const std::set<ProfileType> acceptedCommunicationSet{acceptedCommunications};
    auto profileName = getProfileName();
    ErpExpect(profileName.has_value(), HttpStatus::BadRequest, "Profile missing in Communication message.");
    auto profileType = findProfileType(*profileName);
    if (! profileType.has_value() || ! acceptedCommunicationSet.contains(*profileType))
    {
        const auto& fhirInstance = Fhir::instance();
        const auto& backend = std::addressof(fhirInstance.backend());
        const auto& viewList = fhirInstance.structureRepository(model::Timestamp::now());
        std::list<std::string> profileUrls;
        std::ranges::transform(acceptedCommunications, back_inserter(profileUrls), [](ProfileType pt) {
            return std::string{value(profile(pt))};
        });
        std::string_view sep;
        std::ostringstream supportlist;
        for (const auto& key : viewList.supportedVersions(backend, profileUrls))
        {
            supportlist << sep << key;
            sep = ", ";
        }
        ModelFail("invalid profile " + std::string{*profileName} + " must be one of: " + std::move(supportlist).str());
    }
    return *profileType;
}
// GEMREQ-end A_19450-01#profileCheck
