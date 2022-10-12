/*
* (C) Copyright IBM Deutschland GmbH 2021
* (C) Copyright IBM Corp. 2021
*/

#include "erp/database/PostgresBackendTask.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/database/DatabaseModel.hxx"
#include "erp/database/PostgresBackendHelper.hxx"
#include "erp/util/DurationConsumer.hxx"
#include "erp/util/search/UrlArguments.hxx"

#include <pqxx/pqxx>

PostgresBackendTask::PostgresBackendTask(model::PrescriptionType prescriptionType)
    : mPrescriptionType(prescriptionType)
{
#define QUERY(name, query) mQueries.name = {#name, query};

    QUERY(retrieveTaskById, R"--(
        SELECT prescription_id, kvnr, EXTRACT(EPOCH FROM last_modified), EXTRACT(EPOCH FROM authored_on),
            EXTRACT(EPOCH FROM expiry_date), EXTRACT(EPOCH FROM accept_date), status, salt, task_key_blob_id,
            access_code, secret
        FROM )--" + taskTableName() + R"--(
        WHERE prescription_id = $1
        )--")

    QUERY(retrieveTaskByIdPlusReceipt, R"--(
        SELECT prescription_id, kvnr, EXTRACT(EPOCH FROM last_modified), EXTRACT(EPOCH FROM authored_on),
            EXTRACT(EPOCH FROM expiry_date), EXTRACT(EPOCH FROM accept_date), status, salt, task_key_blob_id,
            secret, receipt
        FROM )--" + taskTableName() + R"--(
        WHERE prescription_id = $1
        )--")

    QUERY(retrieveTaskByIdForUpdatePlusPrescription, R"--(
        SELECT prescription_id, kvnr, EXTRACT(EPOCH FROM last_modified), EXTRACT(EPOCH FROM authored_on),
            EXTRACT(EPOCH FROM expiry_date), EXTRACT(EPOCH FROM accept_date), status, salt, task_key_blob_id,
            access_code, secret, healthcare_provider_prescription
        FROM )--" + taskTableName() + R"--(
        WHERE prescription_id = $1
        FOR UPDATE
        )--")

    QUERY(retrieveTaskByIdPlusPrescription, R"--(
        SELECT prescription_id, kvnr, EXTRACT(EPOCH FROM last_modified), EXTRACT(EPOCH FROM authored_on),
            EXTRACT(EPOCH FROM expiry_date), EXTRACT(EPOCH FROM accept_date), status, salt, task_key_blob_id,
            access_code, healthcare_provider_prescription
        FROM )--" + taskTableName() + R"--(
        WHERE prescription_id = $1
        FOR UPDATE
        )--")

    QUERY(retrieveTaskByIdPlusPrescriptionPlusReceipt, R"--(
        SELECT prescription_id, kvnr, EXTRACT(EPOCH FROM last_modified), EXTRACT(EPOCH FROM authored_on),
            EXTRACT(EPOCH FROM expiry_date), EXTRACT(EPOCH FROM accept_date), status, salt, task_key_blob_id,
            access_code, secret, healthcare_provider_prescription, receipt
        FROM )--" + taskTableName() + R"--(
        WHERE prescription_id = $1
        FOR UPDATE
        )--")

    QUERY(countAllTasksByKvnr, R"--(
        SELECT COUNT(*)
        FROM )--" + taskTableName() + R"--(
        WHERE kvnr_hashed = $1 AND status != 4
        )--")

    QUERY(getTaskKeyData, R"--(
        SELECT task_key_blob_id, salt, EXTRACT(EPOCH FROM authored_on)
        FROM )--" + taskTableName() + R"--( WHERE prescription_id = $1 FOR UPDATE)--")

    QUERY(createTask, R"--(
        INSERT INTO )--" + taskTableName() + R"--( (last_modified, authored_on, status) VALUES ($1, $2, $3)
            RETURNING prescription_id, EXTRACT(EPOCH FROM authored_on))--")

    QUERY(updateTask, R"--(
        UPDATE )--" + taskTableName() + R"--( SET task_key_blob_id = $2, salt = $3, access_code = $4 WHERE prescription_id = $1
    )--")

    QUERY(updateTask_secret, R"--(
        UPDATE )--" + taskTableName() + R"--( SET status = $2, last_modified = $3, secret = $4
        WHERE prescription_id = $1
        )--")

    QUERY(updateTask_activateTask, R"--(
        UPDATE )--" + taskTableName() + R"--(
        SET kvnr = $2, kvnr_hashed = $3, last_modified = $4, expiry_date = $5, accept_date = $6, status = $7,
            healthcare_provider_prescription = $8
        WHERE prescription_id = $1
        )--")

    QUERY(updateTask_medicationDispenseReceipt, R"--(
        UPDATE )--" + taskTableName() + R"--(
        SET status = $2, last_modified = $3, medication_dispense_bundle = $4, medication_dispense_blob_id =$5,
            receipt = $6, when_handed_over = $7, when_prepared = $8, performer = $9
        WHERE prescription_id = $1
        )--")

    QUERY(updateTask_deletePersonalData, R"--(
        UPDATE )--" + taskTableName() + R"--(
        SET status = $2, last_modified = $3, kvnr = NULL, kvnr_hashed = NULL, salt = NULL, access_code = NULL,
            secret = NULL, healthcare_provider_prescription = NULL, receipt = NULL,
            when_handed_over = NULL, when_prepared = NULL, performer = NULL, medication_dispense_bundle = NULL
        WHERE prescription_id = $1
        )--")

    QUERY(deleteChargeItemSupportingInformation, R"--(
        UPDATE )--" + taskTableName() + R"--(
        SET healthcare_provider_prescription = NULL, medication_dispense_bundle = NULL, medication_dispense_blob_id = NULL, receipt = NULL
        WHERE prescription_id = $1
        )--")

    QUERY(clearAllChargeItemSupportingInformation, R"--(
        UPDATE )--" + taskTableName() + R"--(
        SET healthcare_provider_prescription = NULL, medication_dispense_bundle = NULL, medication_dispense_blob_id = NULL, receipt = NULL
        WHERE kvnr_hashed = $1
    )--")
#undef QUERY
}


