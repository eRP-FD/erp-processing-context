/*
* (C) Copyright IBM Deutschland GmbH 2021, 2025
* (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
*/

#include "erp/database/ErpDatabaseModel.hxx"
#include "erp/database/PostgresBackendTask.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/util/DurationConsumer.hxx"

#include <pqxx/pqxx>

PostgresBackendTask::PostgresBackendTask(model::PrescriptionType prescriptionType)
    : mPrescriptionType(prescriptionType)
{
#define QUERY(name, query) mQueries.name = {#name, query};

    QUERY(retrieveTaskById, R"--(
        SELECT prescription_id, kvnr, EXTRACT(EPOCH FROM last_modified) as last_modified, EXTRACT(EPOCH FROM authored_on) as authored_on,
            EXTRACT(EPOCH FROM expiry_date) as expiry_date, EXTRACT(EPOCH FROM accept_date) as accept_date, status, EXTRACT(EPOCH FROM last_status_update) as last_status_update, salt, task_key_blob_id,
            access_code, secret, owner, EXTRACT(EPOCH from last_medication_dispense) as last_medication_dispense, eu_redeemable_patient, eu_redeemable, is_pkv
        FROM )--" + taskTableName() + R"--(
        WHERE prescription_id = $1
        )--")

    QUERY(retrieveTaskByIdPlusReceipt, R"--(
        SELECT prescription_id, kvnr, EXTRACT(EPOCH FROM last_modified) as last_modified, EXTRACT(EPOCH FROM authored_on) as authored_on,
            EXTRACT(EPOCH FROM expiry_date) as expiry_date, EXTRACT(EPOCH FROM accept_date) as accept_date, status, EXTRACT(EPOCH FROM last_status_update) as last_status_update, salt, task_key_blob_id,
            secret, owner, receipt, EXTRACT(EPOCH from last_medication_dispense) as last_medication_dispense, eu_redeemable_patient, eu_redeemable, is_pkv
        FROM )--" + taskTableName() + R"--(
        WHERE prescription_id = $1
        )--")

    QUERY(retrieveTaskByIdForUpdatePlusPrescription, R"--(
        SELECT prescription_id, kvnr, EXTRACT(EPOCH FROM last_modified) as last_modified, EXTRACT(EPOCH FROM authored_on) as authored_on,
            EXTRACT(EPOCH FROM expiry_date) as expiry_date, EXTRACT(EPOCH FROM accept_date) as accept_date, status, EXTRACT(EPOCH FROM last_status_update) as last_status_update, salt, task_key_blob_id,
            access_code, secret, owner, healthcare_provider_prescription, EXTRACT(EPOCH from last_medication_dispense) as last_medication_dispense, eu_redeemable_patient, eu_redeemable, is_pkv
        FROM )--" + taskTableName() + R"--(
        WHERE prescription_id = $1
        FOR UPDATE
        )--")

    QUERY(retrieveTaskByIdPlusPrescription, R"--(
        SELECT prescription_id, kvnr, EXTRACT(EPOCH FROM last_modified) as last_modified, EXTRACT(EPOCH FROM authored_on) as authored_on,
            EXTRACT(EPOCH FROM expiry_date) as expiry_date, EXTRACT(EPOCH FROM accept_date) as accept_date, status, EXTRACT(EPOCH FROM last_status_update) as last_status_update, salt, task_key_blob_id,
            access_code, owner, healthcare_provider_prescription, EXTRACT(EPOCH from last_medication_dispense) as last_medication_dispense, eu_redeemable_patient, eu_redeemable, is_pkv
        FROM )--" + taskTableName() + R"--(
        WHERE prescription_id = $1
        FOR UPDATE
        )--")


    QUERY(retrieveTaskWithSecretByIdPlusPrescription, R"--(
        SELECT prescription_id, kvnr, EXTRACT(EPOCH FROM last_modified) as last_modified, EXTRACT(EPOCH FROM authored_on) as authored_on,
            EXTRACT(EPOCH FROM expiry_date) as expiry_date, EXTRACT(EPOCH FROM accept_date) as accept_date, status, EXTRACT(EPOCH FROM last_status_update) as last_status_update, salt, task_key_blob_id,
            access_code, secret, owner, healthcare_provider_prescription, EXTRACT(EPOCH from last_medication_dispense) as last_medication_dispense, eu_redeemable_patient, eu_redeemable, is_pkv
        FROM )--" + taskTableName() + R"--(
        WHERE prescription_id = $1
    )--")

    // GEMREQ-start A_22135-01#query, A_22134#query
    QUERY(retrieveTaskByIdPlusPrescriptionPlusReceipt, R"--(
        SELECT prescription_id, kvnr, EXTRACT(EPOCH FROM last_modified) as last_modified, EXTRACT(EPOCH FROM authored_on) as authored_on,
            EXTRACT(EPOCH FROM expiry_date) as expiry_date, EXTRACT(EPOCH FROM accept_date) as accept_date, status, EXTRACT(EPOCH FROM last_status_update) as last_status_update, salt, task_key_blob_id,
            access_code, secret, owner, healthcare_provider_prescription, receipt, EXTRACT(EPOCH from last_medication_dispense) as last_medication_dispense, eu_redeemable_patient, eu_redeemable, is_pkv
        FROM )--" + taskTableName() + R"--(
        WHERE prescription_id = $1
        FOR UPDATE
        )--")
    // GEMREQ-end A_22135-01#query, A_22134#query

    // GEMREQ-start A_23452-02#query
    QUERY(retrieveAllTasksByKvnrWithAccessCode, R"--(
        SELECT prescription_id, kvnr, EXTRACT(EPOCH FROM last_modified) as last_modified, EXTRACT(EPOCH FROM authored_on) as authored_on,
            EXTRACT(EPOCH FROM expiry_date) as expiry_date, EXTRACT(EPOCH FROM accept_date) as accept_date, status, EXTRACT(EPOCH FROM last_status_update) as last_status_update, salt, task_key_blob_id,
            access_code, eu_redeemable_patient, eu_redeemable, is_pkv
        FROM )--" + taskTableName() + R"--(
        WHERE kvnr_hashed = $1
        )--")
    // GEMREQ-end A_23452-02#query

    QUERY(getTaskKeyData, R"--(
        SELECT task_key_blob_id, salt, EXTRACT(EPOCH FROM authored_on) as authored_on
        FROM )--" + taskTableName() + R"--( WHERE prescription_id = $1 FOR UPDATE)--")

    QUERY(createTask, R"--(
        INSERT INTO )--" + taskTableName() + R"--( (last_modified, authored_on, status, last_status_update) VALUES ($1, $2, $3, $4)
            RETURNING prescription_id, EXTRACT(EPOCH FROM authored_on) as authored_on)--")

    QUERY(updateTask, R"--(
        UPDATE )--" + taskTableName() + R"--( SET task_key_blob_id = $2, salt = $3, access_code = $4 WHERE prescription_id = $1
    )--")
    // GEMREQ-start A_24174#sql
    QUERY(updateTask_secret, R"--(
        UPDATE )--" + taskTableName() + R"--( SET status = $2, last_modified = $3, secret = $4, owner = $5, last_status_update = $6
        WHERE prescription_id = $1
        )--")
    // GEMREQ-end A_24174#sql
    QUERY(updateTask_activateTask, R"--(
        UPDATE )--" + taskTableName() + R"--(
        SET kvnr = $2, kvnr_hashed = $3, last_modified = $4, expiry_date = $5, accept_date = $6, status = $7,
            healthcare_provider_prescription = $8, doctor_identity = $9, last_status_update = $10, eu_redeemable = $11, is_pkv = $12
        WHERE prescription_id = $1
        )--")

    QUERY(updateTask_receipt, R"--(
        UPDATE )--" + taskTableName() + R"--( SET status = $2, last_modified = $3, receipt = $4, pharmacy_identity = $5, last_status_update = $6
        WHERE prescription_id = $1)--")

    QUERY(updateTask_medicationDispense, R"--(
        UPDATE )--" + taskTableName() + R"--(
        SET last_modified = $2, medication_dispense_bundle = $3, medication_dispense_blob_id =$4,
            when_handed_over = $5, when_prepared = $6, performer = $7, last_medication_dispense = $8,
            medication_dispense_salt = $9
        WHERE prescription_id = $1
        )--")

    QUERY(updateTask_medicationDispenseAndStatus, R"--(
        UPDATE )--" + taskTableName() + R"--(
        SET last_modified = $2, medication_dispense_bundle = $3, medication_dispense_blob_id =$4,
            when_handed_over = $5, when_prepared = $6, performer = $7, last_medication_dispense = $8,
            medication_dispense_salt = $9, status = $10
        WHERE prescription_id = $1
        )--")

    QUERY(updateTask_medicationDispenseReceipt, R"--(
        UPDATE )--" + taskTableName() + R"--(
        SET status = $2, last_modified = $3, medication_dispense_bundle = $4, medication_dispense_blob_id =$5,
            receipt = $6, when_handed_over = $7, when_prepared = $8, performer = $9, last_medication_dispense = $10,
            medication_dispense_salt = $11, pharmacy_identity = $12, last_status_update = $13
        WHERE prescription_id = $1
        )--")

// GEMREQ-start A_24286-02#sql-updateTaskDeleteMedicationDispense
    QUERY(updateTask_deleteMedicationDispense, R"--(
        UPDATE )--" + taskTableName() + R"--(
        SET last_modified = $2, when_handed_over = NULL, when_prepared = NULL, last_medication_dispense = NULL,
            performer = NULL, medication_dispense_blob_id = NULL, medication_dispense_bundle = NULL
        WHERE prescription_id = $1
        )--")
// GEMREQ-end A_24286-02#sql-updateTaskDeleteMedicationDispense

// GEMREQ-start A_19027-06#query-updateTaskClearPersonalData
    QUERY(updateTask_deletePersonalData, R"--(
        UPDATE )--" + taskTableName() + R"--(
        SET status = $2, last_modified = $3, kvnr = NULL, salt = NULL, access_code = NULL,
            secret = NULL, owner = NULL, healthcare_provider_prescription = NULL, receipt = NULL,
            when_handed_over = NULL, when_prepared = NULL, performer = NULL,
            medication_dispense_blob_id = NULL, medication_dispense_bundle = NULL, last_medication_dispense = NULL,
            doctor_identity = NULL, pharmacy_identity=NULL, last_status_update = $4
        WHERE prescription_id = $1
        )--")
// GEMREQ-end A_19027-06#query-updateTaskClearPersonalData

    QUERY(updateTaskMarkingFlag, R"--(
        UPDATE )--" + taskTableName() + R"--(
        SET eu_redeemable_patient = $2, last_modified = $3
        WHERE prescription_id = $1
        )--")
#undef QUERY
}


