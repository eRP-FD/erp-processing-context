/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/AuditData.hxx"
#include "shared/util/Expect.hxx"

#include <rapidjson/pointer.h>

#include <utility>
#include <map>


namespace model
{

bool isEventCausedByPatient(AuditEventId eventId)
{
    switch (eventId)
    {
        case AuditEventId::GET_Task:
        case AuditEventId::GET_Task_id_insurant:
        case AuditEventId::POST_Task_abort_insurant:
        case AuditEventId::GET_MedicationDispense:
        case AuditEventId::GET_MedicationDispense_id:
        case AuditEventId::DELETE_ChargeItem_id_insurant:
        case AuditEventId::PATCH_ChargeItem_id:
        case AuditEventId::GET_ChargeItem_id_insurant:
        case AuditEventId::POST_Consent:
        case AuditEventId::DELETE_Consent:
        case AuditEventId::POST_GRANT_EU_ACCESS_PERMISSION:
        case AuditEventId::DELETE_REVOKE_EU_ACCESS_PERMISSION:
        case AuditEventId::PATCH_TASK_ID_MARK:
        case AuditEventId::POST_EU_Consent:
        case AuditEventId::DELETE_EU_Consent:
            return true;
        case AuditEventId::GET_Task_id_representative:
        case AuditEventId::GET_Task_id_pharmacy:
        case AuditEventId::POST_Task_activate:
        case AuditEventId::POST_Task_accept:
        case AuditEventId::POST_Task_reject:
        case AuditEventId::POST_Task_close:
        case AuditEventId::POST_Task_abort_doctor:
        case AuditEventId::POST_Task_abort_representative:
        case AuditEventId::POST_Task_abort_pharmacy:
        case AuditEventId::Task_delete_expired_id:
        case AuditEventId::DELETE_ChargeItem_id_pharmacy:
        case AuditEventId::POST_ChargeItem:
        case AuditEventId::PUT_ChargeItem_id:
        case AuditEventId::ChargeItem_delete_expired_id:
        case AuditEventId::GET_ChargeItem_id_pharmacy:
        case AuditEventId::GET_Tasks_by_pharmacy_with_pz:
        case AuditEventId::GET_Tasks_by_pharmacy_pnw_check_failed:
        case AuditEventId::Communication_delete_expired_id:
        case AuditEventId::GET_Tasks_by_pharmacy_with_pn3:
        case AuditEventId::GET_Tasks_by_pharmacy_with_pn3_failed:
        case AuditEventId::POST_Task_dispense:
        case AuditEventId::POST_PROVIDE_PRESCRIPTION_ERP:
        case AuditEventId::POST_PROVIDE_PRESCRIPTION_ERP_failed:
        case AuditEventId::POST_PROVIDE_DISPENSATION_ERP:
        case AuditEventId::POST_PROVIDE_DISPENSATION_ERP_failed:
        case AuditEventId::POST_CANCEL_PRESCRIPTION_ERP:
        case AuditEventId::POST_CANCEL_PRESCRIPTION_ERP_failed:
        case AuditEventId::POST_CANCEL_DISPENSATION_REP:
        case AuditEventId::POST_CANCEL_DISPENSATION_REP_failed:
        case AuditEventId::POST_GET_EU_PRESCRIPTIONS_DEMOGRAPHICS:
        case AuditEventId::POST_GET_EU_PRESCRIPTIONS_E_PRESCRIPTIONS_LIST:
        case AuditEventId::POST_GET_EU_PRESCRIPTIONS_E_PRESCRIPTIONS_RETRIEVAL:
        case AuditEventId::POST_TASK_EU_CLOSE:
        case AuditEventId::GET_Tasks_by_pharmacy_with_popp:
            break;
    }
    return false;
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
    switch (eventId)
    {
        case model::AuditEventId::GET_Task:
        case model::AuditEventId::GET_Tasks_by_pharmacy_with_pz:
        case model::AuditEventId::GET_Tasks_by_pharmacy_pnw_check_failed:
        case model::AuditEventId::GET_Tasks_by_pharmacy_with_pn3:
        case model::AuditEventId::GET_Tasks_by_pharmacy_with_pn3_failed:
        case AuditEventId::GET_Tasks_by_pharmacy_with_popp:
            return "Task";
        case model::AuditEventId::GET_Task_id_insurant:
        case model::AuditEventId::GET_Task_id_representative:
        case model::AuditEventId::GET_Task_id_pharmacy:
        case model::AuditEventId::Task_delete_expired_id:
        case model::AuditEventId::POST_Task_activate:
        case model::AuditEventId::POST_Task_accept:
        case model::AuditEventId::POST_Task_reject:
        case model::AuditEventId::POST_Task_dispense:
        case model::AuditEventId::POST_Task_close:
        case model::AuditEventId::POST_Task_abort_doctor:
        case model::AuditEventId::POST_Task_abort_insurant:
        case model::AuditEventId::POST_Task_abort_representative:
        case model::AuditEventId::POST_Task_abort_pharmacy:
        case model::AuditEventId::POST_PROVIDE_PRESCRIPTION_ERP_failed:
        case model::AuditEventId::POST_PROVIDE_PRESCRIPTION_ERP:
        case model::AuditEventId::POST_CANCEL_PRESCRIPTION_ERP_failed:
        case model::AuditEventId::POST_CANCEL_PRESCRIPTION_ERP:
        case model::AuditEventId::PATCH_TASK_ID_MARK:
        case model::AuditEventId::POST_TASK_EU_CLOSE:
            return "Task/" + resourceId;
        case model::AuditEventId::GET_MedicationDispense:
            return "MedicationDispense";
        case model::AuditEventId::GET_MedicationDispense_id:
        case model::AuditEventId::POST_PROVIDE_DISPENSATION_ERP_failed:
        case model::AuditEventId::POST_PROVIDE_DISPENSATION_ERP:
        case model::AuditEventId::POST_CANCEL_DISPENSATION_REP_failed:
        case model::AuditEventId::POST_CANCEL_DISPENSATION_REP:
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
        case model::AuditEventId::POST_EU_Consent:
        case model::AuditEventId::DELETE_EU_Consent:
            return "Consent/" + resourceId;
        case model::AuditEventId::Communication_delete_expired_id:
            return "Communication";
        case AuditEventId::POST_GRANT_EU_ACCESS_PERMISSION:
            return "$grant-eu-access-permission";
        case AuditEventId::DELETE_REVOKE_EU_ACCESS_PERMISSION:
            return "$revoke-eu-access-permission";
        case AuditEventId::POST_GET_EU_PRESCRIPTIONS_DEMOGRAPHICS:
        case AuditEventId::POST_GET_EU_PRESCRIPTIONS_E_PRESCRIPTIONS_LIST:
        case AuditEventId::POST_GET_EU_PRESCRIPTIONS_E_PRESCRIPTIONS_RETRIEVAL:
            return "$get-eu-prescriptions";
    }
    Fail2("Invalid event id", std::logic_error);
}

bool isConsentEvent(AuditEventId eventId)
{
    return eventId == AuditEventId::POST_Consent || eventId == AuditEventId::DELETE_Consent ||
           eventId == AuditEventId::POST_EU_Consent || eventId == AuditEventId::DELETE_EU_Consent;
}

namespace
{

const rapidjson::Pointer agentNamePointer("/an");
const rapidjson::Pointer agentWhoPointer("/aw");
const rapidjson::Pointer countryCodePointer("/cc");

}


AuditMetaData::AuditMetaData(const std::optional<std::string_view>& agentName,
                             const std::optional<std::string_view>& agentWho,
                             const std::optional<std::string_view>& countryCode,
                             const std::map<std::string, std::string>& variables)
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

    if (countryCode.has_value())
    {
        setValue(countryCodePointer, countryCode.value());
    }
    for (const auto& [key, value] : variables)
    {
        setValue(rapidjson::Pointer("/" + key), value);
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

std::optional<std::string_view> AuditMetaData::countryCode() const
{
    return getOptionalStringValue(countryCodePointer);
}

std::map<std::string, std::string> AuditMetaData::variables() const
{
    std::map<std::string, std::string> result;
    for (auto it = jsonDocument().MemberBegin(); it != jsonDocument().MemberEnd(); ++it)
    {
        if (it->value.IsString() && it->name != "an" && it->name != "aw" && it->name != "cc")
        {
            result.emplace(it->name.GetString(), getStringValue(it->value, rapidjson::Pointer{""}));
        }
    }
    return result;
}

bool AuditMetaData::isEmpty() const
{
    return !agentName().has_value() && !agentWho().has_value() && !countryCode().has_value();
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

std::optional<CountryCode> AuditData::countryCode() const
{
    if (const auto cc = metaData().countryCode(); cc.has_value() && ! cc->empty())
    {
        return CountryCode{std::string{*cc}};
    }
    return std::nullopt;
}

std::map<std::string, std::string> AuditData::variables() const
{
    return metaData().variables();
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
