/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_EXPORTER_TASKEVENTCONVERTER_HXX
#define ERP_EXPORTER_TASKEVENTCONVERTER_HXX

#include "exporter/database/ExporterDatabaseModel.hxx"
#include "exporter/model/TaskEvent.hxx"

#include <memory>

class TaskEventConverter
{
public:
    TaskEventConverter(const DataBaseCodec& codec);

    std::unique_ptr<model::TaskEvent> convert(const db_model::TaskEvent& dbTaskEvent, const SafeString& key,
                                              const SafeString& medicationDispenseKey) const;

private:
    std::unique_ptr<model::TaskEvent> convertProvidePrescriptionTaskEvent(
        const db_model::TaskEvent& dbTaskEvent, const SafeString& key, model::PrescriptionType prescriptionType,
        const model::Kvnr& kvnr, const std::string& hashedKvnr, model::TaskEvent::UseCase usecase,
        model::TaskEvent::State state, const std::optional<model::TelematikId>& qesDoctorId, model::Bundle&& kbvBundle) const;

    std::unique_ptr<model::TaskEvent>
    convertCancelPrescriptionTaskEvent(const db_model::TaskEvent& dbTaskEvent, model::PrescriptionType prescriptionType,
                                       const model::Kvnr& kvnr, const std::string& hashedKvnr,
                                       model::TaskEvent::UseCase usecase, model::TaskEvent::State state,
                                       model::Bundle&& kbvBundle) const;

    std::unique_ptr<model::TaskEvent> convertProvideDispensationTaskEvent(
        const db_model::TaskEvent& dbTaskEvent, const SafeString& key, const SafeString& medicationDispenseKey,
        model::PrescriptionType prescriptionType, const model::Kvnr& kvnr, const std::string& hashedKvnr,
        model::TaskEvent::UseCase usecase, model::TaskEvent::State state, const std::optional<model::TelematikId>& qesDoctorId,
        model::Bundle&& kbvBundle) const;

    std::unique_ptr<model::TaskEvent>
    convertCancelDispensationTaskEvent(const db_model::TaskEvent& dbTaskEvent, model::PrescriptionType prescriptionType,
                                       const model::Kvnr& kvnr, const std::string& hashedKvnr,
                                       model::TaskEvent::UseCase usecase, model::TaskEvent::State state,
                                       model::Bundle&& kbvBundle) const;

    const DataBaseCodec& mCodec;
};

#endif// ERP_EXPORTER_DATABASEMODEL_HXX
