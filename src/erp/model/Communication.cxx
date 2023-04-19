/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Communication.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/UrlHelper.hxx"
#include "erp/util/String.hxx"

#include <rapidjson/writer.h>
#include <map>
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

const std::map<Communication::MessageType, std::string_view> MessageTypeToProfileUrlDeprecated = {
    { Communication::MessageType::InfoReq,          structure_definition::deprecated::communicationInfoReq          },
    { Communication::MessageType::Reply,            structure_definition::deprecated::communicationReply            },
    { Communication::MessageType::DispReq,          structure_definition::deprecated::communicationDispReq          },
    { Communication::MessageType::Representative,   structure_definition::deprecated::communicationRepresentative   }
};

const std::map<std::string_view, Communication::MessageType> ProfileUrlToMessageType = {
    {structure_definition::communicationInfoReq,          Communication::MessageType::InfoReq          },
    {structure_definition::communicationChargChangeReq,   Communication::MessageType::ChargChangeReq   },
    {structure_definition::communicationChargChangeReply, Communication::MessageType::ChargChangeReply },
    {structure_definition::communicationReply,            Communication::MessageType::Reply            },
    {structure_definition::communicationDispReq,          Communication::MessageType::DispReq          },
    {structure_definition::communicationRepresentative,   Communication::MessageType::Representative   }
};

