/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/mock/MockTaskTable.hxx"

#include "erp/util/Gsl.hxx"
#include "erp/util/TLog.hxx"
#include "test/mock/MockAccountTable.hxx"
#include "test/mock/TestUrlArguments.hxx"

MockTaskTable::Row::Row(model::Timestamp initAuthoredOn, model::Timestamp initLastModified)
    : authoredOn(initAuthoredOn)
    , lastModified(initLastModified)
{
}

MockTaskTable::MockTaskTable(MockAccountTable& accountTable, const model::PrescriptionType& prescriptionType)
    : mAccounts(accountTable)
    , mPrescriptionType(prescriptionType)
{
}


std::tuple<BlobId, db_model::Blob, model::Timestamp>
MockTaskTable::getTaskKeyData(const model::PrescriptionId& taskId)
{
    auto taskRowIt = mTasks.find(taskId.toDatabaseId());
    Expect(taskRowIt != mTasks.end(), "no such task:" + taskId.toString());
    const auto& taskRow = taskRowIt->second;
    return std::make_tuple(taskRow.taskKeyBlobId.value(), *taskRow.salt, taskRow.authoredOn);
}

std::optional<db_model::Task> MockTaskTable::retrieveTaskBasics(const model::PrescriptionId& taskId)
{
    return select(taskId.toDatabaseId(), {prescription_id, kvnr, last_modified, authored_on,
            expiry_date, accept_date, status, salt, task_key_blob_id,
            access_code, secret});
}

std::vector<db_model::Task> MockTaskTable::retrieveAllTasksForPatient(const db_model::HashedKvnr& kvnrHashed,
                                                                      const std::optional<UrlArguments>& search,
                                                                      bool applyOnlySearch) const
{
    static constexpr auto selectedFields = {prescription_id, kvnr, last_modified, authored_on,
            expiry_date, accept_date, status, salt, task_key_blob_id
                                           };
    std::vector<db_model::Task> allTasks;
    for (const auto& task : mTasks)
    {
        if (task.second.kvnrHashed == kvnrHashed)
        {
            allTasks.emplace_back(select(task.first, selectedFields).value());
        }
    }

    if (search.has_value())
    {
        if(applyOnlySearch)
            return TestUrlArguments(search.value()).applySearch(std::move(allTasks));
        else
            return TestUrlArguments(search.value()).apply(std::move(allTasks));
    }
    else
        return allTasks;
}

uint64_t MockTaskTable::countAllTasksForPatient(const db_model::HashedKvnr& kvnr,
                                                const std::optional<UrlArguments>& search) const
{
    auto allTasks = retrieveAllTasksForPatient(kvnr, search, true/*applyOnlySearch*/);
    return allTasks.size();
}

std::tuple<model::PrescriptionId, model::Timestamp> MockTaskTable::createTask(model::PrescriptionType prescriptionType,
                                                                              model::Task::Status taskStatus,
                                                                              const model::Timestamp& lastUpdated,
                                                                              const model::Timestamp& created)
{
    ++mTaskId;
    auto id = model::PrescriptionId::fromDatabaseId(prescriptionType, mTaskId);
    auto& row = createRow(id, lastUpdated, created);
    row.status = taskStatus;
    TVLOG(1) << "createTask: " + id.toString();
    return std::make_tuple(id, created);
}

MockTaskTable::Row& MockTaskTable::createRow(const model::PrescriptionId& taskId, const model::Timestamp& lastUpdated,
                                                                               const model::Timestamp& created)
{
    auto [newTask, inserted] = mTasks.try_emplace(taskId.toDatabaseId(), created, lastUpdated);
    Expect3(inserted, "Failed to insert new Task: " + taskId.toString(), std::logic_error);
    return newTask->second;
}