std::string PostgresBackendTask::taskTableName() const
{
    switch (mPrescriptionType)
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
            return TASK_160_TABLE_NAME;
        case model::PrescriptionType::direkteZuweisung:
            return TASK_169_TABLE_NAME;
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
            return TASK_200_TABLE_NAME;
        case model::PrescriptionType::direkteZuweisungPkv:
            return TASK_209_TABLE_NAME;
    }
    Fail("invalid mPrescriptionType type : " + std::to_string(uintmax_t(mPrescriptionType)));
}


std::tuple<model::PrescriptionId, model::Timestamp> PostgresBackendTask::createTask(pqxx::work& transaction,
                                                                                    model::Task::Status taskStatus,
                                                                                    const model::Timestamp& lastUpdated,
                                                                                    const model::Timestamp& created)
{
    TVLOG(2) << mQueries.createTask.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres, "PostgreSQL:createTask");

    const auto status = gsl::narrow<int>(model::Task::toNumericalStatus(taskStatus));
    auto result =
        transaction.exec_params1(mQueries.createTask.query, lastUpdated.toXsDateTime(), created.toXsDateTime(), status);
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.size() == 2, "Expected Prescription Id and athored on as returned values.");
    auto prescriptionId = model::PrescriptionId::fromDatabaseId(mPrescriptionType, result[0].as<int64_t>());
    model::Timestamp authoredOn{result[1].as<double>()};
    return std::make_tuple(prescriptionId, authoredOn);
}