std::string PostgresBackendTask::taskTableName() const
{
    switch (mPrescriptionType)
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
            return TASK_160_TABLE_NAME;
        case model::PrescriptionType::digitaleGesundheitsanwendungen:
            return TASK_162_TABLE_NAME;
        case model::PrescriptionType::tRezept:
            return TASK_166_TABLE_NAME;
        case model::PrescriptionType::direkteZuweisung:
            return TASK_169_TABLE_NAME;
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
            return TASK_200_TABLE_NAME;
        case model::PrescriptionType::direkteZuweisungPkv:
            return TASK_209_TABLE_NAME;
    }
    Fail("invalid mPrescriptionType type : " + std::to_string(uintmax_t(mPrescriptionType)));
}


std::tuple<model::PrescriptionId, model::Timestamp>
PostgresBackendTask::createTask(pqxx::transaction_base& transaction, model::Task::Status taskStatus,
                                const model::Timestamp& lastUpdated, const model::Timestamp& created,
                                const model::Timestamp& lastStatusUpdate)
{
    TVLOG(2) << mQueries.createTask.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "createtask");

    const auto status = gsl::narrow<int>(model::Task::toNumericalStatus(taskStatus));
    auto result = transaction
                      .exec(mQueries.createTask.query, pqxx::params{lastUpdated.toXsDateTime(), created.toXsDateTime(),
                                                                    status, lastStatusUpdate.toXsDateTime()})
                      .one_row();
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.size() == 2, "Expected Prescription Id and authored on as returned values.");
    A_19019_01.start("Create prescription ID");
    auto prescriptionId = model::PrescriptionId::fromDatabaseId(mPrescriptionType, result[0].as<int64_t>());
    A_19019_01.finish();
    model::Timestamp authoredOn{result[1].as<double>()};
    return std::make_tuple(prescriptionId, authoredOn);
}


