/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/mock/MockTaskTable.hxx"

#include "fhirtools/util/Gsl.hxx"
#include "shared/util/TLog.hxx"
#include "test/mock/MockAccountTable.hxx"
#include "test/mock/TestUrlArguments.hxx"

#include <date/tz.h>

MockTaskTable::Row::Row(model::Timestamp initAuthoredOn, model::Timestamp initLastModified,
                        model::Timestamp initLastStatusUpdate)
    : authoredOn(initAuthoredOn)
    , lastModified(initLastModified)
    , lastStatusUpdate(initLastStatusUpdate)
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
            access_code, secret, last_medication_dispense});
}

std::vector<db_model::Task> MockTaskTable::retrieveAllTasksForPatient(const db_model::HashedKvnr& kvnrHashed,
                                                                      const std::optional<UrlArguments>& search,
                                                                      const bool onlyFlowtype160,
                                                                      bool applyOnlySearch, bool withAccessCode) const
{
    static std::set<FieldName> selectedFields = {prescription_id, kvnr,   last_modified, last_medication_dispense, authored_on,     expiry_date,
                                                 accept_date,     status, salt,          task_key_blob_id, last_medication_dispense};
    if (withAccessCode)
    {
        selectedFields.emplace(access_code);
    }
    std::vector<db_model::Task> allTasks;
    for (const auto& task : mTasks)
    {
        if (task.second.kvnrHashed == kvnrHashed
        && (!onlyFlowtype160 || mPrescriptionType == ::model::PrescriptionType::apothekenpflichigeArzneimittel) )
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
    auto allTasks = retrieveAllTasksForPatient(kvnr, search, false/*onlyFlowtype160*/, true/*applyOnlySearch*/, false);
    return allTasks.size();
}

uint64_t MockTaskTable::countAll160Tasks(const db_model::HashedKvnr& kvnr,
                                                const std::optional<UrlArguments>& search) const
{
    auto allTasks = retrieveAllTasksForPatient(kvnr, search, true/*onlyFlowtype160*/, true/*applyOnlySearch*/, false);
    return allTasks.size();
}

std::tuple<model::PrescriptionId, model::Timestamp> MockTaskTable::createTask(model::PrescriptionType prescriptionType,
                                                                              model::Task::Status taskStatus,
                                                                              const model::Timestamp& lastUpdated,
                                                                              const model::Timestamp& created,
                                                                              const model::Timestamp& lastStatusUpdate)
{
    ++mTaskId;
    auto id = model::PrescriptionId::fromDatabaseId(prescriptionType, mTaskId);
    auto& row = createRow(id, lastUpdated, created, lastStatusUpdate);
    row.status = taskStatus;
    TVLOG(1) << "createTask: " + id.toString();
    return std::make_tuple(id, created);
}

MockTaskTable::Row& MockTaskTable::createRow(const model::PrescriptionId& taskId, const model::Timestamp& lastUpdated,
                                             const model::Timestamp& created, const model::Timestamp& lastStatusUpdate)
{
    auto [newTask, inserted] = mTasks.try_emplace(taskId.toDatabaseId(), created, lastUpdated, lastStatusUpdate);
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
                                const db_model::EncryptedBlob& healthCareProviderPrescription,
                                const model::Timestamp& lastStatusUpdate)
{
    auto taskRowIt = mTasks.find(taskId.toDatabaseId());
    Expect(taskRowIt != mTasks.end(), "no such task:" + taskId.toString());
    auto& taskRow = taskRowIt->second;
    taskRow.kvnr = encryptedKvnr;
    taskRow.kvnrHashed = hashedKvnr;
    taskRow.status = taskStatus;
    taskRow.lastStatusUpdate = lastStatusUpdate;
    taskRow.lastModified = lastModified;
    taskRow.expiryDate.emplace(expiryDate.localDay(model::Timestamp::GermanTimezone));
    taskRow.acceptDate.emplace(acceptDate.localDay(model::Timestamp::GermanTimezone));
    taskRow.healthcareProviderPrescription = healthCareProviderPrescription;
}

void MockTaskTable::updateTaskReceipt(const model::PrescriptionId& taskId, const model::Task::Status& taskStatus,
                                      const model::Timestamp& lastModified, const db_model::EncryptedBlob& receipt,
                                      const model::Timestamp& lastStatusUpdate)
{
    auto taskRowIt = mTasks.find(taskId.toDatabaseId());
    Expect(taskRowIt != mTasks.end(), "no such task:" + taskId.toString());
    auto& taskRow = taskRowIt->second;
    taskRow.status = taskStatus;
    taskRow.lastStatusUpdate = lastStatusUpdate;
    taskRow.lastModified = lastModified;
    taskRow.receipt = receipt;
}

void MockTaskTable::updateTask(const model::PrescriptionId& taskId,
                              const db_model::EncryptedBlob& accessCode,
                              BlobId blobId,
                              const db_model::Blob& salt)
{
    auto taskRowIt = mTasks.find(taskId.toDatabaseId());
    Expect(taskRowIt != mTasks.end(), "no such task:" + taskId.toString());
    auto& taskRow = taskRowIt->second;
    taskRow.accessCode = accessCode;
    taskRow.taskKeyBlobId = blobId;
    taskRow.salt = salt;
}

void MockTaskTable::updateTaskStatusAndSecret(const model::PrescriptionId& taskId, model::Task::Status taskStatus,
                                              const model::Timestamp& lastModifiedDate,
                                              const std::optional<db_model::EncryptedBlob>& taskSecret,
                                              const std::optional<db_model::EncryptedBlob>& owner,
                                              const model::Timestamp& lastStatusUpdate)
{
    auto taskRowIt = mTasks.find(taskId.toDatabaseId());
    Expect(taskRowIt != mTasks.end(), "no such task:" + taskId.toString());
    auto& taskRow = taskRowIt->second;
    taskRow.status = taskStatus;
    taskRow.lastStatusUpdate = lastStatusUpdate;
    taskRow.lastModified = lastModifiedDate;
    taskRow.secret = taskSecret;
    taskRow.owner = owner;
}

void MockTaskTable::updateTaskMedicationDispense(const model::PrescriptionId& taskId,
                                                 const model::Timestamp& lastModified,
                                                 const model::Timestamp& lastMedicationDispense,
                                                 const db_model::EncryptedBlob& medicationDispense,
                                                 BlobId medicationDispenseBlobId,
                                                 const db_model::HashedTelematikId& telematikId,
                                                 const model::Timestamp& whenHandedOver,
                                                 const std::optional<model::Timestamp>& whenPrepared,
                                                 const db_model::Blob& medicationDispenseSalt)
{
    auto taskRowIt = mTasks.find(taskId.toDatabaseId());
    Expect(taskRowIt != mTasks.end(), "no such task:" + taskId.toString());
    auto& taskRow = taskRowIt->second;
    taskRow.lastModified = lastModified;
    taskRow.lastMedicationDispense = lastMedicationDispense;
    taskRow.medicationDispenseBundle = medicationDispense;
    taskRow.medicationDispenseKeyBlobId = medicationDispenseBlobId;
    taskRow.performer = telematikId;
    taskRow.whenHandedOver = whenHandedOver;
    taskRow.whenPrepared = whenPrepared;
    taskRow.medicationDispenseSalt = medicationDispenseSalt;
}

void MockTaskTable::updateTaskMedicationDispenseReceipt(const model::PrescriptionId& taskId,
                                                       const model::Task::Status& taskStatus,
                                                       const model::Timestamp& lastModified,
                                                       const db_model::EncryptedBlob& medicationDispense,
                                                       BlobId medicationDispenseBlobId,
                                                       const db_model::HashedTelematikId& telematicId,
                                                       const model::Timestamp& whenHandedOver,
                                                       const std::optional<model::Timestamp>& whenPrepared,
                                                       const db_model::EncryptedBlob& taskReceipt,
                                                       const model::Timestamp& lastMedicationDispense,
                                                       const db_model::Blob& medicationDispenseSalt,
                                                       const model::Timestamp& lastStatusUpdate)
{
    auto taskRowIt = mTasks.find(taskId.toDatabaseId());
    Expect(taskRowIt != mTasks.end(), "no such task:" + taskId.toString());
    auto& taskRow = taskRowIt->second;
    taskRow.status = taskStatus;
    taskRow.lastStatusUpdate = lastStatusUpdate;
    taskRow.lastModified = lastModified;
    taskRow.medicationDispenseBundle = medicationDispense;
    taskRow.medicationDispenseKeyBlobId = medicationDispenseBlobId;
    taskRow.performer = telematicId;
    taskRow.whenHandedOver = whenHandedOver;
    taskRow.whenPrepared = whenPrepared;
    taskRow.receipt = taskReceipt;
    taskRow.lastMedicationDispense = lastMedicationDispense;
    taskRow.medicationDispenseSalt = medicationDispenseSalt;
}
void MockTaskTable::updateTaskDeleteMedicationDispense(const model::PrescriptionId& taskId, const model::Timestamp& lastModified)
{
    auto taskRowIt = mTasks.find(taskId.toDatabaseId());
    Expect(taskRowIt != mTasks.end(), "no such task:" + taskId.toString());
    auto& taskRow = taskRowIt->second;
    taskRow.lastMedicationDispense.reset();
    taskRow.medicationDispenseKeyBlobId.reset();
    taskRow.medicationDispenseBundle.reset();
    taskRow.lastModified = lastModified;
}

void MockTaskTable::updateTaskClearPersonalData(const model::PrescriptionId& taskId, model::Task::Status taskStatus,
                                                const model::Timestamp& lastModified,
                                                const model::Timestamp& lastStatusUpdate)
{
    auto taskRowIt = mTasks.find(taskId.toDatabaseId());
    Expect(taskRowIt != mTasks.end(), "no such task:" + taskId.toString());
    auto& taskRow = taskRowIt->second;
    taskRow.status = taskStatus;
    taskRow.lastStatusUpdate = lastStatusUpdate;
    taskRow.lastModified = lastModified;
    taskRow.kvnr.reset();
    taskRow.salt.reset();
    taskRow.accessCode.reset();
    taskRow.secret.reset();
    taskRow.owner.reset();
    taskRow.healthcareProviderPrescription.reset();
    taskRow.receipt.reset();
    taskRow.whenHandedOver.reset();
    taskRow.whenPrepared.reset();
    taskRow.performer.reset();
    taskRow.medicationDispenseBundle.reset();
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
            secret, owner, receipt, last_medication_dispense});
}