void PostgresBackendTask::updateTask(pqxx::work& transaction, const model::PrescriptionId& taskId,
                                     const db_model::EncryptedBlob& accessCode, uint32_t blobId,
                                     const db_model::Blob& salt) const
{
    TVLOG(2) << mQueries.updateTask.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres, "PostgreSQL:updateTask");

    auto result = transaction.exec_params(mQueries.updateTask.query, taskId.toDatabaseId(), blobId, salt.binarystring(),
                                          accessCode.binarystring());
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.empty(), "No result rows expected");
}


std::tuple<BlobId, db_model::Blob, model::Timestamp>
PostgresBackendTask::getTaskKeyData(pqxx::work& transaction, const model::PrescriptionId& taskId) const
{
    TVLOG(2) << mQueries.getTaskKeyData.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres, "PostgreSQL:getTaskKeyData");

    auto result = transaction.exec_params(mQueries.getTaskKeyData.query, taskId.toDatabaseId());
    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() == 1, "Expected exactly one set of key data");
    const auto blobId = result.front().at(0).as<int64_t>();
    const db_model::Blob salt{result.front().at(1).as<db_model::postgres_bytea>()};

    const model::Timestamp authoredOn{result.front().at(2).as<double>()};
    return std::make_tuple(gsl::narrow_cast<BlobId>(blobId), salt, authoredOn);
}


void PostgresBackendTask::updateTaskStatusAndSecret(pqxx::work& transaction, const model::PrescriptionId& taskId,
                                                    model::Task::Status status,
                                                    const model::Timestamp& lastModifiedDate,
                                                    const std::optional<db_model::EncryptedBlob>& secret) const
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres,
                                                                        "PostgreSQL:updateTaskStatusAndSecret");

    // It is possible to set or delete the secret using this query
    std::optional<db_model::postgres_bytea_view> secretBin;
    if (secret.has_value())
    {
        secretBin = secret->binarystring();
    }

    TVLOG(2) << mQueries.updateTask_secret.query;
    const pqxx::result result = transaction.exec_params(mQueries.updateTask_secret.query, taskId.toDatabaseId(),
                                                        static_cast<int>(model::Task::toNumericalStatus(status)),
                                                        lastModifiedDate.toXsDateTime(), secretBin);
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.empty(), "Expected an empty result");
}


void PostgresBackendTask::activateTask(pqxx::work& transaction, const model::PrescriptionId& taskId,
                                       const db_model::EncryptedBlob& encryptedKvnr,
                                       const db_model::HashedKvnr& hashedKvnr, model::Task::Status taskStatus,
                                       const model::Timestamp& lastModified, const model::Timestamp& expiryDate,
                                       const model::Timestamp& acceptDate,
                                       const db_model::EncryptedBlob& healthCareProviderPrescription) const
{
    TVLOG(2) << mQueries.updateTask_activateTask.query;
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres, "PostgreSQL:activateTask");

    const auto status = model::Task::toNumericalStatus(taskStatus);

    const pqxx::result result = transaction.exec_params(
        mQueries.updateTask_activateTask.query, taskId.toDatabaseId(), encryptedKvnr.binarystring(),
        hashedKvnr.binarystring(), lastModified.toXsDateTime(), expiryDate.toGermanDate(), acceptDate.toGermanDate(),
        static_cast<int>(status), healthCareProviderPrescription.binarystring());
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.empty(), "Expected an empty result");
}