void PostgresBackendTask::updateTask(pqxx::transaction_base& transaction, const model::PrescriptionId& taskId,
                                     const db_model::EncryptedBlob& accessCode, uint32_t blobId,
                                     const db_model::Blob& salt) const
{
    TVLOG(2) << mQueries.updateTask.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "updatetask");

    auto result =
        transaction.exec(mQueries.updateTask.query,
                         pqxx::params{taskId.toDatabaseId(), blobId, salt.binarystring(), accessCode.binarystring()});
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.empty(), "No result rows expected");
}


std::tuple<BlobId, db_model::Blob, model::Timestamp>
PostgresBackendTask::getTaskKeyData(pqxx::transaction_base& transaction, const model::PrescriptionId& taskId) const
{
    TVLOG(2) << mQueries.getTaskKeyData.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "gettaskkeydata");

    auto result = transaction.exec(mQueries.getTaskKeyData.query, pqxx::params{taskId.toDatabaseId()});
    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() == 1, "Expected exactly one set of key data");
    const auto blobId = result.front().at(0).as<int64_t>();
    const db_model::Blob salt{result.front().at(1).as<db_model::postgres_bytea>()};

    const model::Timestamp authoredOn{result.front().at(2).as<double>()};
    return std::make_tuple(gsl::narrow_cast<BlobId>(blobId), salt, authoredOn);
}


void PostgresBackendTask::updateTaskStatusAndSecret(pqxx::transaction_base& transaction,
                                                    const model::PrescriptionId& taskId, model::Task::Status status,
                                                    const model::Timestamp& lastModifiedDate,
                                                    const std::optional<db_model::EncryptedBlob>& secret,
                                                    const std::optional<db_model::EncryptedBlob>& owner,
                                                    const model::Timestamp& lastStatusUpdate) const
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                        "updatetaskstatusandsecret");

    // GEMREQ-start A_24174#call-sql
    // It is possible to set or delete the secret using this query
    std::optional<db_model::postgres_bytea_view> secretBin;
    if (secret.has_value())
    {
        secretBin = secret->binarystring();
    }
    std::optional<db_model::postgres_bytea_view> ownerBin;
    if (owner.has_value())
    {
        ownerBin = owner->binarystring();
    }

    TVLOG(2) << mQueries.updateTask_secret.query;
    const pqxx::result result = transaction.exec(
        mQueries.updateTask_secret.query,
        pqxx::params{taskId.toDatabaseId(), static_cast<int>(model::Task::toNumericalStatus(status)),
                     lastModifiedDate.toXsDateTime(), secretBin, ownerBin, lastStatusUpdate.toXsDateTime()});
    // GEMREQ-end A_24174#call-sql
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.empty(), "Expected an empty result");
}


