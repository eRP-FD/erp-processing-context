/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_AUDITEVENTDATACOLLECTOR_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_AUDITEVENTDATACOLLECTOR_HXX

#include "shared/model/AuditData.hxx"
#include "shared/service/Operation.hxx"
#include "shared/model/PrescriptionId.hxx"
#include "shared/network/message/HttpStatus.hxx"
#include "shared/model/Kvnr.hxx"

#include <stdexcept>

class JWT;


class MissingAuditDataException : public std::runtime_error
{
public:
    explicit MissingAuditDataException(const std::string& message) :
        std::runtime_error(message)
    { }
};


class AuditDataCollector
{
public:
    AuditDataCollector& fillFromAccessToken(const JWT& accessToken);
    AuditDataCollector& setAgentWho(const std::string& agentWho);
    AuditDataCollector& setAgentName(const std::string& agentName);
    AuditDataCollector& setAgentType(model::AuditEvent::AgentType agentType);
    AuditDataCollector& setEventId(const model::AuditEventId eventId);
    AuditDataCollector& setAction(const model::AuditEvent::Action action);
    AuditDataCollector& setInsurantKvnr(const model::Kvnr& kvnr);
    AuditDataCollector& setDeviceId(const std::int16_t deviceId);
    AuditDataCollector& setPrescriptionId(const model::PrescriptionId& prescriptionId);
    AuditDataCollector& setConsentId(const std::string_view& consentId);

    // throws MissingAuditDataException if mandatory data is missing:
    model::AuditData createData() const;

    /**
     * Tells whether for this mEventId an audit event should be created
     */
    bool shouldCreateAuditEventOnSuccess() const noexcept;
    bool shouldCreateAuditEventOnError(HttpStatus errorCode) const noexcept;

private:
    std::optional<model::AuditEventId> mEventId;
    std::optional<model::AuditEvent::Action> mAction;
    std::optional<std::string> mAgentWho;    // TelematicId or Kvnr of accessing agent;
    std::optional<std::string> mAgentName;
    std::optional<model::AuditEvent::AgentType> mAgentType;
    std::optional<model::Kvnr> mInsurantKvnr;
    std::optional<std::int16_t> mDeviceId;
    std::optional<model::PrescriptionId> mPrescriptionId;
    std::optional<std::string> mConsentId;
};


#endif