void PostgresBackendTask::updateTaskMedicationDispenseReceipt(
    pqxx::work& transaction, const model::PrescriptionId& taskId, const model::Task::Status& taskStatus,
    const model::Timestamp& lastModified, const db_model::EncryptedBlob& medicationDispense,
    BlobId medicationDispenseBlobId, const db_model::HashedTelematikId& telematikId,
    const model::Timestamp& whenHandedOver, const std::optional<model::Timestamp>& whenPrepared,
    const db_model::EncryptedBlob& receipt) const
{
    TVLOG(2) << mQueries.updateTask_medicationDispenseReceipt.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(
        DurationConsumer::categoryPostgres, "PostgreSQL:updateTaskMedicationDispenseReceipt");

    const auto status = model::Task::toNumericalStatus(taskStatus);
    const pqxx::result result = transaction.exec_params(
        mQueries.updateTask_medicationDispenseReceipt.query, taskId.toDatabaseId(), static_cast<int>(status),
        lastModified.toXsDateTime(), medicationDispense.binarystring(), gsl::narrow<int32_t>(medicationDispenseBlobId),
        receipt.binarystring(), whenHandedOver.toXsDateTime(),
        whenPrepared ? std::make_optional(whenPrepared->toXsDateTime()) : std::nullopt, telematikId.binarystring());
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.empty(), "Expected an empty result");
}


void PostgresBackendTask::updateTaskClearPersonalData(pqxx::work& transaction, const model::PrescriptionId& taskId,
                                                      model::Task::Status taskStatus,
                                                      const model::Timestamp& lastModified) const
{
    TVLOG(2) << mQueries.updateTask_deletePersonalData.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres,
                                                                        "PostgreSQL:updateTaskClearPersnalData");

    const auto status = model::Task::toNumericalStatus(taskStatus);

    const pqxx::result result =
        transaction.exec_params(mQueries.updateTask_deletePersonalData.query, taskId.toDatabaseId(),
                                static_cast<int>(status), lastModified.toXsDateTime());
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.empty(), "Expected an empty result");
}


std::optional<db_model::Task> PostgresBackendTask::retrieveTaskForUpdate(pqxx::work& transaction,
                                                                         const model::PrescriptionId& taskId)
{
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres, "PostgreSQL:retrieveTaskForUpdate");

    std::string query(mQueries.retrieveTaskById.query);
    query.append("    FOR UPDATE");
    TVLOG(2) << query;
    const pqxx::result result = transaction.exec_params(query, taskId.toDatabaseId());

    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() <= 1, "Too many results in result set.");
    if (! result.empty())
    {
        return taskFromQueryResultRow(result.front(), TaskQueryIndexes(), mPrescriptionType);
    }
    return {};
}


::std::optional<::db_model::Task>
PostgresBackendTask::retrieveTaskForUpdateAndPrescription(::pqxx::work& transaction,
                                                          const ::model::PrescriptionId& taskId)
{
    TVLOG(2) << mQueries.retrieveTaskByIdForUpdatePlusPrescription.query;

    const auto timerKeepAlive = ::DurationConsumer::getCurrent().getTimer(
        DurationConsumer::categoryPostgres, "PostgreSQL:retrieveTaskForUpdateAndPrescription");

    const auto result =
        transaction.exec_params(mQueries.retrieveTaskByIdForUpdatePlusPrescription.query, taskId.toDatabaseId());

    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() <= 1, "Too many results in result set.");
    if (! result.empty())
    {
        TaskQueryIndexes indexes;
        indexes.healthcareProviderPrescriptionIndex = 11;

        return taskFromQueryResultRow(result.front(), indexes, mPrescriptionType);
    }

    return {};
}


std::optional<db_model::Task> PostgresBackendTask::retrieveTaskAndReceipt(pqxx::work& transaction,
                                                                          const model::PrescriptionId& taskId)
{
    TVLOG(2) << mQueries.retrieveTaskByIdPlusReceipt.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres,
                                                                        "PostgreSQL:retrieveTaskAndReceipt");

    const pqxx::result result =
        transaction.exec_params(mQueries.retrieveTaskByIdPlusReceipt.query, taskId.toDatabaseId());

    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() <= 1, "Too many results in result set.");
    if (! result.empty())
    {
        Expect(result.front().size() == 11,
               "Invalid number of fields in result row: " + std::to_string(result.front().size()));
        TaskQueryIndexes indexes;
        indexes.accessCodeIndex.reset();
        indexes.secretIndex = 9;
        indexes.receiptIndex = 10;
        return taskFromQueryResultRow(result.front(), indexes, mPrescriptionType);
    }
    return {};
}


