/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/audit/AuditDataCollector.hxx"

#include "shared/ErpRequirements.hxx"
#include "shared/crypto/Jwt.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/String.hxx"

namespace
{

template<class T>
const T& assertHasValue(const std::optional<T>& var)
{
    Expect3(var.has_value(), "Audit data field not filled", MissingAuditDataException);
    return var.value();
}

}// anonymous namespace


AuditDataCollector& AuditDataCollector::fillFromAccessToken(const JWT& accessToken)
{
    A_19391_01.start("Use name of caller for audit logging");
    A_19392.start("Use id of caller for audit logging");

    auto idNumberClaim = accessToken.stringForClaim(JWT::idNumberClaim);
    Expect3(idNumberClaim.has_value(), "Missing idNumberClaim", std::logic_error);
    mAgentWho = std::move(idNumberClaim);
    mAgentName = accessToken.displayName();

    const auto professionOIDClaim = accessToken.stringForClaim(JWT::professionOIDClaim);
    Expect3(professionOIDClaim.has_value(), "Missing professionOIDClaim", std::logic_error);
    if (professionOIDClaim.value() == profession_oid::oid_versicherter)
    {
        Expect3(mAgentName.has_value(), "Unable to determine display name", std::logic_error);
    }

    A_19391_01.finish();
    A_19392.finish();

    return *this;
}

AuditDataCollector& AuditDataCollector::setAgentWho(const std::string& agentWho)
{
    mAgentWho = agentWho;
    return *this;
}

AuditDataCollector& AuditDataCollector::setAgentName(const std::string& agentName)
{
    mAgentName = agentName;
    return *this;
}

AuditDataCollector& AuditDataCollector::setAgentType(model::AuditEvent::AgentType agentType)
{
    mAgentType = agentType;
    return *this;
}

AuditDataCollector& AuditDataCollector::setEventId(const model::AuditEventId eventId)
{
    mEventId = eventId;
    return *this;
}

AuditDataCollector& AuditDataCollector::setAction(const model::AuditEvent::Action action)
{
    mAction = action;
    return *this;
}

AuditDataCollector& AuditDataCollector::setInsurantKvnr(const model::Kvnr& kvnr)
{
    mInsurantKvnr = kvnr;
    return *this;
}

AuditDataCollector& AuditDataCollector::setDeviceId(const std::int16_t deviceId)
{
    mDeviceId = deviceId;
    return *this;
}

AuditDataCollector& AuditDataCollector::setPrescriptionId(const model::PrescriptionId& prescriptionId)
{
    mPrescriptionId.emplace(prescriptionId);
    return *this;
}

AuditDataCollector& AuditDataCollector::setConsentId(const std::string_view& consentId)
{
    mConsentId = consentId;
    return *this;
}

model::AuditData AuditDataCollector::createData() const
{
    Expect3(mEventId.has_value(), "Event ID should not be missing", std::logic_error);

    const bool isEventCausedByPatient = model::isEventCausedByPatient(*mEventId);
    return model::AuditData(
        *mEventId,
        model::AuditMetaData(isEventCausedByPatient                            ? std::optional<std::string>()
                             : model::isEventCausedByRepresentative(*mEventId) ? assertHasValue(mAgentName)
                                                                               : mAgentName,
                             isEventCausedByPatient ? std::optional<std::string>() : assertHasValue(mAgentWho)),
        assertHasValue(mAction), mAgentType.value_or(model::AuditEvent::AgentType::human),
        assertHasValue(mInsurantKvnr), assertHasValue(mDeviceId), mPrescriptionId, mConsentId);
}

bool AuditDataCollector::shouldCreateAuditEventOnSuccess() const noexcept
{
    return mEventId.has_value();
}

bool AuditDataCollector::shouldCreateAuditEventOnError(HttpStatus errorCode) const noexcept
{
    return (mEventId.has_value() && *mEventId == model::AuditEventId::GET_Tasks_by_pharmacy_pnw_check_failed) ||
           (errorCode == HttpStatus::NotAcceptPN3 && mEventId.has_value() &&
            *mEventId == model::AuditEventId::GET_Tasks_by_pharmacy_with_pn3_failed);
}
