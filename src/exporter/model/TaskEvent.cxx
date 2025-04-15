/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/model/TaskEvent.hxx"
#include "shared/model/KbvMedicationRequest.hxx"

namespace model
{

TaskEvent::TaskEvent(id_t id, const PrescriptionId& prescriptionId, PrescriptionType prescriptionType, const Kvnr& kvnr,
                     std::string_view hashedKvnr, TaskEvent::UseCase useCase, State state, Bundle&& kbvBundle,
                     const model::Timestamp& lastModified)
    : mId(id)
    , mPrescriptionId(prescriptionId)
    , mPrescriptionType(prescriptionType)
    , mKvnr(kvnr)
    , mHashedKvnr(hashedKvnr)
    , mUseCase(useCase)
    , mState(state)
    , mKbvBundle(std::move(kbvBundle))
    , mLastModified(lastModified)
{
}

TaskEvent::id_t TaskEvent::getId() const
{
    return mId;
}

const PrescriptionId& TaskEvent::getPrescriptionId() const
{
    return mPrescriptionId;
}

const Kvnr& TaskEvent::getKvnr() const
{
    return mKvnr;
}

const std::string& TaskEvent::getHashedKvnr() const
{
    return mHashedKvnr;
}

TaskEvent::UseCase TaskEvent::getUseCase() const
{
    return mUseCase;
}

TaskEvent::State TaskEvent::getState() const
{
    return mState;
}

PrescriptionType TaskEvent::getPrescriptionType() const
{
    return mPrescriptionType;
}

const Bundle& TaskEvent::getKbvBundle() const
{
    return mKbvBundle;
}

Timestamp TaskEvent::getMedicationRequestAuthoredOn() const
{
    auto kbvMedicationRequest = mKbvBundle.getUniqueResourceByType<model::KbvMedicationRequest>();
    return kbvMedicationRequest.authoredOn();
}

Timestamp TaskEvent::getLastModified() const
{
    return mLastModified;
}

std::int32_t TaskEvent::getRetryCount() const
{
    return mRetryCount;
}

const std::string& TaskEvent::getXRequestId() const
{
    return mXRequestId;
}

ProvidePrescriptionTaskEvent::ProvidePrescriptionTaskEvent(
    id_t id, const PrescriptionId& prescriptionId, PrescriptionType prescriptionType, const Kvnr& kvnr,
    std::string_view hashedKvnr, TaskEvent::UseCase useCase, State state, const std::optional<TelematikId>& qesDoctorId,
    const TelematikId& jwtDoctorId, const std::string& jwtDoctorOrganizationName,
    const std::string& jwtDoctorProfessionOid, Bundle&& kbvBundle, const model::Timestamp& lastModified)
    : TaskEvent(id, prescriptionId, prescriptionType, kvnr, hashedKvnr, useCase, state, std::move(kbvBundle),
                lastModified)
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
    : TaskEvent(id, prescriptionId, prescriptionType, kvnr, hashedKvnr, useCase, state, std::move(kbvBundle),
                lastModified)
{
}

CancelDispensationTaskEvent::CancelDispensationTaskEvent(id_t id, const PrescriptionId& prescriptionId,
                                                         PrescriptionType prescriptionType, const Kvnr& kvnr,
                                                         std::string_view hashedKvnr, TaskEvent::UseCase useCase,
                                                         State state, Bundle&& kbvBundle,
                                                         const model::Timestamp& lastModified)
    : TaskEvent(id, prescriptionId, prescriptionType, kvnr, hashedKvnr, useCase, state, std::move(kbvBundle),
                lastModified)
{
}

}