std::optional<db_model::Task> PostgresBackendTask::retrieveTaskAndPrescription(pqxx::work& transaction,
                                                                               const model::PrescriptionId& taskId)
{
    TVLOG(2) << mQueries.retrieveTaskByIdPlusPrescription.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres,
                                                                        "PostgreSQL:retrieveTaskAndPrescription");

    const pqxx::result result =
        transaction.exec_params(mQueries.retrieveTaskByIdPlusPrescription.query, taskId.toDatabaseId());

    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() <= 1, "Too many results in result set.");
    if (! result.empty())
    {
        Expect(result.front().size() == 11,
               "Invalid number of fields in result row: " + std::to_string(result.front().size()));
        TaskQueryIndexes indexes;
        indexes.secretIndex.reset();
        indexes.healthcareProviderPrescriptionIndex = 10;
        return taskFromQueryResultRow(result.front(), indexes, mPrescriptionType);
    }
    return {};
}


::std::optional<::db_model::Task>
PostgresBackendTask::retrieveTaskAndPrescriptionAndReceipt(::pqxx::work& transaction,
                                                           const ::model::PrescriptionId& taskId)
{
    TVLOG(2) << mQueries.retrieveTaskByIdPlusPrescriptionPlusReceipt.query;

    const auto timerKeepAlive = ::DurationConsumer::getCurrent().getTimer(
        DurationConsumer::categoryPostgres, "PostgreSQL:retrieveTaskByIdPlusPrescriptionPlusReceipt");

    const auto result =
        transaction.exec_params(mQueries.retrieveTaskByIdPlusPrescriptionPlusReceipt.query, taskId.toDatabaseId());

    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.size() <= 1, "Too many results in result set.");
    if (! result.empty())
    {
        Expect(result.front().size() == 13,
               "Invalid number of fields in result row: " + std::to_string(result.front().size()));
        TaskQueryIndexes indexes;
        indexes.secretIndex = 10;
        indexes.healthcareProviderPrescriptionIndex = 11;
        indexes.receiptIndex = 12;

        return taskFromQueryResultRow(result.front(), indexes, mPrescriptionType);
    }

    return {};
}


uint64_t PostgresBackendTask::countAllTasksForPatient(pqxx::work& transaction, const db_model::HashedKvnr& kvnr,
                                                      const std::optional<UrlArguments>& search) const
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryPostgres,
                                                                        "PostgreSQL:countAllTasksForPatient");
    return PostgresBackendHelper::executeCountQuery(transaction, mQueries.countAllTasksByKvnr.query, kvnr, search,
                                                    "tasks");
}

void PostgresBackendTask::deleteChargeItemSupportingInformation(::pqxx::work& transaction,
                                                                const ::model::PrescriptionId& id)
{
    TVLOG(2) << mQueries.deleteChargeItemSupportingInformation.query;
    Expect(id.type() == mPrescriptionType,
           "deleteChargeItemSupportingInformation: Invalid prescription type for: " + id.toString());

    const auto timerKeepAlive = ::DurationConsumer::getCurrent().getTimer(
        DurationConsumer::categoryPostgres, "PostgreSQL:deleteChargeItemSupportingInformation");
    transaction.exec_params0(mQueries.deleteChargeItemSupportingInformation.query, id.toDatabaseId());
}

void PostgresBackendTask::clearAllChargeItemSupportingInformation(::pqxx::work& transaction,
                                                                  const ::db_model::HashedKvnr& kvnr) const
{
    TVLOG(2) << mQueries.clearAllChargeItemSupportingInformation.query;
    const auto timerKeepAlive = ::DurationConsumer::getCurrent().getTimer(
        DurationConsumer::categoryPostgres, "PostgreSQL:clearAllChargeItemSupportingInformation");
    transaction.exec_params0(mQueries.clearAllChargeItemSupportingInformation.query, kvnr.binarystring());
}