void MockTaskTable::activateTask(const model::PrescriptionId& taskId,
                                const db_model::EncryptedBlob& encryptedKvnr,
                                const db_model::HashedKvnr& hashedKvnr,
                                model::Task::Status taskStatus,
                                const model::Timestamp& lastModified,
                                const model::Timestamp& expiryDate,
                                const model::Timestamp& acceptDate,
                                const db_model::EncryptedBlob& healthCareProviderPrescription)
{
    auto taskRowIt = mTasks.find(taskId.toDatabaseId());
    Expect(taskRowIt != mTasks.end(), "no such task:" + taskId.toString());
    auto& taskRow = taskRowIt->second;
    taskRow.kvnr = encryptedKvnr;
    taskRow.kvnrHashed = hashedKvnr;
    taskRow.status = taskStatus;
    taskRow.lastModified = lastModified;
    taskRow.expiryDate = expiryDate;
    taskRow.acceptDate = acceptDate;
    taskRow.healthcareProviderPrescription = healthCareProviderPrescription;
}

void MockTaskTable::updateTask(const model::PrescriptionId& taskId,
                              const db_model::EncryptedBlob& accessCode,
                              int32_t keyGenerationId,
                              const db_model::Blob& salt)
{
    auto taskRowIt = mTasks.find(taskId.toDatabaseId());
    Expect(taskRowIt != mTasks.end(), "no such task:" + taskId.toString());
    auto& taskRow = taskRowIt->second;
    taskRow.accessCode = accessCode;
    taskRow.taskKeyBlobId = keyGenerationId;
    taskRow.salt = salt;
}

void MockTaskTable::updateTaskStatusAndSecret(const model::PrescriptionId& taskId, model::Task::Status taskStatus, const model::Timestamp& lastModifiedDate, const std::optional<db_model::EncryptedBlob>& taskSecret)
{
    auto taskRowIt = mTasks.find(taskId.toDatabaseId());
    Expect(taskRowIt != mTasks.end(), "no such task:" + taskId.toString());
    auto& taskRow = taskRowIt->second;
    taskRow.status = taskStatus;
    taskRow.lastModified = lastModifiedDate;
    taskRow.secret = taskSecret;
}
void MockTaskTable::updateTaskMedicationDispenseReceipt(const model::PrescriptionId& taskId,
                                                       const model::Task::Status& taskStatus,
                                                       const model::Timestamp& lastModified,
                                                       const db_model::EncryptedBlob& medicationDispense,
                                                       BlobId medicationDispenseBlobId,
                                                       const db_model::HashedTelematikId& telematicId,
                                                       const model::Timestamp& whenHandedOver,
                                                       const std::optional<model::Timestamp>& whenPrepared,
                                                       const db_model::EncryptedBlob& taskReceipt)
{
    auto taskRowIt = mTasks.find(taskId.toDatabaseId());
    Expect(taskRowIt != mTasks.end(), "no such task:" + taskId.toString());
    auto& taskRow = taskRowIt->second;
    taskRow.status = taskStatus;
    taskRow.lastModified = lastModified;
    taskRow.medicationDispenseBundle = medicationDispense;
    taskRow.medicationDispenseKeyBlobId = medicationDispenseBlobId;
    taskRow.performer = telematicId;
    taskRow.whenHandedOver = whenHandedOver;
    taskRow.whenPrepared = whenPrepared;
    taskRow.receipt = taskReceipt;
}

void MockTaskTable::updateTaskClearPersonalData(const model::PrescriptionId& taskId, model::Task::Status taskStatus, const model::Timestamp& lastModified)
{
    auto taskRowIt = mTasks.find(taskId.toDatabaseId());
    Expect(taskRowIt != mTasks.end(), "no such task:" + taskId.toString());
    auto& taskRow = taskRowIt->second;
    taskRow.status = taskStatus;
    taskRow.lastModified = lastModified;
    taskRow.kvnr.reset();
    taskRow.kvnrHashed.reset();
    taskRow.salt.reset();
    taskRow.accessCode.reset();
    taskRow.secret.reset();
    taskRow.healthcareProviderPrescription.reset();
    taskRow.receipt.reset();
    taskRow.whenHandedOver.reset();
    taskRow.whenPrepared.reset();
    taskRow.performer.reset();
    taskRow.medicationDispenseBundle.reset();
    clearChargeInformation(taskRow);
}

