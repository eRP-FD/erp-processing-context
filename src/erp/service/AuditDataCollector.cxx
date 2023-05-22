/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/AuditDataCollector.hxx"

#include "erp/ErpRequirements.hxx"
#include "erp/crypto/Jwt.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/String.hxx"

namespace
{

template<class T>
const T& assertHasValue(const std::optional<T>& var)
{
    Expect3(var.has_value(), "Audit data field not filled", MissingAuditDataException);
    return var.value();
}

} // anonymous namespace


AuditDataCollector& AuditDataCollector::fillFromAccessToken(const JWT& accessToken)
{
    A_19391.start("Use name of caller for audit logging");
    A_19392.start("Use id of caller for audit logging");

    auto idNumberClaim = accessToken.stringForClaim(JWT::idNumberClaim);
    Expect3(idNumberClaim.has_value(), "Missing idNumberClaim", std::logic_error);
    mAgentWho = std::move(idNumberClaim);

    const auto professionOIDClaim = accessToken.stringForClaim(JWT::professionOIDClaim);
    Expect3(professionOIDClaim.has_value(), "Missing professionOIDClaim", std::logic_error);

    if (professionOIDClaim.value() == profession_oid::oid_versicherter)
    {
        const auto givenNameClaim = accessToken.stringForClaim(JWT::givenNameClaim);
        Expect3(givenNameClaim.has_value(), "Missing givenNameClaim", std::logic_error);
        const auto familyNameClaim = accessToken.stringForClaim(JWT::familyNameClaim);
        Expect3(familyNameClaim.has_value(), "Missing familyNameClaim", std::logic_error);
        mAgentName = givenNameClaim.value() + (givenNameClaim.value().empty() ? "" : " ") + familyNameClaim.value();
    }
    else
    {
        mAgentName = accessToken.stringForClaim(JWT::organizationNameClaim);
    }

    A_19391.finish();
    A_19392.finish();

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
        assertHasValue(mAction), model::AuditEvent::AgentType::human, assertHasValue(mInsurantKvnr),
        assertHasValue(mDeviceId), mPrescriptionId, mConsentId);
}

bool AuditDataCollector::shouldCreateAuditEventOnSuccess() const noexcept
{
    return mEventId.has_value();
}

bool AuditDataCollector::shouldCreateAuditEventOnError(HttpStatus errorCode) const noexcept
{
    return errorCode == HttpStatus::Forbidden &&
           mEventId.has_value() &&
           *mEventId == model::AuditEventId::GET_Tasks_by_pharmacy_pnw_check_failed;
}
