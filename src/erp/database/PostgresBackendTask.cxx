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

    QUERY(updateTask_storeChargeInformation, R"--(
        UPDATE )--" + taskTableName() + R"--(
        SET charge_item_enterer = $2, charge_item_entered_date = $3, charge_item = $4, dispense_item = $5
        WHERE prescription_id = $1
        )--")

    QUERY(retrieveAllChargeItemsForInsurant, R"--(
        SELECT prescription_id, task_key_blob_id, salt, EXTRACT(EPOCH FROM authored_on), charge_item
        FROM )--" + taskTableName() + R"--(
        WHERE kvnr_hashed = $1
            AND charge_item_enterer IS NOT NULL
            AND charge_item_entered_date IS NOT NULL
            AND charge_item IS NOT NULL
        )--")

    QUERY(retrieveAllChargeItemsForPharmacy, R"--(
        SELECT prescription_id, task_key_blob_id, salt, EXTRACT(EPOCH FROM authored_on), charge_item
        FROM )--" + taskTableName() + R"--(
        WHERE charge_item_enterer = $1
            AND charge_item_enterer IS NOT NULL
            AND charge_item_entered_date IS NOT NULL
            AND charge_item IS NOT NULL
        )--")

    QUERY(retrieveChargeItemAndDispenseItemAndPrescriptionAndReceipt, R"--(
        SELECT prescription_id, kvnr, EXTRACT(EPOCH FROM last_modified), EXTRACT(EPOCH FROM authored_on),
            EXTRACT(EPOCH FROM expiry_date), EXTRACT(EPOCH FROM accept_date), status, salt, task_key_blob_id,
            access_code, secret, healthcare_provider_prescription, receipt, charge_item, dispense_item
        FROM )--" + taskTableName() + R"--(
        WHERE prescription_id = $1
            AND charge_item_enterer IS NOT NULL
            AND charge_item_entered_date IS NOT NULL
            AND charge_item IS NOT NULL
            AND dispense_item IS NOT NULL
        )--")

    QUERY(retrieveChargeInformation, R"--(
        SELECT prescription_id, task_key_blob_id, salt, EXTRACT(EPOCH FROM authored_on), charge_item, dispense_item
        FROM )--" + taskTableName() + R"--(
        WHERE prescription_id = $1
            AND charge_item_enterer IS NOT NULL
            AND charge_item_entered_date IS NOT NULL
            AND charge_item IS NOT NULL
            AND dispense_item IS NOT NULL
        )--")

    QUERY(deleteChargeInformation, R"--(
        UPDATE )--" + taskTableName() + R"--(
        SET charge_item_enterer = NULL, charge_item_entered_date = NULL, charge_item = NULL, dispense_item = NULL, receipt = NULL
        WHERE prescription_id = $1
        )--")

    QUERY(clearAllChargeInformation,R"--(
        UPDATE )--" + taskTableName() + R"--(
        SET charge_item_enterer = NULL, charge_item_entered_date = NULL, charge_item = NULL, dispense_item = NULL
        WHERE kvnr_hashed = $1
    )--")

    QUERY(countChargeInformationForInsurant, R"--(
        SELECT COUNT(*)
        FROM )--" + taskTableName() + R"--(
        WHERE kvnr_hashed = $1
            AND charge_item_enterer IS NOT NULL
            AND charge_item_entered_date IS NOT NULL
            AND charge_item IS NOT NULL
    )--")

    QUERY(countChargeInformationForPharmacy, R"--(
        SELECT COUNT(*)
        FROM )--" + taskTableName() + R"--(
        WHERE charge_item_enterer = $1
            AND charge_item_enterer IS NOT NULL
            AND charge_item_entered_date IS NOT NULL
            AND charge_item IS NOT NULL
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
    }
    Fail("invalid mPrescriptionType type : " + std::to_string(uintmax_t(mPrescriptionType)));
}


std::tuple<model::PrescriptionId, model::Timestamp> PostgresBackendTask::createTask(pqxx::work& transaction,
                                                                                    model::Task::Status taskStatus,
                                                                                    const model::Timestamp& lastUpdated,
                                                                                    const model::Timestamp& created)
{
    TVLOG(2) << mQueries.createTask.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:createTask");

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
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:updateTask");

    auto result = transaction.exec_params(mQueries.updateTask.query, taskId.toDatabaseId(), blobId, salt.binarystring(),
                                          accessCode.binarystring());
    TVLOG(2) << "got " << result.size() << " results";

    Expect(result.empty(), "No result rows expected");
}