::std::optional<::db_model::Task>
MockTaskTable::retrieveTaskForUpdateAndPrescription(const model::PrescriptionId& taskId)
{
    return select(taskId.toDatabaseId(),
                  {prescription_id, kvnr, last_modified, authored_on, expiry_date, accept_date, status, salt,
                   task_key_blob_id, access_code, secret, owner, healthcare_provider_prescription, last_medication_dispense});
}


std::optional<db_model::Task> MockTaskTable::retrieveTaskAndPrescription(const model::PrescriptionId& taskId)
{
    return select(taskId.toDatabaseId(),
                  {prescription_id, kvnr, last_modified, authored_on, expiry_date, accept_date, status, salt,
                   task_key_blob_id, access_code, owner, healthcare_provider_prescription, last_medication_dispense});
}

std::optional<db_model::Task> MockTaskTable::retrieveTaskWithSecretAndPrescription(const model::PrescriptionId& taskId)
{
    return select(taskId.toDatabaseId(),
                  {prescription_id, kvnr, last_modified, authored_on, expiry_date, accept_date, status, salt,
                   task_key_blob_id, access_code, secret, owner, healthcare_provider_prescription, last_medication_dispense});
}

std::optional<db_model::Task> MockTaskTable::retrieveTaskAndPrescriptionAndReceipt(const model::PrescriptionId& taskId)
{
    return select(taskId.toDatabaseId(),
                  {prescription_id, kvnr, last_modified, authored_on, expiry_date, accept_date, status, salt,
                   task_key_blob_id, access_code, secret, owner, healthcare_provider_prescription, receipt, last_medication_dispense});
}