void PostgresBackendTask::activateTask(pqxx::transaction_base& transaction, const model::PrescriptionId& taskId,
                                       const db_model::EncryptedBlob& encryptedKvnr,
                                       const db_model::HashedKvnr& hashedKvnr, model::Task::Status taskStatus,
                                       const model::Timestamp& lastModified, const model::Timestamp& expiryDate,
                                       const model::Timestamp& acceptDate,
                                       const db_model::EncryptedBlob& healthCareProviderPrescription,
                                       const db_model::EncryptedBlob& doctorIdentity,
                                       const model::Timestamp& lastStatusUpdate,
                                       bool euRedeemable, bool isPkv) const
{
    TVLOG(2) << mQueries.updateTask_activateTask.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "activatetask");

    const auto status = model::Task::toNumericalStatus(taskStatus);

    const pqxx::result result = transaction.exec(
        mQueries.updateTask_activateTask.query, {taskId.toDatabaseId(), encryptedKvnr.binarystring(),
        hashedKvnr.binarystring(), lastModified.toXsDateTime(), expiryDate.toGermanDate(), acceptDate.toGermanDate(),
        static_cast<int>(status), healthCareProviderPrescription.binarystring(), doctorIdentity.binarystring(),
        lastStatusUpdate.toXsDateTime(), euRedeemable, isPkv}).no_rows();
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.empty(), "Expected an empty result");
}

void PostgresBackendTask::updateTaskReceipt(pqxx::transaction_base& transaction, const model::PrescriptionId& taskId,
                                            const model::Task::Status& taskStatus, const model::Timestamp& lastModified,
                                            const db_model::EncryptedBlob& receipt,
                                            const db_model::EncryptedBlob& pharmacyIdentity,
                                            const model::Timestamp& lastStatusUpdate) const
{
    Expect3(taskId.type() == mPrescriptionType, "Wrong prescription type for table", std::logic_error);
    TVLOG(2) << mQueries.updateTask_receipt.query;
    const auto timerKeepAlive =
    DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "updatetaskreceipt");
    const auto status = model::Task::toNumericalStatus(taskStatus);
    const pqxx::result result = transaction.exec(
        mQueries.updateTask_receipt.query,
        pqxx::params{taskId.toDatabaseId(), static_cast<int>(status), lastModified.toXsDateTime(),
                     receipt.binarystring(), pharmacyIdentity.binarystring(), lastStatusUpdate.toXsDateTime()});
    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.empty(), "Expected an empty result");
}

void PostgresBackendTask::updateTaskMedicationDispense(
    pqxx::transaction_base& transaction, const model::PrescriptionId& taskId, const model::Timestamp& lastModified,
    const model::Timestamp& lastMedicationDispense, const db_model::EncryptedBlob& medicationDispense,
    BlobId medicationDispenseBlobId, const db_model::HashedTelematikId& telematikId,
    const model::Timestamp& whenHandedOver, const std::optional<model::Timestamp>& whenPrepared,
    const db_model::Blob& medicationDispenseSalt, const std::optional<model::Task::Status>& taskStatus /* = std::nullopt */) const
{
    TVLOG(2) << mQueries.updateTask_medicationDispense.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(
        DurationCategory::postgres, "updatetaskmedicationdispense");

    if (taskStatus.has_value()) {
        const auto status = model::Task::toNumericalStatus(*taskStatus);
        const pqxx::result result = transaction.exec(
            mQueries.updateTask_medicationDispenseAndStatus.query,
            pqxx::params{taskId.toDatabaseId(), lastModified.toXsDateTime(), medicationDispense.binarystring(),
                         gsl::narrow<int32_t>(medicationDispenseBlobId), whenHandedOver.toXsDateTime(),
                         whenPrepared ? std::make_optional(whenPrepared->toXsDateTime()) : std::nullopt,
                         telematikId.binarystring(), lastMedicationDispense.toXsDateTime(),
                         medicationDispenseSalt.binarystring(), static_cast<int>(status)});
        TVLOG(2) << "got " << result.size() << " results";

        Expect(result.empty(), "Expected an empty result");
    }
    else
    {
        const pqxx::result result = transaction.exec(
            mQueries.updateTask_medicationDispense.query,
            pqxx::params{taskId.toDatabaseId(), lastModified.toXsDateTime(), medicationDispense.binarystring(),
                         gsl::narrow<int32_t>(medicationDispenseBlobId), whenHandedOver.toXsDateTime(),
                         whenPrepared ? std::make_optional(whenPrepared->toXsDateTime()) : std::nullopt,
                         telematikId.binarystring(), lastMedicationDispense.toXsDateTime(),
                         medicationDispenseSalt.binarystring()});
        TVLOG(2) << "got " << result.size() << " results";

        Expect(result.empty(), "Expected an empty result");
    }

}