std::tuple<BlobId, db_model::Blob, model::Timestamp>
PostgresBackendTask::getTaskKeyData(pqxx::work& transaction, const model::PrescriptionId& taskId) const
{
    TVLOG(2) << mQueries.getTaskKeyData.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:getTaskKeyData");

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
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:updateTaskStatusAndSecret");

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
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:activateTask");

    const auto status = model::Task::toNumericalStatus(taskStatus);

    const pqxx::result result = transaction.exec_params(
        mQueries.updateTask_activateTask.query, taskId.toDatabaseId(), encryptedKvnr.binarystring(),
        hashedKvnr.binarystring(), lastModified.toXsDateTime(), expiryDate.toXsDateTime(), acceptDate.toXsDateTime(),
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
    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer("PostgreSQL:updateTaskMedicationDispenseReceipt");

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
    // TODO: ERP-8162: also clear ChargeInformation
    TVLOG(2) << mQueries.updateTask_deletePersonalData.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:updateTaskClearPersnalData");

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
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveTaskForUpdate");

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

    const auto timerKeepAlive =
        ::DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveTaskForUpdateAndPrescription");

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
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveTaskAndReceipt");

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
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveTaskAndPrescription");

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

    const auto timerKeepAlive =
        ::DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveTaskByIdPlusPrescriptionPlusReceipt");

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
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:countAllTasksForPatient");
    return PostgresBackendHelper::executeCountQuery(transaction, mQueries.countAllTasksByKvnr.query, kvnr, search, "tasks");
}

void PostgresBackendTask::storeChargeInformation(::pqxx::work& transaction,
                                                 const db_model::HashedTelematikId& pharmacyTelematikId,
                                                 model::PrescriptionId id, const model::Timestamp& enteredDate,
                                                 const db_model::EncryptedBlob& chargeItem,
                                                 const db_model::EncryptedBlob& dispenseItem)
{
    TVLOG(2) << mQueries.updateTask_storeChargeInformation.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:storeChargeInformation");
    Expect(id.type() == mPrescriptionType, "storeChargeInformation: Invalid prescription type for: " + id.toString());
    const pqxx::result result =
        transaction.exec_params(mQueries.updateTask_storeChargeInformation.query,
                                id.toDatabaseId(),
                                pharmacyTelematikId.binarystring(),
                                enteredDate.toXsDateTime(),
                                chargeItem.binarystring(),
                                dispenseItem.binarystring());
    TVLOG(2) << "got " << result.size() << " results";
    Expect(result.empty(), "Expected an empty result");
}


std::vector<db_model::ChargeItem>
PostgresBackendTask::retrieveAllChargeItems(::pqxx::work& transaction, const QueryDefinition& queryDef,
                                            const db_model::HashedId& hashedId,
                                            const std::optional<UrlArguments>& search) const
{
    std::string query{queryDef.query};
    if (search.has_value())
    {
        query.append(search->getSqlExpression(transaction.conn(), "                "));
    }
    TVLOG(2) << query;
    TVLOG(2) << "hashedId = " << hashedId.toHex();
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveAllChargeItems");

    const pqxx::result dbResult =
        transaction.exec_params(query, hashedId.binarystring());

    TVLOG(2) << "got " << dbResult.size() << " results";
    std::vector<db_model::ChargeItem> result;
    result.reserve(dbResult.size());
    for (const auto& row : dbResult)
    {
        Expect3(row.size() == 5, "Unexpected number of columns.", std::logic_error);
        result.emplace_back(chargeItemFromQueryResultRow(row));
    }
    return result;
}

std::vector<db_model::ChargeItem>
PostgresBackendTask::retrieveAllChargeItemsForInsurant(::pqxx::work& transaction,
                                                       const db_model::HashedKvnr& kvnr,
                                                       const std::optional<UrlArguments>& search) const
{
    return retrieveAllChargeItems(transaction, mQueries.retrieveAllChargeItemsForInsurant, kvnr, search);
}

std::vector<db_model::ChargeItem>
PostgresBackendTask::retrieveAllChargeItemsForPharmacy(::pqxx::work& transaction,
                                                       const db_model::HashedTelematikId& pharmacyTelematikId,
                                                       const std::optional<UrlArguments>& search) const
{
    return retrieveAllChargeItems(transaction, mQueries.retrieveAllChargeItemsForPharmacy, pharmacyTelematikId, search);
}

std::tuple<std::optional<db_model::Task>, std::optional<db_model::EncryptedBlob>, std::optional<db_model::EncryptedBlob>>
PostgresBackendTask::retrieveChargeItemAndDispenseItemAndPrescriptionAndReceipt(::pqxx::work& transaction, const model::PrescriptionId& id) const
{
    TVLOG(2) << mQueries.retrieveChargeItemAndDispenseItemAndPrescriptionAndReceipt.query;
    Expect(id.type() == mPrescriptionType,
           "retrieveChargeItemAndDispenseItemAndPrescriptionAndReceipt: Invalid prescription type for: " + id.toString());

    const auto timerKeepAlive =
        DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveChargeItemAndDispenseItemAndPrescriptionAndReceipt");

    const pqxx::result dbResult =
        transaction.exec_params(mQueries.retrieveChargeItemAndDispenseItemAndPrescriptionAndReceipt.query, id.toDatabaseId());

    TVLOG(2) << "got " << dbResult.size() << " results";
    Expect(dbResult.size() <= 1, "Too many results in result set.");
    if (! dbResult.empty())
    {
        const auto& row = dbResult[0];
        Expect(dbResult.front().size() == 15,
               "Invalid number of fields in result row: " + std::to_string(dbResult.front().size()));
        TaskQueryIndexes indexes;
        indexes.secretIndex = 10;
        indexes.healthcareProviderPrescriptionIndex = 11;
        indexes.receiptIndex = 12;

        return {taskFromQueryResultRow(dbResult.front(), indexes, mPrescriptionType),
                db_model::EncryptedBlob{row[13].as<db_model::postgres_bytea>()},
                db_model::EncryptedBlob{row[14].as<db_model::postgres_bytea>()}};
    }
    return {};
}

std::tuple<db_model::ChargeItem, db_model::EncryptedBlob>
PostgresBackendTask::retrieveChargeInformation(::pqxx::work& transaction, const model::PrescriptionId& id) const
{
    TVLOG(2) << mQueries.retrieveChargeInformation.query;
    Expect(id.type() == mPrescriptionType,
           "retrieveChargeInformation: Invalid prescription type for: " + id.toString());

    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveChargeInformation");

    const pqxx::result dbResult =
        transaction.exec_params(mQueries.retrieveChargeInformation.query, id.toDatabaseId());

    TVLOG(2) << "got " << dbResult.size() << " results";
    ErpExpectWithDiagnostics(dbResult.size() == 1, HttpStatus::NotFound, "no such task", id.toString());
    const auto& row = dbResult[0];
    Expect3(row.size() == 6, "Unexpected number of columns.", std::logic_error);
    return {chargeItemFromQueryResultRow(row), db_model::EncryptedBlob{row[5].as<db_model::postgres_bytea>()}};
}

std::tuple<db_model::ChargeItem, db_model::EncryptedBlob>
PostgresBackendTask::retrieveChargeInformationForUpdate(::pqxx::work& transaction, const model::PrescriptionId& id) const
{
    std::string query = mQueries.retrieveChargeInformation.query;
    query.append("    FOR UPDATE");

    TVLOG(2) << query;
    Expect(id.type() == mPrescriptionType,
           "retrieveChargeInformationForUpdate: Invalid prescription type for: " + id.toString());

    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:retrieveChargeInformationForUpdate");

    const pqxx::result dbResult = transaction.exec_params(query, id.toDatabaseId());

    TVLOG(2) << "got " << dbResult.size() << " results";
    ErpExpectWithDiagnostics(dbResult.size() == 1, HttpStatus::NotFound, "no such task", id.toString());
    const auto& row = dbResult[0];
    Expect3(row.size() == 6, "Unexpected number of columns.", std::logic_error);
    return {chargeItemFromQueryResultRow(row), db_model::EncryptedBlob{row[5].as<db_model::postgres_bytea>()}};
}

db_model::ChargeItem PostgresBackendTask::chargeItemFromQueryResultRow(const pqxx::row& row) const
{
    return db_model::ChargeItem{
            model::PrescriptionId::fromDatabaseId(mPrescriptionType, row[0].as<int64_t>()),
            row[1].as<BlobId>(),
            db_model::Blob{row[2].as<db_model::postgres_bytea>()},
            model::Timestamp{row[3].as<double>()},
            db_model::EncryptedBlob{row[4].as<db_model::postgres_bytea>()}
        };
}

void PostgresBackendTask::clearAllChargeInformation(::pqxx::work& transaction, const db_model::HashedKvnr& kvnr) const
{
    TVLOG(2) << mQueries.clearAllChargeInformation.query;
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:clearAllChargeInformation");
    transaction.exec_params0(mQueries.clearAllChargeInformation.query, kvnr.binarystring());
}

void PostgresBackendTask::deleteChargeInformation(::pqxx::work& transaction, const model::PrescriptionId& id)
{
    TVLOG(2) << mQueries.deleteChargeInformation.query;
    Expect(id.type() == mPrescriptionType,
           "deleteChargeInformation: Invalid prescription type for: " + id.toString());

    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:deleteChargeInformation");
    transaction.exec_params0(mQueries.deleteChargeInformation.query, id.toDatabaseId());
}

uint64_t PostgresBackendTask::countChargeInformationForInsurant(pqxx::work& transaction,
                                                                const db_model::HashedKvnr& kvnr,
                                                                const std::optional<UrlArguments>& search) const
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:countChargeInformationForInsurant");
    return PostgresBackendHelper::executeCountQuery(transaction, mQueries.countChargeInformationForInsurant.query,
                                                    kvnr, search, "ChargeItem for insurant");
}

uint64_t PostgresBackendTask::countChargeInformationForPharmacy(pqxx::work& transaction,
                                                                const db_model::HashedTelematikId& pharmacyTelematikId,
                                                                const std::optional<UrlArguments>& search) const
{
    const auto timerKeepAlive = DurationConsumer::getCurrent().getTimer("PostgreSQL:countChargeInformationForPharmacy");
    return PostgresBackendHelper::executeCountQuery(transaction, mQueries.countChargeInformationForPharmacy.query,
                                                    pharmacyTelematikId, search,"ChargeItem for pharmacy");
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