void MockTaskTable::deleteTask (const model::PrescriptionId& taskId)
{
    Expect(mTasks.erase(taskId.toDatabaseId()), "no such task: " + taskId.toString());
}

void MockTaskTable::deleteChargeItemSupportingInformation(const ::model::PrescriptionId& taskId)
{
    switch (mPrescriptionType)
    {
        case ::model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
            [[fallthrough]];
        case ::model::PrescriptionType::direkteZuweisungPkv:
        {
            auto task = mTasks.find(taskId.toDatabaseId());
            Expect(task != mTasks.end(), "no such task:" + taskId.toString());

            task->second.healthcareProviderPrescription = ::std::nullopt;
            task->second.medicationDispenseBundle = ::std::nullopt;
            task->second.medicationDispenseKeyBlobId = ::std::nullopt;
            task->second.receipt = ::std::nullopt;
            break;
        }
        case ::model::PrescriptionType::apothekenpflichigeArzneimittel:
        case ::model::PrescriptionType::direkteZuweisung:
        case ::model::PrescriptionType::digitaleGesundheitsanwendungen:
            break;
    }
}

void MockTaskTable::clearAllChargeItemSupportingInformation(const ::db_model::HashedKvnr& kvnrHashed)
{
    switch (mPrescriptionType)
    {
        case ::model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
            [[fallthrough]];
        case ::model::PrescriptionType::direkteZuweisungPkv:
        {
            for (auto& task : mTasks)
            {
                if (task.second.kvnrHashed == kvnrHashed)
                {
                    task.second.healthcareProviderPrescription = ::std::nullopt;
                    task.second.medicationDispenseBundle = ::std::nullopt;
                    task.second.medicationDispenseKeyBlobId = ::std::nullopt;
                    task.second.receipt = ::std::nullopt;
                }
            }
            break;
        }
        case ::model::PrescriptionType::apothekenpflichigeArzneimittel:
        case ::model::PrescriptionType::direkteZuweisung:
        case ::model::PrescriptionType::digitaleGesundheitsanwendungen:
            break;
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

std::optional<db_model::Blob> MockTaskTable::retrieveMedicationDispenseSalt(const model::PrescriptionId& taskId)
{
    auto taskRow = mTasks.find(taskId.toDatabaseId());
    if (taskRow == mTasks.end())
    {
        return std::nullopt;
    }
    return taskRow->second.medicationDispenseSalt;
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
            t.lastStatusUpdate,
            t.authoredOn,
            t.lastModified);
    Expect3(not fields.count(kvnr_hashed), "db_model::Task has no field for kvnr_hashed", std::logic_error);
    Expect3(not fields.count(performer), "db_model::Task has no field for performer", std::logic_error);
    Expect3(not fields.count(when_handed_over), "db_model::Task has no field for when_handed_over", std::logic_error);
    Expect3(not fields.count(when_prepared), "db_model::Task has no field for when_prepared", std::logic_error);
    Expect3(not fields.count(medication_dispense_bundle),
            "db_model::Task has no field for medication_dispense_bundle", std::logic_error);
    dbTask->kvnr = fields.count(kvnr)?t.kvnr:std::nullopt;
    dbTask->expiryDate = fields.count(expiry_date) && t.expiryDate
                             ?std::make_optional(model::Timestamp{model::Timestamp::GermanTimezone, date::local_days{*t.expiryDate}})
                             :std::nullopt;
    dbTask->acceptDate = fields.count(accept_date) && t.acceptDate
                             ?std::make_optional(model::Timestamp{model::Timestamp::GermanTimezone, date::local_days{*t.acceptDate}})
                             :std::nullopt;
    if (fields.count(task_key_blob_id))
    {
        dbTask->blobId = t.taskKeyBlobId.value();
    }
    dbTask->accessCode = fields.count(access_code)?t.accessCode:std::nullopt;
    dbTask->secret = fields.count(secret)?t.secret:std::nullopt;
    dbTask->owner = fields.count(owner)?t.owner:std::nullopt;
    dbTask->healthcareProviderPrescription =
            fields.count(healthcare_provider_prescription)?t.healthcareProviderPrescription:std::nullopt;
    dbTask->receipt = fields.count(receipt)?t.receipt:std::nullopt;
    dbTask->lastMedicationDispense = fields.count(last_medication_dispense)?t.lastMedicationDispense:std::nullopt;
    return dbTask;
}