void PostgresBackendTask::updateTaskMedicationDispenseReceipt(
    pqxx::transaction_base& transaction, const model::PrescriptionId& taskId, const model::Task::Status& taskStatus,
    const model::Timestamp& lastModified, const db_model::EncryptedBlob& medicationDispense,
    BlobId medicationDispenseBlobId, const db_model::HashedTelematikId& telematikId,
    const model::Timestamp& whenHandedOver, const std::optional<model::Timestamp>& whenPrepared,
    const db_model::EncryptedBlob& receipt, const model::Timestamp& lastMedicationDispense,
    const db_model::Blob& medicationDispenseSalt, const db_model::EncryptedBlob& pharmacyIdentity,
    const model::Timestamp& lastStatusUpdate) const
{
    TVLOG(2) << mQueries.updateTask_medicationDispenseReceipt.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(
        DurationCategory::postgres, "updatetaskmedicationdispensereceipt");

    const auto status = model::Task::toNumericalStatus(taskStatus);
    const pqxx::result result =
        transaction.exec(mQueries.updateTask_medicationDispenseReceipt.query,
                         pqxx::params{taskId.toDatabaseId(), static_cast<int>(status), lastModified.toXsDateTime(),
                                      medicationDispense.binarystring(), gsl::narrow<int32_t>(medicationDispenseBlobId),
                                      receipt.binarystring(), whenHandedOver.toXsDateTime(),
                                      whenPrepared ? std::make_optional(whenPrepared->toXsDateTime()) : std::nullopt,
                                      telematikId.binarystring(), lastMedicationDispense.toXsDateTime(),
                                      medicationDispenseSalt.binarystring(), pharmacyIdentity.binarystring(),
                                      lastStatusUpdate.toXsDateTime()});
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.empty(), "Expected an empty result");
}


// GEMREQ-start A_24286-02#query-call-updateTaskDeleteMedicationDispense
void PostgresBackendTask::updateTaskDeleteMedicationDispense(pqxx::transaction_base& transaction,
                                                             const model::PrescriptionId& taskId,
                                                             const model::Timestamp& lastModified) const
{
    TVLOG(2) << mQueries.updateTask_deleteMedicationDispense.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                        "updatetaskdeletemedicationdispense");

    const pqxx::result result = transaction.exec(mQueries.updateTask_deleteMedicationDispense.query,
                                                 pqxx::params{taskId.toDatabaseId(), lastModified.toXsDateTime()});
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.empty(), "Expected an empty result");
}
// GEMREQ-end A_24286-02#query-call-updateTaskDeleteMedicationDispense

// GEMREQ-start A_19027-06#query-call-updateTaskClearPersonalData
void PostgresBackendTask::updateTaskClearPersonalData(pqxx::transaction_base& transaction,
                                                      const model::PrescriptionId& taskId,
                                                      model::Task::Status taskStatus,
                                                      const model::Timestamp& lastModified,
                                                      const model::Timestamp& lastStatusUpdate) const
{
    TVLOG(2) << mQueries.updateTask_deletePersonalData.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                        "updatetaskclearpersonaldata");

    const auto status = model::Task::toNumericalStatus(taskStatus);

    const pqxx::result result =
        transaction.exec(mQueries.updateTask_deletePersonalData.query,
                         pqxx::params{taskId.toDatabaseId(), static_cast<int>(status), lastModified.toXsDateTime(),
                                      lastStatusUpdate.toXsDateTime()});
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.empty(), "Expected an empty result");
}
// GEMREQ-end A_19027-06#query-call-updateTaskClearPersonalData


void PostgresBackendTask::updateTaskEuRedeemableByPatient(pqxx::transaction_base& transaction, const model::PrescriptionId& taskId,
                                                          bool redeemable,
                                                          const model::Timestamp& lastModified) const
{
    TVLOG(2) << mQueries.updateTaskMarkingFlag.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                        "updatetaskeuredeemablebypatient");

    const pqxx::result result =
        transaction.exec(mQueries.updateTaskMarkingFlag.query,
                         pqxx::params{taskId.toDatabaseId(), redeemable, lastModified.toXsDateTime()});

    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.empty(), "Expected an empty result");
}


std::optional<db_model::Task> PostgresBackendTask::retrieveTaskForUpdate(pqxx::transaction_base& transaction,
                                                                         const model::PrescriptionId& taskId)
{
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationCategory::postgres, "retrievetaskforupdate");

    std::string query(mQueries.retrieveTaskById.query);
    query.append("    FOR UPDATE");
    TVLOG(2) << query;
    const pqxx::result result = transaction.exec(query, pqxx::params{taskId.toDatabaseId()});

    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() <= 1, "Too many results in result set.");
    if (! result.empty())
    {
        return taskFromQueryResultRow(result.front(), mPrescriptionType);
    }
    return {};
}


::std::optional<::db_model::Task>
PostgresBackendTask::retrieveTaskForUpdateAndPrescription(::pqxx::transaction_base& transaction,
                                                          const ::model::PrescriptionId& taskId)
{
    TVLOG(2) << mQueries.retrieveTaskByIdForUpdatePlusPrescription.query;

    const auto timerKeepAlive = ::DurationConsumer::getCurrent().getTimer(
        DurationCategory::postgres, "retrievetaskforupdateandprescription");

    const auto result = transaction.exec(mQueries.retrieveTaskByIdForUpdatePlusPrescription.query,
                                                pqxx::params{taskId.toDatabaseId()});

    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() <= 1, "Too many results in result set.");
    if (! result.empty())
    {
        return taskFromQueryResultRow(result.front(), mPrescriptionType);
    }

    return {};
}


