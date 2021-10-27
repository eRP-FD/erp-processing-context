/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/AuditData.hxx"

#include <rapidjson/pointer.h>


namespace model
{

bool isEventCausedByPatient(AuditEventId eventId)
{
    return eventId == AuditEventId::GET_Task ||
           eventId == AuditEventId::GET_Task_id_insurant ||
           eventId == AuditEventId::POST_Task_abort_insurant ||
           eventId == AuditEventId::GET_MedicationDispense ||
           eventId == AuditEventId::GET_MedicationDispense_id;
}

bool isEventCausedByRepresentative(AuditEventId eventId)
{
    return eventId == AuditEventId::POST_Task_abort_representative ||
           eventId == AuditEventId::GET_Task_id_representative;
}


namespace
{

const rapidjson::Pointer agentNamePointer("/an");
const rapidjson::Pointer agentWhoPointer("/aw");

}


AuditMetaData::AuditMetaData(
    const std::optional<std::string_view>& agentName,
    const std::optional<std::string_view>& agentWho)
    : Resource<AuditMetaData>()
{
    if(agentName.has_value())
        setValue(agentNamePointer, agentName.value());
    if(agentWho.has_value())
        setValue(agentWhoPointer, agentWho.value());
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
    const std::string& insurantKvnr,
    const std::int16_t deviceId,
    std::optional<PrescriptionId> prescriptionId)
    : mEventId(eventId)
    , mMetaData(std::move(metaData))
    , mAction(action)
    , mAgentType(agentType)
    , mInsurantKvnr(insurantKvnr)
    , mDeviceId(deviceId)
    , mPrescriptionId(std::move(prescriptionId))
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
