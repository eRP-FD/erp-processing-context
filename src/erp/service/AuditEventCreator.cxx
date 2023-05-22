/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/AuditEventCreator.hxx"
#include "erp/service/Operation.hxx"
#include "erp/service/ErpRequestHandler.hxx"

#include "erp/model/ResourceNames.hxx"
#include "erp/crypto/Jwt.hxx"

#include "erp/util/String.hxx"

#include "erp/util/Expect.hxx"

namespace
{

std::tuple<std::string, std::string, std::string> evalAgentData(
    const model::AuditData& auditData,
    const JWT& accessToken,
    model::ResourceVersion::DeGematikErezeptWorkflowR4 profileVersion)
{
    std::string whoIdentifierSystem;
    std::string whoIdentifierValue;
    std::string agentName;

    const bool isDeprecatedVersion = model::ResourceVersion::deprecatedProfile(profileVersion);

    const auto nsGkvKvid = isDeprecatedVersion ? model::resource::naming_system::deprecated::gkvKvid10
                                               : model::resource::naming_system::gkvKvid10;
    const auto nsTelematikId = isDeprecatedVersion ? model::resource::naming_system::deprecated::telematicID
                                                   : model::resource::naming_system::telematicID;

    if(model::isEventCausedByPatient(auditData.eventId()))
    {
        // if we dont know if it is PKV or GKV, use GKV, cf. ERP-11991
        whoIdentifierSystem = auditData.insurantKvnr().namingSystem(isDeprecatedVersion);
        whoIdentifierValue = auditData.insurantKvnr().id();
        const auto givenNameClaim = accessToken.stringForClaim(JWT::givenNameClaim);
        Expect3(givenNameClaim.has_value(), "Missing givenNameClaim", std::logic_error);
        const auto familyNameClaim = accessToken.stringForClaim(JWT::familyNameClaim);
        Expect3(familyNameClaim.has_value(), "Missing familyNameClaim", std::logic_error);
        agentName = givenNameClaim.value() + (givenNameClaim.value().empty() ? "" : " ") + familyNameClaim.value();
    }
    else
    {
        whoIdentifierSystem = model::isEventCausedByRepresentative(auditData.eventId()) ? nsGkvKvid : nsTelematikId;
        Expect3(auditData.metaData().agentWho().has_value(), "Missing agent identifier in audit data",
                std::logic_error);
        whoIdentifierValue = auditData.metaData().agentWho().value();
        if (!model::isEventCausedByRepresentative(auditData.eventId()) && !auditData.metaData().agentName().has_value())
        {
            agentName = "unbekannt";
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
    const JWT& accessToken,
    model::ResourceVersion::DeGematikErezeptWorkflowR4 profileVersion)
{
    model::AuditEvent auditEvent(profileVersion);

    auditEvent.setId(auditData.id());
    auditEvent.setRecorded(auditData.recorded());
    auditEvent.setAction(auditData.action());
    auditEvent.setSubTypeCode(model::AuditEvent::ActionToSubType.at(auditData.action()));
    auditEvent.setOutcome(model::AuditEvent::Outcome::success);
    auditEvent.setSourceObserverReference("Device/" + std::to_string(auditData.deviceId()));

    // agent data
    std::string agentName = "E-Rezept Fachdienst"; // default value for audit events created by maintenance script;
    if(!isEventCausedByMaintenanceScript(auditData.eventId()))
    {
        const auto [whoIdentifierSystem, whoIdentifierValue, whoAgentName] = evalAgentData(auditData, accessToken, profileVersion);
        auditEvent.setAgentWho(whoIdentifierSystem, whoIdentifierValue);
        agentName = whoAgentName;
    }
    auditEvent.setAgentName(agentName);
    auditEvent.setAgentType(auditData.agentType());

    // entity data
    std::string resourceIdStr;
    if (auditData.prescriptionId().has_value())
    {
        const bool isDeprecatedVersion = model::ResourceVersion::deprecatedProfile(profileVersion);
        const auto nsPrescriptionId = isDeprecatedVersion ? model::resource::naming_system::deprecated::prescriptionID
                                                : model::resource::naming_system::prescriptionID;
        resourceIdStr = auditData.prescriptionId()->toString();
        auditEvent.setEntityWhatIdentifier(nsPrescriptionId, resourceIdStr);
        auditEvent.setEntityDescription(resourceIdStr);
    }
    else
    {
        // entity.description is mandatory according to the profile. If we don't have a unique prescriptionID
        // to fill it (for endpoints GET /Task or GET /MedicationDispense and the /Consent endpoints), we write a
        // fixed string "+", except for POST and DELETE /Consent, where we write "CHARGCONS".
        // See ticket ERP-5081, question ERP-7951 and ticket ERP-10743

        const auto eventId = auditData.eventId();
        if(model::AuditEventId::POST_Consent == eventId || model::AuditEventId::DELETE_Consent == eventId)
        {
            auditEvent.setEntityDescription("CHARGCONS");
        }
        else
        {
            auditEvent.setEntityDescription("+");
        }

        if (auditData.consentId().has_value())
        {
            resourceIdStr = auditData.consentId().value();
            auditEvent.setEntityWhatIdentifier({}, resourceIdStr);
        }
    }

    auditEvent.setEntityName(auditData.insurantKvnr().id());
    auditEvent.setEntityWhatReference(model::createEventResourceReference(auditData.eventId(), resourceIdStr));

    // text/div
    const auto [text, usedLanguage] =  textResources.retrieveTextTemplate(auditData.eventId(), language);
    auditEvent.setTextDiv(R"--(<div xmlns="http://www.w3.org/1999/xhtml">)--" +
                          replaceTextTemplateVariables(text, agentName, resourceIdStr) +
                          "</div>");
    auditEvent.setLanguage(usedLanguage);

    return auditEvent;
}