std::optional<db_model::Task> PostgresBackendTask::retrieveTaskAndReceipt(pqxx::transaction_base& transaction,
                                                                          const model::PrescriptionId& taskId)
{
    TVLOG(2) << mQueries.retrieveTaskByIdPlusReceipt.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                        "retrievetaskandreceipt");

    const pqxx::result result =
        transaction.exec(mQueries.retrieveTaskByIdPlusReceipt.query, pqxx::params{taskId.toDatabaseId()});

    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() <= 1, "Too many results in result set.");
    if (! result.empty())
    {
        Expect(result.front().size() == 17,
               "Invalid number of fields in result row: " + std::to_string(result.front().size()));
        return taskFromQueryResultRow(result.front(), mPrescriptionType);
    }
    return {};
}


std::optional<db_model::Task> PostgresBackendTask::retrieveTaskAndPrescription(pqxx::transaction_base& transaction,
                                                                               const model::PrescriptionId& taskId)
{
    TVLOG(2) << mQueries.retrieveTaskByIdPlusPrescription.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                        "retrievetaskandprescription");

    const pqxx::result result =
        transaction.exec(mQueries.retrieveTaskByIdPlusPrescription.query, pqxx::params{taskId.toDatabaseId()});

    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() <= 1, "Too many results in result set.");
    if (! result.empty())
    {
        Expect(result.front().size() == 17,
               "Invalid number of fields in result row: " + std::to_string(result.front().size()));
        return taskFromQueryResultRow(result.front(), mPrescriptionType);
    }
    return {};
}

std::optional<db_model::Task>
PostgresBackendTask::retrieveTaskWithSecretAndPrescription(pqxx::transaction_base& transaction,
                                                           const model::PrescriptionId& taskId)
{
    TVLOG(2) << mQueries.retrieveTaskWithSecretByIdPlusPrescription.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationCategory::postgres,
                                                                        "retrievetaskwithsecretandprescription");

    const pqxx::result result = transaction.exec(mQueries.retrieveTaskWithSecretByIdPlusPrescription.query,
                                                 pqxx::params{taskId.toDatabaseId()});

    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() <= 1, "Too many results in result set.");
    if (! result.empty())
    {
        Expect(result.front().size() == 18,
               "Invalid number of fields in result row: " + std::to_string(result.front().size()));
        return taskFromQueryResultRow(result.front(), mPrescriptionType);
    }
    return {};
}

// GEMREQ-start A_22135-01#retrieveTask, A_22134#retrieveTask
::std::optional<::db_model::Task>
PostgresBackendTask::retrieveTaskAndPrescriptionAndReceipt(::pqxx::transaction_base& transaction,
                                                           const ::model::PrescriptionId& taskId)
{
    TVLOG(2) << mQueries.retrieveTaskByIdPlusPrescriptionPlusReceipt.query;

    const auto timerKeepAlive = ::DurationConsumer::getCurrent().getTimer(
        DurationCategory::postgres, "retrievetaskbyidplusprescriptionplusreceipt");

    const auto result = transaction.exec(mQueries.retrieveTaskByIdPlusPrescriptionPlusReceipt.query,
                                         pqxx::params{taskId.toDatabaseId()});

    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() <= 1, "Too many results in result set.");
    if (! result.empty())
    {
        Expect(result.front().size() == 19,
               "Invalid number of fields in result row: " + std::to_string(result.front().size()));
        return taskFromQueryResultRow(result.front(), mPrescriptionType);
    }

    return {};
}
// GEMREQ-end A_22135-01#retrieveTask, A_22134#retrieveTask

std::vector<db_model::Task>
PostgresBackendTask::retrieveAllTasksWithAccessCode(::pqxx::transaction_base& transaction,
                                                    const db_model::HashedKvnr& kvnrHashed,
                                                    const std::optional<UrlArguments>& search)
{
    auto query = mQueries.retrieveAllTasksByKvnrWithAccessCode.query;
    if (search.has_value())
    {
        query.append(search->getSqlExpression(transaction.conn(), "                "));
    }
    TVLOG(2) << query;

    const auto timerKeepAlive = ::DurationConsumer::getCurrent().getTimer(
        DurationCategory::postgres, "retrievealltasksbykvnrwithaccesscode");

    const auto results = transaction.exec(query, pqxx::params{kvnrHashed.binarystring()});

    TVLOG(2) << "got " << results.size() << " results";
    if (!results.empty())
    {
        Expect(results.front().size() == 14,
               "Invalid number of fields in result row: " + std::to_string(results.front().size()));
    }
    return tasksFromQueryResult(results, mPrescriptionType);
}