std::vector<TestUrlArguments::SearchMedicationDispense>
MockTaskTable::retrieveAllMedicationDispenses(const db_model::HashedKvnr& kvnr,
                                              const std::optional<model::PrescriptionId>& prescriptionId) const
{
    std::vector<TestUrlArguments::SearchMedicationDispense> medicationDispenses;
    for (const auto& task : mTasks)
    {
        if (task.second.kvnrHashed == kvnr && task.second.medicationDispenseBundle)
        {
            if (!prescriptionId.has_value() || (prescriptionId->toDatabaseId() == task.first))
            {
                auto id = model::PrescriptionId::fromDatabaseId(
                    mPrescriptionType, task.first);
                auto salt = mAccounts.getSalt(kvnr, db_model::MasterKeyType::medicationDispense,
                                              task.second.medicationDispenseKeyBlobId.value());
                Expect(salt, "No Account data found.");
                medicationDispenses.emplace_back(
                    db_model::MedicationDispense{
                        id, *task.second.medicationDispenseBundle,
                        gsl::narrow<BlobId>(*task.second.medicationDispenseKeyBlobId), *salt},
                    *task.second.performer, *task.second.whenHandedOver, task.second.whenPrepared);
            }
        }
    }
    return medicationDispenses;
}

std::optional<db_model::Task> MockTaskTable::retrieveTaskAndReceipt(const model::PrescriptionId& taskId)
{
    return select(taskId.toDatabaseId(), {prescription_id, kvnr, last_modified, authored_on,
            expiry_date, accept_date, status, salt, task_key_blob_id,
            secret, receipt});
}

::std::optional<::db_model::Task>
MockTaskTable::retrieveTaskForUpdateAndPrescription(const model::PrescriptionId& taskId)
{
    return select(taskId.toDatabaseId(),
                  {prescription_id, kvnr, last_modified, authored_on, expiry_date, accept_date, status, salt,
                   task_key_blob_id, access_code, secret, healthcare_provider_prescription});
}


std::optional<db_model::Task> MockTaskTable::retrieveTaskAndPrescription(const model::PrescriptionId& taskId)
{
    return select(taskId.toDatabaseId(), {prescription_id, kvnr, last_modified, authored_on,
                                          expiry_date, accept_date, status, salt, task_key_blob_id, access_code,
                                          healthcare_provider_prescription, receipt});
}


std::optional<db_model::Task> MockTaskTable::retrieveTaskAndPrescriptionAndReceipt(const model::PrescriptionId& taskId)
{
    return select(taskId.toDatabaseId(),
                  {prescription_id, kvnr, last_modified, authored_on, expiry_date, accept_date, status, salt,
                   task_key_blob_id, access_code, secret, healthcare_provider_prescription, receipt});
}


void MockTaskTable::deleteTask (const model::PrescriptionId& taskId)
{
    Expect(mTasks.erase(taskId.toDatabaseId()), "no such task: " + taskId.toString());
}

void MockTaskTable::storeChargeInformation(const db_model::HashedTelematikId& pharmacyId, model::PrescriptionId id,
                                           const model::Timestamp& enteredDate,
                                           const db_model::EncryptedBlob& chargeItem,
                                           const db_model::EncryptedBlob& dispenseItem)
{
    Expect3(id.type() == mPrescriptionType,
            "wrong task table (" + std::to_string(int(mPrescriptionType)) + ") for: " + id.toString(),
            std::logic_error);
    auto taskRowIt = mTasks.find(id.toDatabaseId());
    Expect(taskRowIt != mTasks.end(), "no such task:" + id.toString());
    auto& taskRow = taskRowIt->second;
    taskRow.chargeItemEnterer = pharmacyId;
    taskRow.chargeItem = chargeItem;
    taskRow.dispenseItem = dispenseItem;
    taskRow.chargeItemEnteredDate = enteredDate;
}