const std::map<std::string_view, Communication::MessageType> ProfileUrlToMessageTypeDeprecated = {
    {structure_definition::deprecated::communicationInfoReq,          Communication::MessageType::InfoReq          },
    {structure_definition::deprecated::communicationReply,            Communication::MessageType::Reply            },
    {structure_definition::deprecated::communicationDispReq,          Communication::MessageType::DispReq          },
    {structure_definition::deprecated::communicationRepresentative,   Communication::MessageType::Representative   }
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
    : Resource<Communication, ResourceVersion::WorkflowOrPatientenRechnungProfile>(std::move(document))
    , mPayload(getValue(payloadPointer))
{
}

int8_t model::Communication::messageTypeToInt(Communication::MessageType messageType)
{
    return magic_enum::enum_integer(messageType);
}

bool Communication::isDeprecatedProfile() const
{
    return ResourceVersion::deprecatedProfile(value(getSchemaVersion()));
}

std::string_view Communication::messageTypeToString(MessageType messageType)
{
    const auto& it = MessageTypeToString.find(messageType);
    Expect3 (it != MessageTypeToString.end(), "Message type enumerator value " + std::to_string(static_cast<int>(messageType)) + " is out of range", std::logic_error);
    return it->second;
}

std::string_view Communication::messageTypeToProfileUrl(MessageType messageType) const
{
    const std::map<Communication::MessageType, std::string_view>& messageTypeMap =
        isDeprecatedProfile() ? MessageTypeToProfileUrlDeprecated : MessageTypeToProfileUrl;
    const auto& it = messageTypeMap.find(messageType);
    ModelExpect(it != messageTypeMap.end(),
                "Message type enumerator value " + std::to_string(static_cast<int>(messageType)) + " is out of range");
    return it->second;
}

Communication::MessageType Communication::profileUrlToMessageType(std::string_view profileUrl) const
{
    const auto parts = String::split(profileUrl, '|');
    ModelExpect(!parts.empty(), "error processing profileUrl: " + std::string(profileUrl));
    const std::map<std::string_view, Communication::MessageType>& profileMap =
        isDeprecatedProfile() ? ProfileUrlToMessageTypeDeprecated : ProfileUrlToMessageType;
    const auto& it = profileMap.find(parts[0]);
    ModelExpect(it != profileMap.end(), "Could not retrieve message type from " + std::string(profileUrl));
    return it->second;
}

SchemaType Communication::messageTypeToSchemaType(MessageType messageType)
{
    switch (messageType)
    {
    case model::Communication::MessageType::InfoReq:
        return SchemaType::Gem_erxCommunicationInfoReq;
    case model::Communication::MessageType::ChargChangeReq:
        return SchemaType::Gem_erxCommunicationChargChangeReq;
    case model::Communication::MessageType::ChargChangeReply:
        return SchemaType::Gem_erxCommunicationChargChangeReply;
    case model::Communication::MessageType::Reply:
        return SchemaType::Gem_erxCommunicationReply;
    case model::Communication::MessageType::DispReq:
        return SchemaType::Gem_erxCommunicationDispReq;
    case model::Communication::MessageType::Representative:
        return SchemaType::Gem_erxCommunicationRepresentative;
    }
    ModelFail("Converting Communication::messageType to SchemaType failed");
}

Communication::MessageType Communication::schemaTypeToMessageType(SchemaType schemaType)
{
    switch (schemaType)
    {
        case SchemaType::Gem_erxCommunicationInfoReq:
            return model::Communication::MessageType::InfoReq;
        case SchemaType::Gem_erxCommunicationChargChangeReq:
            return  model::Communication::MessageType::ChargChangeReq;
        case SchemaType::Gem_erxCommunicationChargChangeReply:
            return model::Communication::MessageType::ChargChangeReply;
        case SchemaType::Gem_erxCommunicationReply:
            return model::Communication::MessageType::Reply;
        case SchemaType::Gem_erxCommunicationDispReq:
            return model::Communication::MessageType::DispReq;
        case SchemaType::Gem_erxCommunicationRepresentative:
            return model::Communication::MessageType::Representative;
        case SchemaType::ActivateTaskParameters:
        case SchemaType::CreateTaskParameters:
        case SchemaType::Gem_erxAuditEvent:
        case SchemaType::Gem_erxBinary:
        case SchemaType::fhir:
        case SchemaType::Gem_erxCompositionElement:
        case SchemaType::Gem_erxDevice:
        case SchemaType::KBV_PR_ERP_Bundle:
        case SchemaType::KBV_PR_ERP_Composition:
        case SchemaType::KBV_PR_ERP_Medication_Compounding:
        case SchemaType::KBV_PR_ERP_Medication_FreeText:
        case SchemaType::KBV_PR_ERP_Medication_Ingredient:
        case SchemaType::KBV_PR_ERP_Medication_PZN:
        case SchemaType::KBV_PR_ERP_PracticeSupply:
        case SchemaType::KBV_PR_ERP_Prescription:
        case SchemaType::KBV_PR_FOR_Coverage:
        case SchemaType::KBV_PR_FOR_HumanName:
        case SchemaType::KBV_PR_FOR_Organization:
        case SchemaType::KBV_PR_FOR_Patient:
        case SchemaType::KBV_PR_FOR_Practitioner:
        case SchemaType::KBV_PR_FOR_PractitionerRole:
        case SchemaType::Gem_erxMedicationDispense:
        case SchemaType::MedicationDispenseBundle:
        case SchemaType::Gem_erxOrganizationElement:
        case SchemaType::Gem_erxReceiptBundle:
        case SchemaType::Gem_erxTask:
        case SchemaType::BNA_tsl:
        case SchemaType::Gematik_tsl:
        case SchemaType::Gem_erxChargeItem:
        case SchemaType::Gem_erxConsent:
        case SchemaType::PatchChargeItemParameters:
        case SchemaType::DAV_DispenseItem:
        case SchemaType::Pruefungsnachweis:
            ModelFail("Not a Communication Schema");
    }
    Fail2("Communication::schemaTypeToMessageType: Unknown SchemaType: "
        + std::to_string(static_cast<intmax_t>(schemaType)), std::logic_error);
}

Communication::MessageType Communication::messageType() const
{
    const auto profileName = getStringValue(metaProfile0Pointer);
    const auto* info = ResourceVersion::profileInfoFromProfileName(profileName);
    if (info) {
        try {
            return schemaTypeToMessageType(info->schemaType);
        }
        catch (const ModelException&) {}
    }
    std::ostringstream msg;
    msg << "invalid profile expected one of: [";
    std::string sep;
    if (ResourceVersion::supportedBundles().contains(ResourceVersion::FhirProfileBundleVersion::v_2022_01_01))
    {
        auto allVersions = ResourceVersion::profileVersionFromBundle(ResourceVersion::FhirProfileBundleVersion::v_2022_01_01);
        auto gemVer = get<ResourceVersion::DeGematikErezeptWorkflowR4>(allVersions);
        msg << '"' << model::resource::structure_definition::deprecated::communicationInfoReq << '|' << v_str(gemVer);
        msg << R"(", ")" << model::resource::structure_definition::deprecated::communicationReply << '|' << v_str(gemVer);
        msg << R"(", ")" << model::resource::structure_definition::deprecated::communicationDispReq << '|' << v_str(gemVer);
        msg << R"(", ")" << model::resource::structure_definition::deprecated::communicationRepresentative << '|' << v_str(gemVer);
        msg << '"';
        sep = ", ";
    }
    if (ResourceVersion::supportedBundles().contains(ResourceVersion::FhirProfileBundleVersion::v_2023_07_01))
    {
        auto allVersions = ResourceVersion::profileVersionFromBundle(ResourceVersion::FhirProfileBundleVersion::v_2023_07_01);
        auto gemVer = get<ResourceVersion::DeGematikErezeptWorkflowR4>(allVersions);
        auto gemChargVer = get<ResourceVersion::DeGematikErezeptPatientenrechnungR4>(allVersions);
        msg << sep << '"' << model::resource::structure_definition::communicationInfoReq << '|' << v_str(gemVer);
        msg << R"(", ")" << model::resource::structure_definition::communicationReply << '|' << v_str(gemVer);
        msg << R"(", ")" << model::resource::structure_definition::communicationDispReq << '|' << v_str(gemVer);
        msg << R"(", ")" << model::resource::structure_definition::communicationRepresentative << '|' << v_str(gemVer);
        msg << R"(", ")" << model::resource::structure_definition::communicationChargChangeReq << '|' << v_str(gemChargVer);
        msg << R"(", ")" << model::resource::structure_definition::communicationChargChangeReply << '|' << v_str(gemChargVer);
        msg << '"';
    }
    msg << "]";
    ModelFail(std::move(msg).str());
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
    if (*senderSystem == resource::naming_system::telematicID ||
        *senderSystem == resource::naming_system::deprecated::telematicID)
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
    if (std::holds_alternative<Kvnr>(sender))
    {
        ModelExpect(messageType() != MessageType::ChargChangeReply, "ChargChangeReply cannot be sent by insurant");
        const auto& kvnr = std::get<Kvnr>(sender);
        setSender(kvnr.id(), kvnr.namingSystem(isDeprecatedProfile()));
    }
    else
    {
        ModelExpect(isReply(), "Requests cannot be sent from telematik-id");
        const auto& telematikId = std::get<TelematikId>(sender);
        auto nsTelematikId =
            isDeprecatedProfile() ? naming_system::deprecated::telematicID : naming_system::telematicID;
        setSender(telematikId.id(), nsTelematikId);
    }
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
    if (recipientSystem == resource::naming_system::telematicID ||
        recipientSystem == resource::naming_system::deprecated::telematicID)
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
        setRecipient(kvnr.id(), kvnr.namingSystem(isDeprecatedProfile()));
    }
    else
    {
        ModelExpect(! isReply(), "Recipient cannot be telematik-id for replies");
        const auto& telematikId = std::get<TelematikId>(recipient);
        auto nsTelematikId =
            isDeprecatedProfile() ? naming_system::deprecated::telematicID : naming_system::telematicID;
        setRecipient(telematikId.id(), nsTelematikId);
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
void Communication::verifyPayload() const
{
    mPayload.verifyLength();
}
// GEMREQ-end A_19450-01

bool Communication::canValidateGeneric(MessageType messageType, ResourceVersion::WorkflowOrPatientenRechnungProfile profile)
{
    switch (messageType)
    {
        using enum model::Communication::MessageType;
        case DispReq:
        case Representative:
            return true;
        case InfoReq:
        case Reply:
        case ChargChangeReq:
        case ChargChangeReply:
            return ! ResourceVersion::deprecatedProfile(profile);
    }
    Fail2("Unexpected value for 'messageType': " + std::to_string(intmax_t(messageType)), std::logic_error);
}

bool model::Communication::canValidateGeneric() const
{
    return canValidateGeneric(messageType(), value(getSchemaVersion()));
}
