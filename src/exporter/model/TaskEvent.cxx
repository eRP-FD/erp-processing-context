/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/model/TaskEvent.hxx"
#include "shared/model/KbvMedicationRequest.hxx"

namespace model
{

TaskEventBase::TaskEventBase(id_t id, const PrescriptionId& prescriptionId, PrescriptionType prescriptionType, Kvnr kvnr,
                             std::string_view hashedKvnr, Bundle&& kbvBundle, const model::Timestamp& lastModified, std::int32_t retryCount /* = 0 */)
    : mId(id)
    , mPrescriptionId(prescriptionId)
    , mPrescriptionType(prescriptionType)
    , mKvnr(std::move(kvnr))
    , mHashedKvnr(hashedKvnr)
    , mKbvBundle(std::move(kbvBundle))
    , mLastModified(lastModified)
    , mRetryCount(retryCount)
{
}


TaskEvent::id_t TaskEventBase::getId() const
{
    return mId;
}

const PrescriptionId& TaskEventBase::getPrescriptionId() const
{
    return mPrescriptionId;
}

const Kvnr& TaskEventBase::getKvnr() const
{
    return mKvnr;
}

const std::string& TaskEventBase::getHashedKvnr() const
{
    return mHashedKvnr;
}

PrescriptionType TaskEventBase::getPrescriptionType() const
{
    return mPrescriptionType;
}

const Bundle& TaskEventBase::getKbvBundle() const
{
    return mKbvBundle;
}

Timestamp TaskEventBase::getMedicationRequestAuthoredOn() const
{
    auto kbvMedicationRequest = mKbvBundle.getUniqueResourceByType<model::KbvMedicationRequest>();
    return kbvMedicationRequest.authoredOn();
}

Timestamp TaskEventBase::getLastModified() const
{
    return mLastModified;
}

std::int32_t TaskEventBase::getRetryCount() const
{
    return mRetryCount;
}

const std::string& TaskEventBase::getXRequestId() const
{
    return mXRequestId;
}

TaskEvent::TaskEvent(id_t id, const PrescriptionId& prescriptionId, PrescriptionType prescriptionType, const Kvnr& kvnr,
                     std::string_view hashedKvnr, Bundle&& kbvBundle, const model::Timestamp& lastModified,
                     TaskEvent::UseCase useCase, State state)
    : TaskEventBase(id, prescriptionId, prescriptionType, kvnr, hashedKvnr, std::move(kbvBundle), lastModified)
    , mUseCase(useCase)
    , mState(state)
{
}

TaskEvent::UseCase TaskEvent::getUseCase() const
{
    return mUseCase;
}

TaskEvent::State TaskEvent::getState() const
{
    return mState;
}

ProvidePrescriptionTaskEvent::ProvidePrescriptionTaskEvent(
    id_t id, const PrescriptionId& prescriptionId, PrescriptionType prescriptionType, const Kvnr& kvnr,
    std::string_view hashedKvnr, TaskEvent::UseCase useCase, State state, const std::optional<TelematikId>& qesDoctorId,
    const TelematikId& jwtDoctorId, const std::string& jwtDoctorOrganizationName,
    const std::string& jwtDoctorProfessionOid, Bundle&& kbvBundle, const model::Timestamp& lastModified)
    : TaskEvent(id, prescriptionId, prescriptionType, kvnr, hashedKvnr, std::move(kbvBundle),
                lastModified, useCase, state)
    , mQesDoctorId(qesDoctorId)
    , mJwtDoctorId(jwtDoctorId)
    , mJwtDoctorOrganizationName(jwtDoctorOrganizationName)
    , mJwtDoctorOrganizationOid(jwtDoctorProfessionOid)
{
}

const std::optional<TelematikId>& ProvidePrescriptionTaskEvent::getQesDoctorId() const
{
    return mQesDoctorId;
}

const TelematikId& ProvidePrescriptionTaskEvent::getJwtDoctorId() const
{
    return mJwtDoctorId;
}

const std::string& ProvidePrescriptionTaskEvent::getJwtDoctorOrganizationName() const
{
    return mJwtDoctorOrganizationName;
}


const std::string& ProvidePrescriptionTaskEvent::getJwtDoctorProfessionOid() const
{
    return mJwtDoctorOrganizationOid;
}


ProvideDispensationTaskEvent::ProvideDispensationTaskEvent(
    id_t id, const PrescriptionId& prescriptionId, PrescriptionType prescriptionType, const Kvnr& kvnr,
    std::string_view hashedKvnr, TaskEvent::UseCase useCase, State state, const std::optional<TelematikId>& qesDocotorId,
    const TelematikId& jwtDoctorId, const std::string& jwtDoctorOrganizationName,
    const std::string& jwtDoctorProfessionOid, const TelematikId& jwtPharmacyId,
    const std::string& jwtPharmacyOrganizationName, const std::string& jwtPharmacyProfessionOid, Bundle&& kbvBundle,
    Bundle&& medicationDispenseBundle, const model::Timestamp& lastModified)
    : ProvidePrescriptionTaskEvent(id, prescriptionId, prescriptionType, kvnr, hashedKvnr, useCase, state, qesDocotorId,
                                   jwtDoctorId, jwtDoctorOrganizationName, jwtDoctorProfessionOid, std::move(kbvBundle),
                                   lastModified)
    , mJwtPharmacyId(jwtPharmacyId)
    , mJwtPharmacyOrganizationName(jwtPharmacyOrganizationName)
    , mJwtPharmacyOrganizationOid(jwtPharmacyProfessionOid)
    , mMedicationDispenseBundle(std::move(medicationDispenseBundle))
{
}

const Bundle& ProvideDispensationTaskEvent::getMedicationDispenseBundle() const
{
    return mMedicationDispenseBundle;
}

const TelematikId& ProvideDispensationTaskEvent::getJwtPharmacyId() const
{
    return mJwtPharmacyId;
}

const std::string& ProvideDispensationTaskEvent::getJwtPharmacyOrganizationName() const
{
    return mJwtPharmacyOrganizationName;
}

const std::string& ProvideDispensationTaskEvent::getJwtPharmacyProfessionOid() const
{
    return mJwtPharmacyOrganizationOid;
}

CancelPrescriptionTaskEvent::CancelPrescriptionTaskEvent(id_t id, const PrescriptionId& prescriptionId,
                                                         PrescriptionType prescriptionType, const Kvnr& kvnr,
                                                         std::string_view hashedKvnr, TaskEvent::UseCase useCase,
                                                         State state, Bundle&& kbvBundle,
                                                         const model::Timestamp& lastModified)
    : TaskEvent(id, prescriptionId, prescriptionType, kvnr, hashedKvnr, std::move(kbvBundle),
                lastModified, useCase, state)
{
}

CancelDispensationTaskEvent::CancelDispensationTaskEvent(id_t id, const PrescriptionId& prescriptionId,
                                                         PrescriptionType prescriptionType, const Kvnr& kvnr,
                                                         std::string_view hashedKvnr, TaskEvent::UseCase useCase,
                                                         State state, Bundle&& kbvBundle,
                                                         const model::Timestamp& lastModified)
    : TaskEvent(id, prescriptionId, prescriptionType, kvnr, hashedKvnr, std::move(kbvBundle),
                lastModified, useCase, state)
{
}

TRezeptEvent::TRezeptEvent(id_t id, const PrescriptionId& prescriptionId, PrescriptionType prescriptionType,
                           const Kvnr& kvnr, std::string_view hashedKvnr, Bundle&& kbvBundle,
                           const model::Timestamp& lastModified, TaskEvent::UseCase useCase, State state,
                           model::TelematikId orgTelematikId,
                           Bundle&& medicationDispenseBundle,
                           model::Timestamp qesSigningTime,
                           std::int32_t retryCount)
    : TaskEventBase(id, prescriptionId, prescriptionType, kvnr, hashedKvnr, std::move(kbvBundle), lastModified, retryCount)
    , mUseCase(useCase)
    , mState(state)
    , mOrgTelematikId(std::move(orgTelematikId))
    , mMedicationDispenseBundle(std::move(medicationDispenseBundle))
    , mQesSigningTime(qesSigningTime)
{
}

TaskEvent::UseCase TRezeptEvent::getUseCase() const
{
    return mUseCase;
}

TRezeptEvent::State TRezeptEvent::getState() const
{
    return mState;
}

const model::TelematikId& TRezeptEvent::orgTelematikId() const
{
    return mOrgTelematikId;
}

const Bundle& TRezeptEvent::getMedicationDispenseBundle() const
{
    return mMedicationDispenseBundle;
}

const Timestamp& TRezeptEvent::getQesSigningTime() const
{
    return mQesSigningTime;
}

}