std::vector<db_model::ChargeItem>
MockTaskTable::retrieveAllChargeItemsForPharmacy(const db_model::HashedTelematikId& requestingPharmacy,
                                                 const std::optional<UrlArguments>& search,
                                                 const bool applyPaging) const
{
    std::vector<db_model::ChargeItem> result;
    auto testArgs = search ? std::make_optional<TestUrlArguments>(*search) : std::nullopt;
    for (const auto& [id, row] : mTasks)
    {
        if (!row.chargeItem.has_value() || requestingPharmacy != row.chargeItemEnterer)
        {
            continue;
        }
        if (! testArgs || (testArgs->matches("patient", row.kvnrHashed) &&
                           testArgs->matches("entered-date", row.chargeItemEnteredDate)))
        {
            auto prescriptionId = model::PrescriptionId::fromDatabaseId(mPrescriptionType, id);
            // TODO return correct blob id / salt
            result.emplace_back(db_model::ChargeItem(prescriptionId, row.taskKeyBlobId.value(),
                                                            row.salt.value(), row.authoredOn,
                                                            row.chargeItem.value()));
        }
    }

    if(applyPaging && search.has_value() && search->pagingArgument().isSet())
    {
        return TestUrlArguments(search.value()).applyPaging(std::move(result));
    }

    return result;
}

std::vector<db_model::ChargeItem>
MockTaskTable::retrieveAllChargeItemsForInsurant(const db_model::HashedKvnr& requestingInsurant,
                                                 const std::optional<UrlArguments>& search,
                                                 const bool applyPaging) const
{
    std::vector<db_model::ChargeItem> result;
    auto testArgs = search ? std::make_optional<TestUrlArguments>(*search) : std::nullopt;
    for (const auto& [id, row] : mTasks)
    {
        if (!row.chargeItem.has_value() || requestingInsurant != row.kvnrHashed)
        {
            continue;
        }
        if (! testArgs || (testArgs->matches("enterer", row.chargeItemEnterer) &&
                           testArgs->matches("entered-date", row.chargeItemEnteredDate)))
        {
            auto prescriptionId = model::PrescriptionId::fromDatabaseId(mPrescriptionType, id);
            // TODO return correct blob id / salt
            result.emplace_back( db_model::ChargeItem(prescriptionId, row.taskKeyBlobId.value(),
                                                             row.salt.value(), row.authoredOn,
                                                             row.chargeItem.value()));
        }
    }

    if(applyPaging && search.has_value() && search->pagingArgument().isSet())
    {
        return TestUrlArguments(search.value()).applyPaging(std::move(result));
    }

    return result;
}

std::tuple<db_model::ChargeItem, db_model::EncryptedBlob>
MockTaskTable::retrieveChargeInformation(const model::PrescriptionId& id) const
{
    const auto& rowIt = mTasks.find(id.toDatabaseId());
    ErpExpect(rowIt != mTasks.end(), HttpStatus::NotFound, "No such task: " + id.toString());
    const auto& row = rowIt->second;
    ErpExpect(row.chargeItem.has_value(), HttpStatus::NotFound, "No charge item exists for task " + id.toString());
    db_model::ChargeItem dbChargeItem(id, row.taskKeyBlobId.value(), row.salt.value(), row.authoredOn,
                                       row.chargeItem.value());
    return std::make_tuple(dbChargeItem, row.dispenseItem.value());
}

std::tuple<db_model::ChargeItem, db_model::EncryptedBlob>
MockTaskTable::retrieveChargeInformationForUpdate(const model::PrescriptionId& id) const
{
    return retrieveChargeInformation(id);
}

void MockTaskTable::deleteChargeInformation(const model::PrescriptionId& id)
{
    const auto& rowIt = mTasks.find(id.toDatabaseId());
    Expect(rowIt != mTasks.end(), "No such task: " + id.toString());
    clearChargeInformation(rowIt->second);
}

uint64_t MockTaskTable::countChargeInformationForInsurant(const db_model::HashedKvnr& kvnr,
                                                          const std::optional<UrlArguments>& search) const
{
    const auto chargeItems = retrieveAllChargeItemsForInsurant(kvnr, search, false);
    return chargeItems.size();
}