struct TaskQueryIndexes {
    std::optional<pqxx::row::size_type> prescriptionIdIndex;
    std::optional<pqxx::row::size_type> prescriptionTypeIndex;
    std::optional<pqxx::row::size_type> kvnrIndex;
    std::optional<pqxx::row::size_type> lastModifiedIndex;
    std::optional<pqxx::row::size_type> authoredOnIndex;
    std::optional<pqxx::row::size_type> expiryDateIndex;
    std::optional<pqxx::row::size_type> acceptDateIndex;
    std::optional<pqxx::row::size_type> statusIndex;
    std::optional<pqxx::row::size_type> lastStatusChangeIndex;
    std::optional<pqxx::row::size_type> saltIndex;
    std::optional<pqxx::row::size_type> keyBlobIdIndex;
    std::optional<pqxx::row::size_type> accessCodeIndex;
    std::optional<pqxx::row::size_type> secretIndex;
    std::optional<pqxx::row::size_type> ownerIndex;
    std::optional<pqxx::row::size_type> healthcareProviderPrescriptionIndex;
    std::optional<pqxx::row::size_type> receiptIndex;
    std::optional<pqxx::row::size_type> lastMedicationDispenseIndex;
    std::optional<pqxx::row::size_type> euRedeemableByPatientIndex;
    std::optional<pqxx::row::size_type> euRedeemableIndex;
    std::optional<pqxx::row::size_type> isPkvIndex;
};
namespace
{
bool checkIndexAndRow(const std::optional<pqxx::row::size_type>& row, const pqxx::row& resultRow)
{
    return row.has_value() && ! resultRow.at(*row).is_null();
}
db_model::Blob getSalt(const pqxx::row& resultRow, model::Task::Status status,
                              const TaskQueryIndexes& indexes)
{
    if (!checkIndexAndRow(indexes.saltIndex, resultRow))
    {
        Expect3(status == model::Task::Status::cancelled, "Missing salt data in task.", std::logic_error);
    }
    else
    {
        return db_model::Blob{resultRow.at(*indexes.saltIndex).as<db_model::postgres_bytea>()};
    }
    return {};
}

TaskQueryIndexes buildIndexes(const pqxx::row& resultRow)
{
    TaskQueryIndexes result{};
    size_t idx = 0;
    for (const auto& field : resultRow)
    {
        auto enumValue = magic_enum::enum_cast<PostgresBackendTask::ColumnName>(field.name());
        Expect(enumValue.has_value(), "missing enum value for column " + std::string{field.name()});
        switch (*enumValue)
        {
            using enum PostgresBackendTask::ColumnName;
        case prescription_id:
            result.prescriptionIdIndex = idx;
            break;
        case prescription_type:
            result.prescriptionTypeIndex = idx;
            break;
        case kvnr:
            result.kvnrIndex = idx;
            break;
        case last_modified:
            result.lastModifiedIndex = idx;
            break;
        case authored_on:
            result.authoredOnIndex = idx;
            break;
        case expiry_date:
            result.expiryDateIndex = idx;
            break;
        case accept_date:
            result.acceptDateIndex = idx;
            break;
        case status:
            result.statusIndex = idx;
            break;
        case task_key_blob_id:
            result.keyBlobIdIndex = idx;
            break;
        case salt:
            result.saltIndex = idx;
            break;
        case access_code:
            result.accessCodeIndex = idx;
            break;
        case secret:
            result.secretIndex = idx;
            break;
        case healthcare_provider_prescription:
            result.healthcareProviderPrescriptionIndex = idx;
            break;
        case receipt:
            result.receiptIndex = idx;
            break;
        case medication_dispense_bundle:
            result.lastMedicationDispenseIndex = idx;
            break;
        case owner:
            result.ownerIndex = idx;
            break;
        case last_medication_dispense:
            result.lastMedicationDispenseIndex = idx;
            break;
        case last_status_update:
            result.lastStatusChangeIndex = idx;
            break;
        case eu_redeemable:
            result.euRedeemableIndex = idx;
            break;
        case eu_redeemable_patient:
            result.euRedeemableByPatientIndex = idx;
            break;
        case is_pkv:
            result.isPkvIndex = idx;
            break;
        case kvnr_hashed:
        case when_handed_over:
        case when_prepared:
        case performer:
        case medication_dispense_blob_id:
        case medication_dispense_salt:
        case doctor_identity:
        case pharmacy_identity:
            // no index
            break;
        }
        ++idx;
    }
    Expect(result.prescriptionIdIndex.has_value(), "prescription_id missing in task table");
    Expect(result.keyBlobIdIndex.has_value(), "task_key_blob_id missing in task table");
    Expect(result.statusIndex.has_value(), "status missing in task table");
    Expect(result.authoredOnIndex.has_value(), "authored_on missing in task table");
    Expect(result.lastModifiedIndex.has_value(), "last_modified missing in task table");
    Expect(result.euRedeemableByPatientIndex.has_value(), "eu_redeemable_patient missing in task table");
    Expect(result.euRedeemableIndex.has_value(), "eu_redeemable missing in task table");
    return result;
}
std::optional<bool> isPkv(const pqxx::row& resultRow, const TaskQueryIndexes& indexes)
{
    if (checkIndexAndRow(indexes.isPkvIndex, resultRow))
    {
        return resultRow.at(*indexes.isPkvIndex).as<bool>();
    }
    // for tasks of type 166 before $activate, it is unknown if the patient is gkv or pkv.
    return std::nullopt;
}

}