namespace taskFromQueryResultRowHelper
{
db_model::Blob getSalt(const pqxx::row& resultRow, model::Task::Status status,
                       const PostgresBackendTask::TaskQueryIndexes& indexes)
{
    if (resultRow.at(indexes.saltIndex).is_null())
    {
        Expect3(status == model::Task::Status::cancelled, "Missing salt data in task.", std::logic_error);
    }
    else
    {
        return db_model::Blob{resultRow.at(indexes.saltIndex).as<db_model::postgres_bytea>()};
    }
    return {};
}

bool checkIndexAndRow(const std::optional<pqxx::row::size_type>& row, const pqxx::row& resultRow)
{
    return row.has_value() && ! resultRow.at(*row).is_null();
}
}

db_model::Task PostgresBackendTask::taskFromQueryResultRow(const pqxx::row& resultRow,
                                                           const PostgresBackendTask::TaskQueryIndexes& indexes,
                                                           model::PrescriptionType prescriptionType)
{
    Expect(resultRow.size() >= 8, "Too few fields in result row");

    const auto& prescriptionId =
        model::PrescriptionId::fromDatabaseId(prescriptionType,
                                              resultRow.at(indexes.prescriptionIdIndex).as<int64_t>());
    const auto keyBlobId = gsl::narrow<BlobId>(resultRow.at(indexes.keyBlobIdIndex).as<int32_t>());
    const auto status =
        model::Task::fromNumericalStatus(gsl::narrow<int8_t>(resultRow.at(indexes.statusIndex).as<int>()));

    db_model::Blob salt = taskFromQueryResultRowHelper::getSalt(resultRow, status, indexes);

    const model::Timestamp authoredOn{resultRow.at(indexes.authoredOnIndex).as<double>()};
    const model::Timestamp lastModified{resultRow.at(indexes.lastModifiedIndex).as<double>()};
    db_model::Task dbTask{prescriptionId, keyBlobId, salt, status, authoredOn, lastModified};

    if (taskFromQueryResultRowHelper::checkIndexAndRow(indexes.accessCodeIndex, resultRow))
    {
        dbTask.accessCode.emplace(resultRow.at(*indexes.accessCodeIndex).as<db_model::postgres_bytea>());
    }

    if (! resultRow.at(indexes.kvnrIndex).is_null())
    {
        dbTask.kvnr.emplace(resultRow.at(indexes.kvnrIndex).as<db_model::postgres_bytea>());
    }

    if (! resultRow.at(indexes.expiryDateIndex).is_null())
    {
        dbTask.expiryDate.emplace(resultRow.at(indexes.expiryDateIndex).as<double>());
    }

    if (! resultRow.at(indexes.acceptDateIndex).is_null())
    {
        dbTask.acceptDate.emplace(resultRow.at(indexes.acceptDateIndex).as<double>());
    }

    if (taskFromQueryResultRowHelper::checkIndexAndRow(indexes.secretIndex, resultRow))
    {
        dbTask.secret.emplace(resultRow.at(*indexes.secretIndex).as<db_model::postgres_bytea>());
    }

    if (taskFromQueryResultRowHelper::checkIndexAndRow(indexes.receiptIndex, resultRow))
    {
        dbTask.receipt.emplace(resultRow.at(*indexes.receiptIndex).as<db_model::postgres_bytea>());
    }

    if (taskFromQueryResultRowHelper::checkIndexAndRow(indexes.healthcareProviderPrescriptionIndex, resultRow))
    {
        dbTask.healthcareProviderPrescription.emplace(
            resultRow.at(*indexes.healthcareProviderPrescriptionIndex).as<db_model::postgres_bytea>());
    }

    return dbTask;
}
