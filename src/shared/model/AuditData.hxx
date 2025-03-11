/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_AUDITDATA_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_AUDITDATA_HXX

#include "shared/model/AuditEvent.hxx"
#include "shared/model/PrescriptionId.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/network/message/HttpStatus.hxx"


namespace model {

enum class AuditEventId : std::int16_t
{
    // Note: For any changes here be sure that the functions below are updated accordingly.
    MIN = 0,
    GET_Task = MIN,            // only allowed by insurant;
    GET_Task_id_insurant = 1,
    GET_Task_id_representative = 2,
    GET_Task_id_pharmacy = 3,
    POST_Task_activate = 4,
    POST_Task_accept = 5,
    POST_Task_reject = 6,
    POST_Task_close = 7,
    POST_Task_abort_doctor = 8,
    POST_Task_abort_insurant = 9,
    POST_Task_abort_representative = 10,
    POST_Task_abort_pharmacy = 11,
    GET_MedicationDispense = 12,    // only allowed by insurant;
    GET_MedicationDispense_id = 13, // only allowed by insurant;
    Task_delete_expired_id = 14,    // deletion of expired tasks by maintenance script => Id 14 used by database script!
    DELETE_ChargeItem_id_insurant = 15,
    DELETE_ChargeItem_id_pharmacy = 16,
    POST_ChargeItem = 17,           // always caused by pharmacy;
    // PUT_ChargeItem_id_insurant (id 18) no longer used;
    PUT_ChargeItem_id = 19,
    POST_Consent = 20,              // only allowed by insurant;
    DELETE_Consent = 21,         // only allowed by insurant;
    ChargeItem_delete_expired_id = 22, // deletion of expired ChargeItems by maintenance script => Id 22 used by database script!
    GET_ChargeItem_id_insurant = 23,
    GET_ChargeItem_id_pharmacy = 24,
    PATCH_ChargeItem_id = 25,
    GET_Tasks_by_pharmacy_with_pz = 26,
    // 27 left out
    GET_Tasks_by_pharmacy_pnw_check_failed = 28,
    Communication_delete_expired_id = 29, // deletion of expired Communications by maintenance script => Id 29 used by database script!
    GET_Tasks_by_pharmacy_with_pn3 = 30,
    GET_Tasks_by_pharmacy_with_pn3_failed = 31,
    POST_Task_dispense = 32,
    POST_PROVIDE_PRESCRIPTION_ERP = 33,
    POST_PROVIDE_PRESCRIPTION_ERP_failed = 34,
    POST_PROVIDE_DISPENSATION_ERP = 35,
    POST_PROVIDE_DISPENSATION_ERP_failed = 36,
    POST_CANCEL_PRESCRIPTION_ERP = 37,
    POST_CANCEL_PRESCRIPTION_ERP_failed = 38,
    POST_CANCEL_DISPENSATION_REP = 39,
    POST_CANCEL_DISPENSATION_REP_failed = 40,
    MAX = POST_CANCEL_DISPENSATION_REP_failed
};

bool isEventCausedByPatient(AuditEventId eventId);
bool isEventCausedByRepresentative(AuditEventId eventId);
bool isEventCausedByMaintenanceScript(AuditEventId eventId);
std::string createEventResourceReference(AuditEventId eventId, const std::string& resourceId);


// NOLINTNEXTLINE(bugprone-exception-escape)
class AuditMetaData : public Resource<AuditMetaData>
{
public:
    static constexpr auto resourceTypeName = "AuditMetaData";

    AuditMetaData(
        const std::optional<std::string_view>& agentName,
        const std::optional<std::string_view>& agentWho);

    std::optional<std::string_view> agentName() const;
    std::optional<std::string_view> agentWho() const;

    bool isEmpty() const;

private:
    friend Resource<AuditMetaData>;
    explicit AuditMetaData(NumberAsStringParserDocument&& jsonTree);
};


class AuditData
{
public:
    AuditData(const AuditData &) = delete;
    AuditData(AuditData &&) = default;

    AuditData(
        AuditEventId eventId,
        AuditMetaData&& metaData,
        AuditEvent::Action action,
        AuditEvent::AgentType agentType,
        const Kvnr& insurantKvnr,
        const std::int16_t deviceId,
        std::optional<PrescriptionId> prescriptionId,
        std::optional<std::string> consentId);

    AuditEvent::AgentType agentType() const;
    AuditEventId eventId() const;
    const AuditMetaData& metaData() const;
    AuditEvent::Action action() const;
    const Kvnr& insurantKvnr() const;
    std::int16_t deviceId() const;
    const std::optional<model::PrescriptionId>& prescriptionId() const;
    const std::optional<std::string>& consentId() const;
    const std::string& id() const;
    const model::Timestamp& recorded() const;

    bool isValidEventId() const;

    void setId(const std::string& id);
    void setRecorded(const model::Timestamp& recorded);

private:
    AuditEventId mEventId;
    AuditMetaData mMetaData;
    AuditEvent::Action mAction;
    AuditEvent::AgentType mAgentType;
    Kvnr mInsurantKvnr;
    std::int16_t mDeviceId;
    std::optional<PrescriptionId> mPrescriptionId;
    std::optional<std::string> mConsentId;

    std::string mId;            // filled after storing in or if loaded from DB;
    model::Timestamp mRecorded; // filled after storing in or if loaded from DB;
};

extern template class Resource<class AuditMetaData>;

}  // namespace model

#endif