std::vector<db_model::Task> PostgresBackendTask::tasksFromQueryResult(const pqxx::result& results,
                                                                      std::optional<model::PrescriptionType> prescriptionType)
{
    std::vector<db_model::Task> resultSet;
    resultSet.reserve(gsl::narrow<size_t>(results.size()));
    if (!results.empty())
    {
        auto indexes = buildIndexes(results.front());
        for (const auto & result : results)
        {
            resultSet.emplace_back(PostgresBackendTask::taskFromQueryResultRow(result, prescriptionType, indexes));
        }
    }
    return resultSet;
}

db_model::Task PostgresBackendTask::taskFromQueryResultRow(const pqxx::row& resultRow,
                                                           std::optional<model::PrescriptionType> prescriptionType)
{
    return taskFromQueryResultRow(resultRow, prescriptionType, buildIndexes(resultRow));
}

db_model::Task PostgresBackendTask::taskFromQueryResultRow(const pqxx::row& resultRow,
                                                           std::optional<model::PrescriptionType> prescriptionType,
                                                           const TaskQueryIndexes& indexes)
{
    Expect(resultRow.size() >= 12, "Too few fields in result row");

    if (checkIndexAndRow(indexes.prescriptionTypeIndex, resultRow))
    {
        prescriptionType = magic_enum::enum_cast<model::PrescriptionType>(
            gsl::narrow<uint8_t>(resultRow.at(*indexes.prescriptionTypeIndex).as<uint16_t>()));
    }
    Expect(prescriptionType.has_value(), "prescription_type missing in query result");

    const auto& prescriptionId =
        model::PrescriptionId::fromDatabaseId(*prescriptionType,
                                              resultRow.at(*indexes.prescriptionIdIndex).as<int64_t>());
    const auto keyBlobId = gsl::narrow<BlobId>(resultRow.at(*indexes.keyBlobIdIndex).as<int32_t>());
    const auto status =
        model::Task::fromNumericalStatus(gsl::narrow<int8_t>(resultRow.at(*indexes.statusIndex).as<int>()));

    db_model::Blob salt = getSalt(resultRow, status, indexes);

    const model::Timestamp authoredOn{resultRow.at(*indexes.authoredOnIndex).as<double>()};
    const model::Timestamp lastModified{resultRow.at(*indexes.lastModifiedIndex).as<double>()};
    auto lastStatusChange = lastModified;
    if (checkIndexAndRow(indexes.lastStatusChangeIndex, resultRow))
    {
        lastStatusChange = model::Timestamp{resultRow.at(*indexes.lastStatusChangeIndex).as<double>()};
    }
    db_model::Task dbTask{prescriptionId,   keyBlobId,  salt,         status,
                          lastStatusChange, authoredOn, lastModified, isPkv(resultRow, indexes)};

    if (checkIndexAndRow(indexes.accessCodeIndex, resultRow))
    {
        dbTask.accessCode.emplace(resultRow.at(*indexes.accessCodeIndex).as<db_model::postgres_bytea>());
    }

    if (checkIndexAndRow(indexes.kvnrIndex, resultRow))
    {
        dbTask.kvnr.emplace(resultRow.at(*indexes.kvnrIndex).as<db_model::postgres_bytea>());
    }

    if (checkIndexAndRow(indexes.expiryDateIndex, resultRow))
    {
        dbTask.expiryDate.emplace(resultRow.at(*indexes.expiryDateIndex).as<double>());
    }

    if (checkIndexAndRow(indexes.acceptDateIndex, resultRow))
    {
        dbTask.acceptDate.emplace(resultRow.at(*indexes.acceptDateIndex).as<double>());
    }

    if (checkIndexAndRow(indexes.secretIndex, resultRow))
    {
        dbTask.secret.emplace(resultRow.at(*indexes.secretIndex).as<db_model::postgres_bytea>());
    }

    if (checkIndexAndRow(indexes.ownerIndex, resultRow))
    {
        dbTask.owner.emplace(resultRow.at(*indexes.ownerIndex).as<db_model::postgres_bytea>());
    }

    if (checkIndexAndRow(indexes.receiptIndex, resultRow))
    {
        dbTask.receipt.emplace(resultRow.at(*indexes.receiptIndex).as<db_model::postgres_bytea>());
    }

    if (checkIndexAndRow(indexes.healthcareProviderPrescriptionIndex, resultRow))
    {
        dbTask.healthcareProviderPrescription.emplace(
            resultRow.at(*indexes.healthcareProviderPrescriptionIndex).as<db_model::postgres_bytea>());
    }

    if (checkIndexAndRow(indexes.lastMedicationDispenseIndex, resultRow))
    {
        dbTask.lastMedicationDispense.emplace(resultRow.at(*indexes.lastMedicationDispenseIndex).as<double>());
    }

    Expect(checkIndexAndRow(indexes.euRedeemableByPatientIndex, resultRow), "eu_redeemable_patient missing in task table");
    dbTask.euRedeemableByPatient = resultRow.at(*indexes.euRedeemableByPatientIndex).as<bool>();

    Expect(checkIndexAndRow(indexes.euRedeemableIndex, resultRow), "eu_redeemable missing in task table");
    dbTask.euRedeemable = resultRow.at(*indexes.euRedeemableIndex).as<bool>();

    return dbTask;
}
