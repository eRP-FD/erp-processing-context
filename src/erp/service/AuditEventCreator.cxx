#include "erp/service/AuditEventCreator.hxx"
#include "erp/service/Operation.hxx"
#include "erp/service/ErpRequestHandler.hxx"

#include "erp/model/ResourceNames.hxx"
#include "erp/crypto/Jwt.hxx"

#include "erp/util/String.hxx"
#include "erp/util/Expect.hxx"

namespace
{

std::string createWhatReference(
    model::AuditEventId eventId,
    const std::string& prescriptionId)
{
    switch(eventId)
    {
        case model::AuditEventId::GET_Task:
            return "Task";
        case model::AuditEventId::GET_Task_id_insurant:
        case model::AuditEventId::GET_Task_id_representative:
        case model::AuditEventId::GET_Task_id_pharmacy:
        case model::AuditEventId::Task_delete_expired_id:
            return "Task/" + prescriptionId;
        case model::AuditEventId::POST_Task_activate:
            return "Task/" + prescriptionId + "/$activate";
        case model::AuditEventId::POST_Task_accept:
            return "Task/" + prescriptionId + "/$accept";
        case model::AuditEventId::POST_Task_reject:
            return "Task/" + prescriptionId + "/$reject";
        case model::AuditEventId::POST_Task_close:
            return "Task/" + prescriptionId + "/$close";
        case model::AuditEventId::POST_Task_abort_doctor:
        case model::AuditEventId::POST_Task_abort_insurant:
        case model::AuditEventId::POST_Task_abort_representative:
        case model::AuditEventId::POST_Task_abort_pharmacy:
            return "Task/" + prescriptionId + "/$abort";
        case model::AuditEventId::GET_MedicationDispense:
            return "MedicationDispense";
        case model::AuditEventId::GET_MedicationDispense_id:
            return "MedicationDispense/" + prescriptionId;
        default:
            throw std::logic_error("Invalid event id");
    }
}

std::tuple<std::string, std::string, std::string> evalAgentData(
    const model::AuditData& auditData,
    const JWT& accessToken)
{
    std::string whoIdentifierSystem;
    std::string whoIdentifierValue;
    std::string agentName;

    if(model::isEventCausedByPatient(auditData.eventId()))
    {
        whoIdentifierSystem = model::resource::naming_system::gkvKvid10;
        whoIdentifierValue = auditData.insurantKvnr();
        const auto givenNameClaim = accessToken.stringForClaim(JWT::givenNameClaim);
        Expect3(givenNameClaim.has_value(), "Missing givenNameClaim", std::logic_error);
        const auto familyNameClaim = accessToken.stringForClaim(JWT::familyNameClaim);
        Expect3(familyNameClaim.has_value(), "Missing familyNameClaim", std::logic_error);
        agentName = givenNameClaim.value() + (givenNameClaim.value().empty() ? "" : " ") + familyNameClaim.value();
    }
    else
    {
        whoIdentifierSystem = model::isEventCausedByRepresentative(auditData.eventId()) ?
                              model::resource::naming_system::gkvKvid10 : model::resource::naming_system::telematicID;
        Expect3(auditData.metaData().agentWho().has_value(), "Missing agent identifier in audit data", std::logic_error);
        whoIdentifierValue = auditData.metaData().agentWho().value();
        if (!model::isEventCausedByRepresentative(auditData.eventId()) && !auditData.metaData().agentName().has_value())
        {
            agentName = "<null>";
        }
        else
        {
            Expect3(auditData.metaData().agentName().has_value(), "Missing agent name in audit data", std::logic_error);
            agentName = auditData.metaData().agentName().value();
        }
    }

    return std::make_tuple(whoIdentifierSystem, whoIdentifierValue, agentName);
}

std::string replaceTextTemplateVariables(
    const std::string& text,
    const std::string& agentName,
    const std::string& prescriptionId)
{
    std::string result = String::replaceAll(text, std::string(AuditEventTextTemplates::selfVariableName), agentName);
    result = String::replaceAll(result, std::string(AuditEventTextTemplates::agentNameVariableName), agentName);
    result = String::replaceAll(result, std::string(AuditEventTextTemplates::prescriptionIdVariableName), prescriptionId);
    return result;
}

}  // anonymous namespace


model::AuditEvent AuditEventCreator::fromAuditData(
    const model::AuditData& auditData,
    const std::string& language,
    const AuditEventTextTemplates& textResources,
    const JWT& accessToken)
{
    model::AuditEvent auditEvent;

    auditEvent.setId(auditData.id());
    auditEvent.setRecorded(auditData.recorded());
    auditEvent.setAction(auditData.action());
    auditEvent.setSubTypeCode(model::AuditEvent::ActionToSubType.at(auditData.action()));
    auditEvent.setOutcome(model::AuditEvent::Outcome::success);
    auditEvent.setSourceObserverReference("Device/" + std::to_string(auditData.deviceId()));

    // agent data
    std::string agentName = "E-Rezept Fachdienst"; // default value for audit events created by maintenance script;
    if(auditData.eventId() != model::AuditEventId::Task_delete_expired_id)
    {
        const auto [whoIdentifierSystem, whoIdentifierValue, whoAgentName] = evalAgentData(auditData, accessToken);
        auditEvent.setAgentWho(whoIdentifierSystem, whoIdentifierValue);
        agentName = whoAgentName;
    }
    auditEvent.setAgentName(agentName);
    auditEvent.setAgentType(auditData.agentType());

    // entity data
    std::string prescriptionIdStr;
    if(auditData.prescriptionId().has_value())
    {
        prescriptionIdStr = auditData.prescriptionId()->toString();
        auditEvent.setEntityWhatIdentifier(model::resource::naming_system::prescriptionID, prescriptionIdStr);
        auditEvent.setEntityDescription(prescriptionIdStr);
    }
    else
    {
        // entity.description is mandatory according to the profile. If we don't have a unique prescriptionID
        // to fill it (for endpoints GET /Task or GET /AuditEvent), we write a fixed string "+".
        // See https://dth01.ibmgcloud.net/jira/browse/ERP-5081.
        auditEvent.setEntityDescription("+");
    }
    auditEvent.setEntityName(auditData.insurantKvnr());
    auditEvent.setEntityWhatReference(createWhatReference(auditData.eventId(), prescriptionIdStr));

    // text/div
    const auto text = replaceTextTemplateVariables(
        textResources.retrieveTextTemplate(auditData.eventId(), language), agentName, prescriptionIdStr);
    auditEvent.setTextDiv(R"--(<div xmlns="http://www.w3.org/1999/xhtml">)--" + text + "</div>");
    auditEvent.setLanguage(language);

    return auditEvent;
}
