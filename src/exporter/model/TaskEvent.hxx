/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_TASKEVENT_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_TASKEVENT_HXX

#include "erp/crypto/SignedPrescription.hxx"
#include "shared/model/Bundle.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/model/PrescriptionId.hxx"
#include "shared/model/TelematikId.hxx"
#include "shared/model/Timestamp.hxx"

#include <date/date.h>

namespace model
{


class TaskEvent
{
public:
    enum class UseCase : uint8_t
    {
        providePrescription,
        cancelPrescription,
        provideDispensation,
        cancelDispensation
    };
    enum class State : uint8_t
    {
        pending,
        deadLetterQueue
    };

    using id_t = std::int64_t;

    TaskEvent(id_t id, const PrescriptionId& prescriptionId, PrescriptionType prescriptionType, const Kvnr& kvnr,
              std::string_view hashedKvnr, UseCase useCase, State state, Bundle&& kbvBundle,
              const model::Timestamp& lastModified);
    virtual ~TaskEvent() = default;

    virtual id_t getId() const;
    virtual const PrescriptionId& getPrescriptionId() const;
    virtual PrescriptionType getPrescriptionType() const;
    virtual const Kvnr& getKvnr() const;
    virtual const std::string& getHashedKvnr() const;
    virtual UseCase getUseCase() const;
    virtual State getState() const;
    virtual const Bundle& getKbvBundle() const;
    virtual Timestamp getMedicationRequestAuthoredOn() const;
    virtual Timestamp getLastModified() const;
    virtual std::int32_t getRetryCount() const;

private:
    id_t mId;
    PrescriptionId mPrescriptionId;
    PrescriptionType mPrescriptionType;
    Kvnr mKvnr;
    std::string mHashedKvnr;
    UseCase mUseCase;
    State mState;
    Bundle mKbvBundle;
    model::Timestamp mLastModified;
    std::int32_t mRetryCount{0};
};


class ProvidePrescriptionTaskEvent : public TaskEvent
{

public:
    ProvidePrescriptionTaskEvent(id_t id, const PrescriptionId& prescriptionId, PrescriptionType prescriptionType,
                                 const Kvnr& kvnr, std::string_view hashedKvnr, TaskEvent::UseCase useCase, State state,
                                 const std::optional<TelematikId>& qesDoctorId, const TelematikId& jwtDoctorId,
                                 const std::string& jwtDoctorOrganizationName,
                                 const std::string& jwtDoctorProfessionOid, Bundle&& kbvBundle,
                                 const model::Timestamp& lastModified);

    virtual const std::optional<TelematikId>& getQesDoctorId() const;
    virtual const TelematikId& getJwtDoctorId() const;
    virtual const std::string& getJwtDoctorOrganizationName() const;
    virtual const std::string& getJwtDoctorProfessionOid() const;

private:
    std::optional<TelematikId> mQesDoctorId;
    TelematikId mJwtDoctorId;
    std::string mJwtDoctorOrganizationName;
    std::string mJwtDoctorOrganizationOid;
};


class ProvideDispensationTaskEvent : public ProvidePrescriptionTaskEvent
{

public:
    ProvideDispensationTaskEvent(id_t id, const PrescriptionId& prescriptionId, PrescriptionType prescriptionType,
                                 const Kvnr& kvnr, std::string_view hashedKvnr, TaskEvent::UseCase useCase, State state,
                                 const std::optional<TelematikId>& qesDocotorId, const TelematikId& jwtDoctorId,
                                 const std::string& jwtDoctorOrganizationName,
                                 const std::string& jwtDoctorProfessionOid, const TelematikId& jwtPharmacyId,
                                 const std::string& jwtPharmacyOrganizationName,
                                 const std::string& jwtPharmacyProfessionOid, Bundle&& kbvBundle,
                                 Bundle&& medicationDispenseBundle, const model::Timestamp& lastModified);
    virtual const Bundle& getMedicationDispenseBundle() const;

    virtual const TelematikId& getJwtPharmacyId() const;
    virtual const std::string& getJwtPharmacyOrganizationName() const;
    virtual const std::string& getJwtPharmacyProfessionOid() const;

private:
    TelematikId mJwtPharmacyId;
    std::string mJwtPharmacyOrganizationName;
    std::string mJwtPharmacyOrganizationOid;
    Bundle mMedicationDispenseBundle;
};

class CancelPrescriptionTaskEvent : public TaskEvent
{

public:
    CancelPrescriptionTaskEvent(id_t id, const PrescriptionId& prescriptionId, PrescriptionType prescriptionType,
                                const Kvnr& kvnr, std::string_view hashedKvnr, TaskEvent::UseCase useCase, State state,
                                Bundle&& kbvBundle, const model::Timestamp& lastModified);
};


class CancelDispensationTaskEvent : public TaskEvent
{

public:
    CancelDispensationTaskEvent(id_t id, const PrescriptionId& prescriptionId, PrescriptionType prescriptionType,
                                const Kvnr& kvnr, std::string_view hashedKvnr, TaskEvent::UseCase useCase, State state,
                                Bundle&& kbvBundle, const model::Timestamp& lastModified);
};


}// model

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_TASKEVENT_HXX
