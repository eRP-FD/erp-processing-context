/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
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
           eventId == AuditEventId::ChargeItem_delete_expired_id ||
           eventId == AuditEventId::Communication_delete_expired_id;
}

std::string createEventResourceReference(AuditEventId eventId, const std::string& resourceId)
{
    switch(eventId)
    {
        case model::AuditEventId::GET_Task:
        case model::AuditEventId::GET_Tasks_by_pharmacy_with_pz:
        case model::AuditEventId::GET_Tasks_by_pharmacy_pnw_check_failed:
        case model::AuditEventId::GET_Tasks_by_pharmacy_with_pn3:
        case model::AuditEventId::GET_Tasks_by_pharmacy_with_pn3_failed:
            return "Task";
        case model::AuditEventId::GET_Task_id_insurant:
        case model::AuditEventId::GET_Task_id_representative:
        case model::AuditEventId::GET_Task_id_pharmacy:
        case model::AuditEventId::Task_delete_expired_id:
        case model::AuditEventId::POST_Task_activate:
        case model::AuditEventId::POST_Task_accept:
        case model::AuditEventId::POST_Task_reject:
        case model::AuditEventId::POST_Task_close:
        case model::AuditEventId::POST_Task_abort_doctor:
        case model::AuditEventId::POST_Task_abort_insurant:
        case model::AuditEventId::POST_Task_abort_representative:
        case model::AuditEventId::POST_Task_abort_pharmacy:
            return "Task/" + resourceId;
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
        case model::AuditEventId::PUT_ChargeItem_id:
        case model::AuditEventId::ChargeItem_delete_expired_id:
            return "ChargeItem/" + resourceId;
        case model::AuditEventId::POST_Consent:
        case model::AuditEventId::DELETE_Consent:
            return "Consent/" + resourceId;
        case model::AuditEventId::Communication_delete_expired_id:
            return "Communication";

    }
    Fail2("Invalid event id", std::logic_error);
}

namespace
{

const rapidjson::Pointer agentNamePointer("/an");
const rapidjson::Pointer agentWhoPointer("/aw");

}


AuditMetaData::AuditMetaData(const std::optional<std::string_view>& agentName,
                             const std::optional<std::string_view>& agentWho)
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
}

std::optional<std::string_view> AuditMetaData::agentName() const
{
    return getOptionalStringValue(agentNamePointer);
}

std::optional<std::string_view> AuditMetaData::agentWho() const
{
    return getOptionalStringValue(agentWhoPointer);
}

bool AuditMetaData::isEmpty() const
{
    return !agentName().has_value() && !agentWho().has_value();
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
    const Kvnr& insurantKvnr,
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

const Kvnr& AuditData::insurantKvnr() const
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

bool AuditData::isValidEventId() const
{
    return (mEventId >= AuditEventId::MIN && mEventId <= AuditEventId::MAX);
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