uint64_t MockTaskTable::countChargeInformationForPharmacy(const db_model::HashedTelematikId& requestingPharmacy,
                                                          const std::optional<UrlArguments>& search) const
{
    const auto chargeItems = retrieveAllChargeItemsForPharmacy(requestingPharmacy, search, false);
    return chargeItems.size();
}

void MockTaskTable::clearAllChargeInformation(const db_model::HashedKvnr& insurant)
{
    for (auto& [id, row] : mTasks)
    {
        if (row.kvnrHashed == insurant)
        {
            clearChargeInformation(row);
        }
    }
}

bool MockTaskTable::isBlobUsed(BlobId blobId) const
{
    auto hasBlobId = [blobId](const auto& row) {
        return row.second.taskKeyBlobId == blobId || row.second.medicationDispenseKeyBlobId == blobId;
    };
    auto blobUser = find_if(mTasks.begin(), mTasks.end(), hasBlobId);
    if (blobUser != mTasks.end())
    {
        auto prescriptionId = model::PrescriptionId::fromDatabaseId(
            mPrescriptionType, blobUser->first);
        TVLOG(0) << "Blob " << blobId << R"( is still in use by an "task":)" << prescriptionId.toString();
        return true;
    }
    return false;
}

std::optional<db_model::Task> MockTaskTable::select(int64_t databaseId,
                                                   const std::set<MockTaskTable::FieldName>& fields) const
{
    Expect3(fields.count(authored_on), "mandatory field authored_on not selected.", std::logic_error);
    Expect3(fields.count(last_modified), "mandatory field last_modified not selected.", std::logic_error);
    Expect3(fields.count(status), "mandatory field status not selected.", std::logic_error);
    Expect3(fields.count(task_key_blob_id), "mandatory field key_generation_id not selected.", std::logic_error);
    Expect3(fields.count(salt), "mandatory field salt not selected.", std::logic_error);
    auto taskRow = mTasks.find(databaseId);
    if (taskRow == mTasks.end())
    {
        return std::nullopt;
    }
    const auto& t = taskRow->second;
    auto taskId = model::PrescriptionId::fromDatabaseId(mPrescriptionType, databaseId);
    const auto& salt = t.salt.has_value()?*t.salt:db_model::Blob{};
    auto dbTask = std::make_optional<db_model::Task>(
            taskId,
            t.taskKeyBlobId.value(),
            salt,
            t.status.value(),
            t.authoredOn,
            t.lastModified);
    Expect3(not fields.count(kvnr_hashed), "db_model::Task has no field for kvnr_hashed", std::logic_error);
    Expect3(not fields.count(performer), "db_model::Task has no field for performer", std::logic_error);
    Expect3(not fields.count(when_handed_over), "db_model::Task has no field for when_handed_over", std::logic_error);
    Expect3(not fields.count(when_prepared), "db_model::Task has no field for when_prepared", std::logic_error);
    Expect3(not fields.count(medication_dispense_bundle),
            "db_model::Task has no field for medication_dispense_bundle", std::logic_error);
    dbTask->kvnr = fields.count(kvnr)?t.kvnr:std::nullopt;
    dbTask->expiryDate = fields.count(expiry_date)?t.expiryDate:std::nullopt;
    dbTask->acceptDate = fields.count(accept_date)?t.acceptDate:std::nullopt;
    dbTask->blobId = fields.count(task_key_blob_id)?t.taskKeyBlobId.value():-1;
    dbTask->accessCode = fields.count(access_code)?t.accessCode:std::nullopt;
    dbTask->secret = fields.count(secret)?t.secret:std::nullopt;
    dbTask->healthcareProviderPrescription =
            fields.count(healthcare_provider_prescription)?t.healthcareProviderPrescription:std::nullopt;
    dbTask->receipt = fields.count(receipt)?t.receipt:std::nullopt;
    return dbTask;
}

void MockTaskTable::clearChargeInformation(MockTaskTable::Row& row)
{
    row.chargeItem.reset();
    row.dispenseItem.reset();
    row.chargeItemEnteredDate.reset();
    row.chargeItemEnterer.reset();
}
