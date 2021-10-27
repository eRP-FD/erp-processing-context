/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_MODEL_AUDITDATA_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_AUDITDATA_HXX

#include "erp/model/Resource.hxx"
#include "erp/model/AuditEvent.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/model/Timestamp.hxx"
#include "erp/common/HttpStatus.hxx"


namespace model {

enum class AuditEventId : std::int16_t
{
    // Note: For any changes here be sure that the functions below are updated accordingly.
    GET_Task = 0,                    // only allowed by insurant;
    GET_Task_id_insurant,
    GET_Task_id_representative,
    GET_Task_id_pharmacy,
    POST_Task_activate,
    POST_Task_accept,
    POST_Task_reject,
    POST_Task_close,
    POST_Task_abort_doctor,
    POST_Task_abort_insurant,
    POST_Task_abort_representative,
    POST_Task_abort_pharmacy,
    GET_MedicationDispense,    // only allowed by insurant;
    GET_MedicationDispense_id, // only allowed by insurant;
    Task_delete_expired_id,    // deletion of expired tasks by maintenance script;
    MAX = Task_delete_expired_id
};

bool isEventCausedByPatient(AuditEventId eventId);
bool isEventCausedByRepresentative(AuditEventId eventId);


class AuditMetaData : public Resource<AuditMetaData>
{
public:
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
        const std::string& insurantKvnr,
        const std::int16_t deviceId,
        std::optional<PrescriptionId> prescriptionId);

    AuditEvent::AgentType agentType() const;
    AuditEventId eventId() const;
    const AuditMetaData& metaData() const;
    AuditEvent::Action action() const;
    const std::string& insurantKvnr() const;
    std::int16_t deviceId() const;
    const std::optional<model::PrescriptionId>& prescriptionId() const;
    const std::string& id() const;
    const model::Timestamp& recorded() const;

    void setId(const std::string& id);
    void setRecorded(const model::Timestamp& recorded);

private:
    AuditEventId mEventId;
    AuditMetaData mMetaData;
    AuditEvent::Action mAction;
    AuditEvent::AgentType mAgentType;
    std::string mInsurantKvnr;
    std::int16_t mDeviceId;
    std::optional<PrescriptionId> mPrescriptionId;

    std::string mId;            // filled after storing in or if loaded from DB;
    model::Timestamp mRecorded; // filled after storing in or if loaded from DB;
};

}  // namespace model

#endif
