/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/AuditData.hxx"
#include "erp/util/Expect.hxx"

#include <rapidjson/pointer.h>

#include <utility>


namespace model
{

bool isEventCausedByPatient(AuditEventId eventId)
{
    return eventId == AuditEventId::GET_Task ||
           eventId == AuditEventId::GET_Task_id_insurant ||
           eventId == AuditEventId::POST_Task_abort_insurant ||
           eventId == AuditEventId::GET_MedicationDispense ||
           eventId == AuditEventId::GET_MedicationDispense_id ||
           eventId == AuditEventId::DELETE_ChargeItem_id_insurant ||
           eventId == AuditEventId::PUT_ChargeItem_id_insurant ||
           eventId == AuditEventId::PATCH_ChargeItem_id ||
           eventId == AuditEventId::GET_ChargeItem_id_insurant ||
           eventId == AuditEventId::POST_Consent ||
           eventId == AuditEventId::DELETE_Consent;
}

bool isEventCausedByRepresentative(AuditEventId eventId)
{
    return eventId == AuditEventId::POST_Task_abort_representative ||
           eventId == AuditEventId::GET_Task_id_representative;
}

bool isEventCausedByMaintenanceScript(AuditEventId eventId)
{
    return eventId == AuditEventId::Task_delete_expired_id ||
           eventId == AuditEventId::ChargeItem_delete_expired_id;
}

std::string createEventResourceReference(AuditEventId eventId, const std::string& resourceId)
{
    switch(eventId)
    {
        case model::AuditEventId::GET_Task:
        case model::AuditEventId::GET_Tasks_by_pharmacy_with_pz:
        case model::AuditEventId::GET_Tasks_by_pharmacy_without_pz:
        case model::AuditEventId::GET_Tasks_by_pharmacy_pnw_check_failed:
            return "Task";
        case model::AuditEventId::GET_Task_id_insurant:
        case model::AuditEventId::GET_Task_id_representative:
        case model::AuditEventId::GET_Task_id_pharmacy:
        case model::AuditEventId::Task_delete_expired_id:
            return "Task/" + resourceId;
        case model::AuditEventId::POST_Task_activate:
            return "Task/" + resourceId + "/$activate";
        case model::AuditEventId::POST_Task_accept:
            return "Task/" + resourceId + "/$accept";
        case model::AuditEventId::POST_Task_reject:
            return "Task/" + resourceId + "/$reject";
        case model::AuditEventId::POST_Task_close:
            return "Task/" + resourceId + "/$close";
        case model::AuditEventId::POST_Task_abort_doctor:
        case model::AuditEventId::POST_Task_abort_insurant:
        case model::AuditEventId::POST_Task_abort_representative:
        case model::AuditEventId::POST_Task_abort_pharmacy:
            return "Task/" + resourceId + "/$abort";
        case model::AuditEventId::GET_MedicationDispense:
            return "MedicationDispense";
        case model::AuditEventId::GET_MedicationDispense_id:
            return "MedicationDispense/" + resourceId;
        case model::AuditEventId::POST_ChargeItem:
        case model::AuditEventId::GET_ChargeItem_id_insurant:
        case model::AuditEventId::GET_ChargeItem_id_pharmacy:
        case model::AuditEventId::PATCH_ChargeItem_id:
        case model::AuditEventId::DELETE_ChargeItem_id_insurant:
        case model::AuditEventId::DELETE_ChargeItem_id_pharmacy:
        case model::AuditEventId::PUT_ChargeItem_id_insurant:
        case model::AuditEventId::PUT_ChargeItem_id_pharmacy:
        case model::AuditEventId::ChargeItem_delete_expired_id:
            return "ChargeItem/" + resourceId;
        case model::AuditEventId::POST_Consent:
        case model::AuditEventId::DELETE_Consent:
            return "Consent/" + resourceId;
    }
    Fail2("Invalid event id", std::logic_error);
}

namespace
{

const rapidjson::Pointer agentNamePointer("/an");
const rapidjson::Pointer agentWhoPointer("/aw");
const rapidjson::Pointer pnwPzNumberPointer("/pz");

}


AuditMetaData::AuditMetaData(const std::optional<std::string_view>& agentName,
                             const std::optional<std::string_view>& agentWho,
                             const std::optional<std::string_view>& pnwPzNumber)
: Resource<AuditMetaData>(Resource::NoProfile)
{
    if (agentName.has_value())
    {
        setValue(agentNamePointer, agentName.value());
    }

    if (agentWho.has_value())
    {
        setValue(agentWhoPointer, agentWho.value());
    }

    if (pnwPzNumber.has_value())
    {
        setValue(pnwPzNumberPointer, pnwPzNumber.value());
    }
}

std::optional<std::string_view> AuditMetaData::agentName() const
{
    return getOptionalStringValue(agentNamePointer);
}

std::optional<std::string_view> AuditMetaData::agentWho() const
{
    return getOptionalStringValue(agentWhoPointer);
}

std::optional<std::string_view> AuditMetaData::pnwPzNumber() const
{
    return getOptionalStringValue(pnwPzNumberPointer);
}

bool AuditMetaData::isEmpty() const
{
    return !agentName().has_value() && !agentWho().has_value() && !pnwPzNumber().has_value();
}

AuditMetaData::AuditMetaData(NumberAsStringParserDocument&& jsonTree)
    : Resource<AuditMetaData>(std::move(jsonTree))
{
}

//----------------------------------------------------------

AuditData::AuditData(
    AuditEventId eventId,
    AuditMetaData&& metaData,
    AuditEvent::Action action,
    AuditEvent::AgentType agentType,
    const std::string& insurantKvnr,
    const std::int16_t deviceId,
    std::optional<PrescriptionId> prescriptionId,
    std::optional<std::string> consentId)
    : mEventId(eventId)
    , mMetaData(std::move(metaData))
    , mAction(action)
    , mAgentType(agentType)
    , mInsurantKvnr(insurantKvnr)
    , mDeviceId(deviceId)
    , mPrescriptionId(std::move(prescriptionId))
    , mConsentId(std::move(consentId))
    , mId()
    , mRecorded(model::Timestamp::now())
{
}

AuditEvent::AgentType AuditData::agentType() const
{
    return mAgentType;
}

AuditEventId AuditData::eventId() const
{
    return mEventId;
}

const AuditMetaData& AuditData::metaData() const
{
    return mMetaData;
}

AuditEvent::Action AuditData::action() const
{
    return mAction;
}

const std::string& AuditData::insurantKvnr() const
{
    return mInsurantKvnr;
}

std::int16_t AuditData::deviceId() const
{
    return mDeviceId;
}

const std::optional<PrescriptionId>& AuditData::prescriptionId() const
{
    return mPrescriptionId;
}

const std::optional<std::string>& AuditData::consentId() const
{
    return mConsentId;
}

const std::string& AuditData::id() const
{
    return mId;
}

const model::Timestamp& AuditData::recorded() const
{
    return mRecorded;
}

void AuditData::setId(const std::string& id)
{
    mId = id;
}

void AuditData::setRecorded(const model::Timestamp& recorded)
{
    mRecorded = recorded;
}

} // namespace model
