/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/DatabaseFrontend.hxx"

#include <boost/algorithm/string.hpp>

#include "erp/ErpRequirements.hxx"
#include "erp/compression/ZStd.hxx"
#include "erp/crypto/CMAC.hxx"
#include "erp/database/DatabaseBackend.hxx"
#include "erp/database/DatabaseConnectionInfo.hxx"
#include "erp/database/DatabaseModel.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/model/AuditData.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Communication.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/MedicationDispenseId.hxx"
#include "erp/model/MedicationDispenseBundle.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/Identity.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/search/UrlArguments.hxx"

using namespace model;
using namespace ::std::literals;

DatabaseFrontend::DatabaseFrontend(std::unique_ptr<DatabaseBackend>&& backend,
                                   HsmPool& hsmPool,
                                   KeyDerivation& keyDerivation)
    : mBackend{std::move(backend)}
    , mHsmPool{hsmPool}
    , mDerivation{keyDerivation}
    , mCodec{compressionInstance()}
{
}


void DatabaseFrontend::commitTransaction()
{
    mBackend->commitTransaction();
}

void DatabaseFrontend::closeConnection()
{
    mBackend->closeConnection();
}

std::string DatabaseFrontend::retrieveSchemaVersion()
{
    return mBackend->retrieveSchemaVersion();
}

void DatabaseFrontend::healthCheck()
{
    mBackend->healthCheck();
}

std::optional<DatabaseConnectionInfo> DatabaseFrontend::getConnectionInfo() const
{
    return mBackend->getConnectionInfo();
}

std::tuple<std::vector<model::MedicationDispense>, bool>
DatabaseFrontend::retrieveAllMedicationDispenses(const model::Kvnr& kvnr,
                                                 const std::optional<UrlArguments>& search)
{
    auto hashedKvnr = mDerivation.hashKvnr(kvnr);

    const auto [encryptedResult, hasNextPage] =
        mBackend->retrieveAllMedicationDispenses(hashedKvnr, {}, search);

    std::vector<model::MedicationDispense> resultSet;
    resultSet.reserve(gsl::narrow<size_t>(encryptedResult.size()));

    std::map<BlobId, SafeString> keys;

    for (const auto& res : encryptedResult)
    {
        auto key = keys.find(res.blobId);
        if (key == keys.end())
        {
            auto salt = mBackend->retrieveSaltForAccount(hashedKvnr,
                                                         db_model::MasterKeyType::medicationDispense,
                                                         res.blobId);
            Expect(salt, "Salt not found in database.");
            std::tie(key, std::ignore) =
                    keys.emplace(res.blobId, mDerivation.medicationDispenseKey(hashedKvnr, res.blobId, *salt));
        }
        auto unspecified = model::UnspecifiedResource::fromJsonNoValidation(mCodec.decode(res.medicationDispense, key->second));
        if (unspecified.getResourceType() == model::MedicationDispense::resourceTypeName)
        {
            resultSet.emplace_back(model::MedicationDispense::fromJson(unspecified.jsonDocument()));
        }
        else if (unspecified.getResourceType() == model::Bundle::resourceTypeName)
        {
            auto bundle = model::MedicationDispenseBundle::fromJson(unspecified.jsonDocument());
            auto medications = bundle.getResourcesByType<model::MedicationDispense>();
            for (auto&& medication : medications)
            {
                resultSet.emplace_back(std::move(medication));
            }
        }
        else
        {
            Fail2("unable to detect resource type of stored medication dispense", std::logic_error);
        }
    }
    return {std::move(resultSet), hasNextPage};
}

std::optional<MedicationDispense> DatabaseFrontend::retrieveMedicationDispense(const model::Kvnr& kvnr,
                                                                               const model::MedicationDispenseId& id)
{
    auto hashedKvnr = mDerivation.hashKvnr(kvnr);

    const auto encryptedResultTuple =
        mBackend->retrieveAllMedicationDispenses(hashedKvnr, id.getPrescriptionId(), {});
    const auto& encryptedResult = std::get<std::vector<db_model::MedicationDispense>>(encryptedResultTuple);

    Expect(encryptedResult.size() <= 1,
           "invalid number of results of Medication Dispenses: " + std::to_string(encryptedResult.size()));

    if(encryptedResult.empty())
    {
        return {};
    }

    const auto& res = encryptedResult[0];

    auto salt = mBackend->retrieveSaltForAccount(hashedKvnr,
                                                 db_model::MasterKeyType::medicationDispense,
                                                 res.blobId);
    Expect(salt, "Salt not found in database.");
    auto key = mDerivation.medicationDispenseKey(hashedKvnr, res.blobId, *salt);

    auto unspecified = model::UnspecifiedResource::fromJsonNoValidation(mCodec.decode(res.medicationDispense, key));
    if (unspecified.getResourceType() == model::MedicationDispense::resourceTypeName)
    {
        auto medication = model::MedicationDispense::fromJson(unspecified.jsonDocument());
        if (medication.id() == id)
        {
            return medication;
        }
        return {};
    }
    else if (unspecified.getResourceType() == model::Bundle::resourceTypeName)
    {
        auto bundle = model::Bundle::fromJson(unspecified.jsonDocument());
        auto medications = bundle.getResourcesByType<model::MedicationDispense>();
        for (auto&& medication : medications)
        {
            if (medication.id() == id)
            {
                return std::move(medication);
            }
        }
        return {}; // not found
    }
    else
    {
        Fail2("unable to detect resource type of stored medication dispense", std::logic_error);
    }
}

CmacKey DatabaseFrontend::acquireCmac(const date::sys_days& validDate, const CmacKeyCategory& cmacType, RandomSource& randomSource)
{
    return mBackend->acquireCmac(validDate, cmacType, randomSource);
}

PrescriptionId DatabaseFrontend::storeTask(const Task& task)
{
    auto [prescriptionId, authoredOn] =
            mBackend->createTask(task.type(), task.status(), task.lastModifiedDate(), task.authoredOn());

    A_19700.start("Initial key derivation for Task.");
    // use authoredOn returned from createTask to ensure we have identical rounding (see ERP-5602)
    auto [key, derivationData] = mDerivation.initialTaskKey(prescriptionId, authoredOn);
    db_model::Blob salt{derivationData.salt};
    A_19700.finish();

    A_19688.start("encrypt access code");
    // TODO: do not compress accesscode
    const auto accessCode = mCodec.encode(task.accessCode(), key, Compression::DictionaryUse::Default_json);
    A_19688.finish();

    mBackend->updateTask(prescriptionId, accessCode, derivationData.blobId, salt);
    return prescriptionId;
}

void DatabaseFrontend::updateTaskStatusAndSecret(const Task& task)
{
    updateTaskStatusAndSecret(task, taskKey(task.prescriptionId()));
}

void DatabaseFrontend::updateTaskStatusAndSecret(const Task& task, const SafeString& key)
{
    A_19688.start("encrypt secret");
    std::optional<db_model::EncryptedBlob> secret;
    if (task.secret().has_value())
    {
        // TODO: do not compress secret
        secret = mCodec.encode(task.secret().value(), key, Compression::DictionaryUse::Default_json);
    }
    A_19688.finish();

    // GEMREQ-start A_19688#encrypt-telematikid
    A_19688.start("encrypt telematik-id as task owner");
    std::optional<db_model::EncryptedBlob> owner;
    if (task.owner().has_value())
    {
        owner = mCodec.encode(task.owner().value(), key, Compression::DictionaryUse::Default_json);
    }
    A_19688.finish();
    // GEMREQ-end A_19688#encrypt-telematikid
    // GEMREQ-start A_24174#call-backend
    mBackend->updateTaskStatusAndSecret(task.prescriptionId(), task.status(), task.lastModifiedDate(), secret, owner);
    // GEMREQ-end A_24174#call-backend
}

void DatabaseFrontend::activateTask(const model::Task& task, const Binary& healthCareProviderPrescription)
{
    activateTask(task, taskKey(task.prescriptionId()), healthCareProviderPrescription);
}

void DatabaseFrontend::activateTask(const model::Task& task, const SafeString& key,
                                    const Binary& healthCareProviderPrescription)
{
    A_19688.start("encrypt kvnr and prescription.");
    const auto encryptedPrescription = mCodec.encode(healthCareProviderPrescription.serializeToJsonString(), key,
                                                     Compression::DictionaryUse::Default_json);
    const auto& kvnr = task.kvnr();
    Expect(kvnr.has_value(), "KVNR not set in task during activate");
    auto encrypedKvnr = mCodec.encode(kvnr->id(), key, Compression::DictionaryUse::Default_json);

    const auto hashedKvnr = mDerivation.hashKvnr(*kvnr);
    A_19688.finish();

    mBackend->activateTask(task.prescriptionId(), encrypedKvnr, hashedKvnr, task.status(), task.lastModifiedDate(),
                           task.expiryDate(), task.acceptDate(), encryptedPrescription);
}

void DatabaseFrontend::updateTaskMedicationDispense(
    const model::Task& task, const std::vector<model::MedicationDispense>& medicationDispenses)
{
    ErpExpect(! medicationDispenses.empty(), HttpStatus::InternalServerError,
              "medication dispense bundle cannot be empty at this place");
    const auto& medicationDispense = medicationDispenses[0];
    const auto telematikId = medicationDispense.telematikId();
    const auto whenHandedOver = medicationDispense.whenHandedOver();
    const auto whenPrepared = medicationDispense.whenPrepared();
    const auto kvnr = task.kvnr();
    ErpExpect(kvnr.has_value(), HttpStatus::InternalServerError,
              "Cannot update medication dispense for task without kvnr.");
    const auto hashedKvnr = mDerivation.hashKvnr(*kvnr);
    const auto hashedTelematikId = mDerivation.hashTelematikId(telematikId);

    /// MedicationDispense uses same derivation master key as task:
    auto [keyForMedicationDispense, blobId] = medicationDispenseKey(hashedKvnr);
    auto encryptedMedicationDispense = encryptMedicationDispense(medicationDispenses, keyForMedicationDispense);
    mBackend->updateTaskMedicationDispense(task.prescriptionId(),
                                           task.lastModifiedDate(),
                                           value(task.lastMedicationDispense()),
                                           encryptedMedicationDispense,
                                           blobId,
                                           hashedTelematikId,
                                           whenHandedOver,
                                           whenPrepared);
}

void DatabaseFrontend::updateTaskMedicationDispenseReceipt(
    const model::Task& task, const std::vector<model::MedicationDispense>& medicationDispenses,
    const model::ErxReceipt& receipt)
{
    updateTaskMedicationDispenseReceipt(task, taskKey(task.prescriptionId()), medicationDispenses, receipt);
}

void DatabaseFrontend::updateTaskMedicationDispenseReceipt(
    const model::Task& task, const SafeString& key, const std::vector<model::MedicationDispense>& medicationDispenses,
    const model::ErxReceipt& receipt)
{
    A_19688.start("encrypt medication dispense and receipt");
    auto encryptReceipt = mCodec.encode(receipt.serializeToJsonString(), key, Compression::DictionaryUse::Default_json);
    A_19688.finish();
    ErpExpect(! medicationDispenses.empty(), HttpStatus::InternalServerError,
              "medication dispense bundle cannot be empty at this place");
    const auto& medicationDispense = medicationDispenses[0];
    const auto telematikId = medicationDispense.telematikId();
    const auto whenHandedOver = medicationDispense.whenHandedOver();
    const auto whenPrepared = medicationDispense.whenPrepared();
    const auto kvnr = task.kvnr();
    ErpExpect(kvnr.has_value(), HttpStatus::InternalServerError,
              "Cannot update medication dispense for task without kvnr.");
    const auto hashedKvnr = mDerivation.hashKvnr(*kvnr);
    const auto hashedTelematikId = mDerivation.hashTelematikId(telematikId);

    /// MedicationDispense uses same derivation master key as task:
    auto [keyForMedicationDispense, blobId] = medicationDispenseKey(hashedKvnr);
    auto encryptedMedicationDispense = encryptMedicationDispense(medicationDispenses, keyForMedicationDispense);
    mBackend->updateTaskMedicationDispenseReceipt(task.prescriptionId(),
                                                  task.status(),
                                                  task.lastModifiedDate(),
                                                  encryptedMedicationDispense,
                                                  blobId,
                                                  hashedTelematikId,
                                                  whenHandedOver,
                                                  whenPrepared,
                                                  encryptReceipt,
                                                  value(task.lastMedicationDispense()));
}

db_model::EncryptedBlob
DatabaseFrontend::encryptMedicationDispense(const std::vector<model::MedicationDispense>& medicationDispenses,
                                            const SafeString& keyForMedicationDispense)
{
    model::Bundle medicationDispenseBundle{model::BundleType::collection, model::FhirResourceBase::NoProfile, Uuid()};
    for (const auto& medication : medicationDispenses)
    {
        medicationDispenseBundle.addResource({}, {}, {}, medication.jsonDocument());
    }

    return mCodec.encode(medicationDispenseBundle.serializeToJsonString(),
                         keyForMedicationDispense,
                         Compression::DictionaryUse::Default_json);
}

// GEMREQ-start A_24286#query-call-updateTaskDeleteMedicationDispense
void DatabaseFrontend::updateTaskDeleteMedicationDispense(const model::Task& task)
{
    mBackend->updateTaskDeleteMedicationDispense(task.prescriptionId(), task.lastModifiedDate());
}
// GEMREQ-end A_24286#query-call-updateTaskDeleteMedicationDispense

// GEMREQ-start A_19027-03#query-call-updateTaskClearPersonalData
void DatabaseFrontend::updateTaskClearPersonalData(const model::Task& task)
{
    mBackend->updateTaskClearPersonalData(task.prescriptionId(), task.status(), task.lastModifiedDate());
}
// GEMREQ-end A_19027-03#query-call-updateTaskClearPersonalData

std::string DatabaseFrontend::storeAuditEventData(model::AuditData& auditData)
{
    auto hashedKvnr = mDerivation.hashKvnr(auditData.insurantKvnr());

    std::optional<db_model::EncryptedBlob> encryptedMeta;
    std::optional<BlobId> auditEventBlobId;
    if (!auditData.metaData().isEmpty())
    {
        auto [keyForAuditData, blobId] = auditEventKey(hashedKvnr);
        encryptedMeta = mCodec.encode(auditData.metaData().serializeToJsonString(),
                                      keyForAuditData,
                                      Compression::DictionaryUse::Default_json);
        auditEventBlobId = blobId;
    }

    db_model::AuditData dbAuditData(auditData.agentType(), auditData.eventId(), std::move(encryptedMeta),
                                    auditData.action(), std::move(hashedKvnr), auditData.deviceId(),
                                    auditData.prescriptionId(), auditEventBlobId);

    auto id = mBackend->storeAuditEventData(dbAuditData);
    auditData.setId(id);
    auditData.setRecorded(dbAuditData.recorded);
    return id;
}

std::vector<model::AuditData>
DatabaseFrontend::retrieveAuditEventData(const model::Kvnr& kvnr, const std::optional<Uuid>& id,
                                         const std::optional<model::PrescriptionId>& prescriptionId,
                                         const std::optional<UrlArguments>& search)
{
    const auto dbAuditDataVec =
        mBackend->retrieveAuditEventData(mDerivation.hashKvnr(kvnr), id, prescriptionId, search);
    std::vector<model::AuditData> ret;
    ret.reserve(dbAuditDataVec.size());
    std::map<BlobId, SafeString> keys;
    for (const auto& item : dbAuditDataVec)
    {
        std::optional<std::string> consentId =
            item.eventId == model::AuditEventId::POST_Consent || item.eventId == model::AuditEventId::DELETE_Consent
                ? std::make_optional<std::string>(Consent::createIdString(model::Consent::Type::CHARGCONS, kvnr))
                : std::nullopt;
        if (item.metaData.has_value())
        {
            Expect(item.blobId, "Blob id must be set in database if meta data exist.");
            auto [key, needDerivation] = keys.try_emplace(*item.blobId, SafeString{});
            if (needDerivation)
            {
                auto salt = mBackend->retrieveSaltForAccount(item.insurantKvnr, db_model::MasterKeyType::auditevent,
                                                             *item.blobId);
                Expect(salt, "Salt not found in database.");
                key->second = mDerivation.auditEventKey(item.insurantKvnr, *item.blobId, *salt);
            }
            auto auditMetaData = AuditMetaData::fromJsonNoValidation(mCodec.decode(*item.metaData, key->second));

            ret.emplace_back(item.eventId, std::move(auditMetaData), item.action, item.agentType, model::Kvnr{kvnr},
                             item.deviceId, item.prescriptionId, consentId);
        }
        else
        {
            ret.emplace_back(item.eventId, AuditMetaData({}, {}), item.action, item.agentType, model::Kvnr{kvnr},
                             item.deviceId, item.prescriptionId, consentId);
        }
        ret.back().setId(item.id);
        ret.back().setRecorded(item.recorded);
    }

    return ret;
}


std::optional<DatabaseFrontend::TaskAndKey> DatabaseFrontend::retrieveTaskForUpdate(const PrescriptionId& taskId)
{
    const auto& dbTask = mBackend->retrieveTaskForUpdate(taskId);
    if (! dbTask)
    {
        return std::nullopt;
    }
    auto key = taskKey(*dbTask);
    auto task = getModelTask(*dbTask, key);
    return TaskAndKey{.task = std::move(task), .key = std::move(key)};
}

std::tuple<std::optional<Database::TaskAndKey>, std::optional<model::Binary>>
DatabaseFrontend::retrieveTaskForUpdateAndPrescription(const model::PrescriptionId& taskId)
{
    const auto& dbTask = mBackend->retrieveTaskForUpdateAndPrescription(taskId);
    if (! dbTask)
    {
        return {};
    }
    auto keyForTask = taskKey(*dbTask);
    auto task = getModelTask(*dbTask, keyForTask);
    auto taskAndKey = TaskAndKey{.task = std::move(task), .key = std::move(keyForTask)};
    std::optional<model::Binary> prescription;
    if (taskAndKey.key)
    {
        prescription = getHealthcareProviderPrescription(*dbTask, *taskAndKey.key);
    }
    return std::make_tuple(std::move(taskAndKey), std::move(prescription));
}

std::tuple<std::optional<Task>, std::optional<Bundle>>
DatabaseFrontend::retrieveTaskAndReceipt(const PrescriptionId& taskId)
{
    const auto& dbTask = mBackend->retrieveTaskAndReceipt(taskId);
    if (! dbTask)
    {
        return {};
    }
    auto keyForTask = taskKey(*dbTask);
    return std::make_tuple(getModelTask(*dbTask, keyForTask),
                           keyForTask ? getReceipt(*dbTask, *keyForTask) : std::nullopt);
}

std::tuple<std::optional<Database::TaskAndKey>, std::optional<Binary>>
DatabaseFrontend::retrieveTaskAndPrescription(const PrescriptionId& taskId)
{
    const auto& dbTask = mBackend->retrieveTaskAndPrescription(taskId);
    if (! dbTask)
    {
        return {};
    }
    auto keyForTask = taskKey(*dbTask);
    auto task = getModelTask(*dbTask, keyForTask);
    auto taskAndKey = TaskAndKey{.task = std::move(task), .key = std::move(keyForTask)};
    std::optional<model::Binary> prescription;
    if (taskAndKey.key)
    {
        prescription = getHealthcareProviderPrescription(*dbTask, *taskAndKey.key);
    }
    return std::make_tuple(std::move(taskAndKey), std::move(prescription));
}

std::tuple<std::optional<model::Task>, std::optional<model::Binary>>
DatabaseFrontend::retrieveTaskWithSecretAndPrescription(const model::PrescriptionId& taskId)
{
    const auto& dbTask = mBackend->retrieveTaskWithSecretAndPrescription(taskId);
    if (! dbTask)
    {
        return {};
    }
    auto keyForTask = taskKey(*dbTask);
    return std::make_tuple(getModelTask(*dbTask, keyForTask),
                           keyForTask ? getHealthcareProviderPrescription(*dbTask, *keyForTask) : std::nullopt);
}

std::tuple<std::optional<Task>, std::optional<Binary>, std::optional<Bundle>>
DatabaseFrontend::retrieveTaskAndPrescriptionAndReceipt(const PrescriptionId& taskId)
{
    const auto& dbTask = mBackend->retrieveTaskAndPrescriptionAndReceipt(taskId);
    if (! dbTask)
    {
        return {};
    }
    auto keyForTask = taskKey(*dbTask);
    return std::make_tuple(getModelTask(*dbTask, keyForTask),
                           keyForTask ? getHealthcareProviderPrescription(*dbTask, *keyForTask) : std::nullopt,
                           keyForTask ? getReceipt(*dbTask, *keyForTask) : std::nullopt);
}

std::vector<model::Task> DatabaseFrontend::retrieveAllTasksForPatient(const model::Kvnr& kvnr,
                                                                      const std::optional<UrlArguments>& search)
{
    ErpExpect(kvnr.validFormat(), HttpStatus::BadRequest, "Invalid KVNR");

    auto dbTaskList = mBackend->retrieveAllTasksForPatient(mDerivation.hashKvnr(kvnr), search);
    std::vector<model::Task> allTasks;
    for (const auto& dbTask : dbTaskList)
    {
        auto modelTask = getModelTask(dbTask);
        auto kvnrType = modelTask.prescriptionId().isPkv() ? model::Kvnr::Type::pkv : model::Kvnr::Type::gkv;
        modelTask.setKvnr(model::Kvnr{kvnr.id(), kvnrType});
        allTasks.emplace_back(std::move(modelTask));
    }
    return allTasks;
}

std::vector<model::Task> DatabaseFrontend::retrieveAll160TasksWithAccessCode(const model::Kvnr& kvnr,
                                                                             const std::optional<UrlArguments>& search)
{
    ErpExpect(kvnr.validFormat(), HttpStatus::BadRequest, "Invalid KVNR");

    auto dbTaskList = mBackend->retrieveAll160TasksWithAccessCode(mDerivation.hashKvnr(kvnr), search);
    std::vector<model::Task> allTasks;
    for (const auto& dbTask : dbTaskList)
    {
        auto keyForTask = taskKey(dbTask);
        auto modelTask = getModelTask(dbTask, keyForTask);
        allTasks.emplace_back(std::move(modelTask));
    }
    return allTasks;
}

uint64_t DatabaseFrontend::countAllTasksForPatient (const model::Kvnr& kvnr, const std::optional<UrlArguments>& search)
{
    return mBackend->countAllTasksForPatient(mDerivation.hashKvnr(kvnr), search);
}

std::optional<Uuid> DatabaseFrontend::insertCommunication(Communication& communication)
{
    const auto sender = communication.sender();
    const auto recipient = communication.recipient();
    const std::optional<model::Timestamp> timeSent = communication.timeSent();

    ErpExpect(sender.has_value(), HttpStatus::InternalServerError, "communication object has no 'sender' value");
    ErpExpect(timeSent.has_value(), HttpStatus::InternalServerError, "communication object has no 'sent' value");

    const auto& messagePlain = communication.serializeToJsonString();

    const std::string& senderIdentity = model::getIdentityString(sender.value());
    const std::string& recipientIdentity = model::getIdentityString(recipient);

    const model::PrescriptionId& prescriptionId = communication.prescriptionId();
    const model::Communication::MessageType messageType = communication.messageType();
    const db_model::HashedId senderHashed = mDerivation.hashIdentity(senderIdentity);
    const db_model::HashedId recipientHashed = mDerivation.hashIdentity(recipientIdentity);
    auto [senderKey, senderBlobId] = communicationKeyAndId(senderIdentity, senderHashed);
    auto [recipientKey, recipientBlobId] = communicationKeyAndId(recipientIdentity, recipientHashed);
    auto messageForSender = mCodec.encode(messagePlain, senderKey, Compression::DictionaryUse::Default_json);
    auto messageForRecipient = mCodec.encode(messagePlain, recipientKey, Compression::DictionaryUse::Default_json);
    auto uuid = mBackend->insertCommunication(prescriptionId, timeSent.value(), messageType, senderHashed, recipientHashed,
                                              senderBlobId, messageForSender, recipientBlobId,
                                              messageForRecipient);
    if (uuid)
    {
        communication.setId(*uuid);
    }
    return uuid;
}

uint64_t DatabaseFrontend::countRepresentativeCommunications(const model::Kvnr& insurantA, const model::Kvnr& insurantB,
                                                             const PrescriptionId& prescriptionId)
{
    return mBackend->countRepresentativeCommunications(mDerivation.hashKvnr(insurantA),
                                                       mDerivation.hashKvnr(insurantB), prescriptionId);
}

bool DatabaseFrontend::existCommunication(const Uuid& communicationId)
{
    return mBackend->existCommunication(communicationId);
}

std::vector<Communication> DatabaseFrontend::retrieveCommunications(const std::string& user,
                                                                    const std::optional<Uuid>& communicationId,
                                                                    const std::optional<UrlArguments>& search)
{
    auto hashedUser = mDerivation.hashIdentity(user);
    auto dbCommunications = mBackend->retrieveCommunications(hashedUser, communicationId, search);
    std::map<BlobId, SafeString> keys;
    std::vector<Communication> communications;
    communications.reserve(dbCommunications.size());
    for (const auto& item : dbCommunications)
    {
        auto [key, needDerivation] = keys.try_emplace(item.blobId, SafeString{});
        if (needDerivation)
        {
            key->second = mDerivation.communicationKey(user, hashedUser, item.blobId, item.salt);
        }
        auto communicationJson = mCodec.decode(item.communication, key->second);
        model::Communication communication = model::Communication::fromJsonNoValidation(communicationJson);
        // Please note that the communication id is not stored in the json string of the message
        // column as the id is only available after the data row has been inserted in the table.
        // To return a valid communication object the id will be set here.
        communication.setId(item.id);
        // Same applies to the time the communication has been received by the user.
        if (item.received)
        {
            communication.setTimeReceived(*item.received);
        }
        communications.emplace_back(std::move(communication));
    }
    return communications;
}

uint64_t DatabaseFrontend::countCommunications(const std::string& user,
                                               const std::optional<UrlArguments>& search)
{
    return mBackend->countCommunications(mDerivation.hashIdentity(user), search);
}


std::vector<Uuid> DatabaseFrontend::retrieveCommunicationIds(const std::string& recipient)
{
    return mBackend->retrieveCommunicationIds(mDerivation.hashIdentity(recipient));
}


std::tuple<std::optional<Uuid>, std::optional<model::Timestamp>>
DatabaseFrontend::deleteCommunication(const Uuid& communicationId, const std::string& sender)
{
    return mBackend->deleteCommunication(communicationId, mDerivation.hashIdentity(sender));
}


void DatabaseFrontend::markCommunicationsAsRetrieved(const std::vector<Uuid>& communicationIds,
                                                     const Timestamp& retrieved, const std::string& recipient)
{
    return mBackend->markCommunicationsAsRetrieved(communicationIds, retrieved, mDerivation.hashIdentity(recipient));
}

// GEMREQ-start A_19027-03#query-call-deleteCommunicationsForTask
void DatabaseFrontend::deleteCommunicationsForTask(const model::PrescriptionId& taskId)
{
    return mBackend->deleteCommunicationsForTask(taskId);
}
// GEMREQ-end A_19027-03#query-call-deleteCommunicationsForTask

void DatabaseFrontend::storeConsent(const Consent& consent)
{
    const auto& hashedKvnr = mDerivation.hashKvnr(consent.patientKvnr());
    return mBackend->storeConsent(hashedKvnr, Timestamp::now());
}

std::optional<model::Consent> DatabaseFrontend::retrieveConsent(const model::Kvnr& kvnr)
{
    const auto& hashedKvnr = mDerivation.hashKvnr(kvnr);
    auto dateTime = mBackend->retrieveConsentDateTime(hashedKvnr);
    if (dateTime.has_value())
    {
        return std::make_optional<model::Consent>(kvnr, *dateTime);
    }
    return std::nullopt;
}

// GEMREQ-start A_22158#query-call
bool DatabaseFrontend::clearConsent(const model::Kvnr& kvnr)
{
    const auto& hashedKvnr = mDerivation.hashKvnr(kvnr);
    return mBackend->clearConsent(hashedKvnr);
}
// GEMREQ-end A_22158#query-call

// GEMREQ-start storeChargeInformation
void DatabaseFrontend::storeChargeInformation(const ::model::ChargeInformation& chargeInformation)
{
    ErpExpect(chargeInformation.prescription.has_value(), ::HttpStatus::BadRequest, "No prescription data found.");
    ErpExpect(chargeInformation.unsignedPrescription.has_value(), ::HttpStatus::BadRequest,
              "No prescription data found.");
    ErpExpect(chargeInformation.receipt.has_value(), ::HttpStatus::BadRequest, "No receipt data found.");
    ErpExpect(chargeInformation.chargeItem.prescriptionId().has_value(), ::HttpStatus::BadRequest,
              "No prescription id found.");
    ErpExpect(chargeInformation.chargeItem.accessCode().has_value(), ::HttpStatus::BadRequest, "No access code found.");
    ErpExpect(chargeInformation.chargeItem.subjectKvnr().has_value(), ::HttpStatus::BadRequest, "No kvnr found.");
    ErpExpect(chargeInformation.chargeItem.entererTelematikId().has_value(), ::HttpStatus::BadRequest,
              "No enterer id found.");
    ErpExpect(chargeInformation.chargeItem.enteredDate().has_value(), ::HttpStatus::BadRequest,
              "No entered date found.");

    const auto encryptionData = chargeItemKey(chargeInformation.chargeItem.prescriptionId().value());
    const auto dbChargeItem = ::db_model::ChargeItem{chargeInformation, ::std::get<::BlobId>(encryptionData),
                                                     ::std::get<::db_model::Blob>(encryptionData),
                                                     ::std::get<::SafeString>(encryptionData), mCodec};
    const auto& hashedKvnr = mDerivation.hashKvnr(chargeInformation.chargeItem.subjectKvnr().value());
    mBackend->storeChargeInformation(dbChargeItem, hashedKvnr);
}
// GEMREQ-end storeChargeInformation

// GEMREQ-start updateChargeInformation
void DatabaseFrontend::updateChargeInformation(const ::model::ChargeInformation& chargeInformation, const BlobId& blobId, const db_model::Blob& salt)
{
    ErpExpect(chargeInformation.chargeItem.prescriptionId().has_value(), ::HttpStatus::BadRequest,
              "No prescription id found.");
    ErpExpect(chargeInformation.chargeItem.accessCode().has_value(), ::HttpStatus::BadRequest, "No access code found.");
    ErpExpect(chargeInformation.chargeItem.subjectKvnr().has_value(), ::HttpStatus::BadRequest, "No kvnr found.");
    ErpExpect(chargeInformation.chargeItem.entererTelematikId().has_value(), ::HttpStatus::BadRequest,
              "No enterer id found.");
    ErpExpect(chargeInformation.chargeItem.enteredDate().has_value(), ::HttpStatus::BadRequest,
              "No entered date found.");

    const auto encryptionData = chargeItemKey(chargeInformation.chargeItem.prescriptionId().value(), blobId, salt);
    mBackend->updateChargeInformation(::db_model::ChargeItem{chargeInformation, ::std::get<::BlobId>(encryptionData),
                                                             ::std::get<::db_model::Blob>(encryptionData),
                                                             ::std::get<::SafeString>(encryptionData), mCodec});
}
// GEMREQ-end updateChargeInformation

// GEMREQ-start A_22119#query-call
::std::vector<::model::ChargeItem>
DatabaseFrontend::retrieveAllChargeItemsForInsurant(const model::Kvnr& kvnr,
                                                    const std::optional<UrlArguments>& search) const
{
    const auto dbChargeItems = mBackend->retrieveAllChargeItemsForInsurant(mDerivation.hashKvnr(kvnr), search);

    ::std::vector<::model::ChargeItem> result;
    ::std::transform(::std::begin(dbChargeItems), ::std::end(dbChargeItems), ::std::back_inserter(result),
                     [&kvnr](const auto& item) {
                                 model::ChargeItem chargeItem = item.toChargeInformation(std::nullopt).chargeItem;
                                 if (! chargeItem.subjectKvnr().has_value())
                                 {
                                     chargeItem.setSubjectKvnr(kvnr);
                                 }
                                 return chargeItem;
                     });

    return result;
}
// GEMREQ-end A_22119#query-call

::model::ChargeInformation DatabaseFrontend::retrieveChargeInformation(const model::PrescriptionId& id) const
{
    const auto chargeItem = mBackend->retrieveChargeInformation(id);
    const auto codecWithKey = DatabaseCodecWithKey{
        .codec = mCodec,
        .key = std::get<SafeString>(chargeItemKey(chargeItem.prescriptionId, chargeItem.blobId, chargeItem.salt))};

    return chargeItem.toChargeInformation(codecWithKey);
}

std::tuple<::model::ChargeInformation, BlobId, db_model::Blob> DatabaseFrontend::retrieveChargeInformationForUpdate(const model::PrescriptionId& id) const
{
    auto chargeItem = mBackend->retrieveChargeInformationForUpdate(id);
    const auto codecWithKey = DatabaseCodecWithKey{
        .codec = mCodec,
        .key = std::get<SafeString>(chargeItemKey(chargeItem.prescriptionId, chargeItem.blobId, chargeItem.salt))};

    return std::make_tuple(
        chargeItem.toChargeInformation(codecWithKey),
        chargeItem.blobId,
        std::move(chargeItem.salt));
}

// GEMREQ-start A_22117-01#query-call-deleteChargeInformation
void DatabaseFrontend::deleteChargeInformation(const model::PrescriptionId& id)
{
    mBackend->deleteChargeInformation(id);
}
// GEMREQ-end A_22117-01#query-call-deleteChargeInformation

// GEMREQ-start A_22157#query-call-clearAllChargeInformation
void DatabaseFrontend::clearAllChargeInformation(const model::Kvnr& kvnr)
{
    const auto& hashedKvnr = mDerivation.hashKvnr(kvnr);
    mBackend->clearAllChargeInformation(hashedKvnr);
}
// GEMREQ-end A_22157#query-call-clearAllChargeInformation

// GEMREQ-start A_22157#query-call-clearAllChargeItemCommunications
void DatabaseFrontend::clearAllChargeItemCommunications(const model::Kvnr& kvnr)
{
    const auto& hashedKvnr = mDerivation.hashKvnr(kvnr);
    mBackend->clearAllChargeItemCommunications(hashedKvnr);
}
// GEMREQ-end A_22157#query-call-clearAllChargeItemCommunications

// GEMREQ-start A_22117-01#query-call-deleteCommunicationsForChargeItem
void DatabaseFrontend::deleteCommunicationsForChargeItem(const model::PrescriptionId& id)
{
    mBackend->deleteCommunicationsForChargeItem(id);
}
// GEMREQ-end A_22117-01#query-call-deleteCommunicationsForChargeItem

uint64_t DatabaseFrontend::countChargeInformationForInsurant(const model::Kvnr& kvnr,
                                                             const std::optional<UrlArguments>& search)
{
    return mBackend->countChargeInformationForInsurant(mDerivation.hashKvnr(kvnr), search);
}

DatabaseBackend& DatabaseFrontend::getBackend()
{
    return *mBackend;
}

DatabaseFrontend::~DatabaseFrontend(void) = default;

std::shared_ptr<Compression> DatabaseFrontend::compressionInstance()
{
    static auto theCompressionInstance =
        std::make_shared<ZStd>(Configuration::instance().getStringValue(ConfigurationKey::ZSTD_DICTIONARY_DIR));
    return theCompressionInstance;
}

model::Task DatabaseFrontend::getModelTask(const db_model::Task& dbTask, const std::optional<SafeString>& key)
{

    model::Task modelTask(dbTask.prescriptionId, dbTask.prescriptionId.type(), dbTask.lastModified, dbTask.authoredOn,
                          dbTask.status);
    if (dbTask.kvnr && key)
    {
        const auto type = dbTask.prescriptionId.isPkv() ? model::Kvnr::Type::pkv : model::Kvnr::Type::gkv;
        modelTask.setKvnr(model::Kvnr{std::string{mCodec.decode(*dbTask.kvnr, *key)}, type});
    }
    if (dbTask.expiryDate)
    {
        modelTask.setExpiryDate(*dbTask.expiryDate);
    }
    if (dbTask.acceptDate)
    {
        modelTask.setAcceptDate(*dbTask.acceptDate);
    }
    if (dbTask.accessCode && key)
    {
        modelTask.setAccessCode(mCodec.decode(*dbTask.accessCode, *key));
    }
    if (dbTask.secret && key)
    {
        modelTask.setSecret(mCodec.decode(*dbTask.secret, *key));
    }
    if (dbTask.owner && key)
    {
        modelTask.setOwner(mCodec.decode(*dbTask.owner, *key));
    }
    if(dbTask.lastMedicationDispense)
    {
        modelTask.updateLastMedicationDispense(dbTask.lastMedicationDispense.value());
    }
    return modelTask;
}

std::optional<model::Binary> DatabaseFrontend::getHealthcareProviderPrescription(const db_model::Task& dbTask,
                                                                                 const SafeString& key)
{
    if (! dbTask.healthcareProviderPrescription)
    {
        return std::nullopt;
    }
    return model::Binary::fromJsonNoValidation(mCodec.decode(*dbTask.healthcareProviderPrescription, key));
}

std::optional<model::Bundle> DatabaseFrontend::getReceipt(const db_model::Task& dbTask, const SafeString& key)
{
    if (! dbTask.receipt)
    {
        return std::nullopt;
    }
    return model::Bundle::fromJsonNoValidation(mCodec.decode(*dbTask.receipt, key));
}

std::optional<model::Bundle> DatabaseFrontend::getDispenseItem(const std::optional<db_model::EncryptedBlob>& dbDispensItem, const SafeString& key)
{
    if (! dbDispensItem.has_value())
    {
        return std::nullopt;
    }
    return model::Bundle::fromJsonNoValidation(mCodec.decode(dbDispensItem.value(), key));
}


SafeString DatabaseFrontend::taskKey(const model::PrescriptionId& taskId)
{
    A_19700.start("Get key derivation information for updates.");
    const auto [blobId, salt, authoredOn] = mBackend->getTaskKeyData(taskId);
    A_19700.finish();
    return mDerivation.taskKey(taskId, authoredOn, blobId, salt);
}

std::optional<SafeString> DatabaseFrontend::taskKey(const db_model::Task& dbTask)
{
    if (dbTask.status == model::Task::Status::cancelled)
    {
        return std::nullopt;
    }
    A_19700.start("Get key derivation information for retrievals.");
    Expect(not dbTask.salt.empty(), "missing salt in task.");
    auto key = std::make_optional(mDerivation.taskKey(dbTask.prescriptionId, dbTask.authoredOn,
                                                      dbTask.blobId, dbTask.salt));
    A_19700.finish();
    return key;
}

std::tuple<SafeString, BlobId> DatabaseFrontend::medicationDispenseKey(const db_model::HashedKvnr& hashedKvnr)
{
    auto blobId = mHsmPool.acquire().session().getLatestTaskPersistenceId();
    auto salt = mBackend->retrieveSaltForAccount(hashedKvnr,
                                                 db_model::MasterKeyType::medicationDispense,
                                                 blobId);
    if (salt)
    {
        return {mDerivation.medicationDispenseKey(hashedKvnr, blobId, *salt), blobId};
    }
    else
    {
        SafeString key;
        OptionalDeriveKeyData secondCallData;
        std::tie(key, secondCallData) = mDerivation.initialMedicationDispenseKey(hashedKvnr);
        auto dbSalt = mBackend->insertOrReturnAccountSalt(hashedKvnr,
                                                          db_model::MasterKeyType::medicationDispense,
                                                          secondCallData.blobId,
                                                          db_model::Blob{std::vector<uint8_t>{secondCallData.salt}});
        // there was a concurrent insert so we need to derive again with the
        // salt created by the concurrent, who was first to insert the salt.
        blobId = secondCallData.blobId;
        if (dbSalt)
        {
            return {mDerivation.medicationDispenseKey(hashedKvnr, blobId, *dbSalt), blobId};
        }
        return {std::move(key), blobId};
    }
}

::std::tuple<::SafeString, ::BlobId, ::db_model::Blob>
DatabaseFrontend::chargeItemKey(const ::model::PrescriptionId& prescriptionId) const
{
    A_19700.start("Get derivation key.");
    auto [key, deriveKeyData] = mDerivation.initialChargeItemKey(prescriptionId);
    A_19700.finish();

    return {::std::move(key), deriveKeyData.blobId, ::db_model::Blob{deriveKeyData.salt}};
}

::std::tuple<::SafeString, ::BlobId, ::db_model::Blob>
DatabaseFrontend::chargeItemKey(const model::PrescriptionId& prescriptionId, const BlobId& blobId, const db_model::Blob& salt) const
{
    A_19700.start("Get derivation key.");
    auto key = mDerivation.chargeItemKey(prescriptionId, blobId, salt);
    A_19700.finish();

    return {::std::move(key), blobId, salt};
}

std::tuple<SafeString, BlobId> DatabaseFrontend::auditEventKey(const db_model::HashedKvnr& hashedKvnr)
{
    auto blobId = mHsmPool.acquire().session().getLatestAuditLogPersistenceId();
    auto salt = mBackend->retrieveSaltForAccount(hashedKvnr, db_model::MasterKeyType::auditevent, blobId);

    if (salt)
    {
        return {mDerivation.auditEventKey(hashedKvnr, blobId, *salt), blobId};
    }
    else
    {
        SafeString keyForAuditData;
        OptionalDeriveKeyData secondCallData;
        std::tie(keyForAuditData, secondCallData) = mDerivation.initialAuditEventKey(hashedKvnr);
        auto dbSalt =
            mBackend->insertOrReturnAccountSalt(hashedKvnr, db_model::MasterKeyType::auditevent, secondCallData.blobId,
                                                db_model::Blob{std::vector<uint8_t>{secondCallData.salt}});
        // there was a concurrent insert so we need to derive again with the
        // salt created by the concurrent, who was first to insert the salt.
        blobId = secondCallData.blobId;
        if (dbSalt)
        {
            return {mDerivation.auditEventKey(hashedKvnr, secondCallData.blobId, *dbSalt), blobId};
        }
        return {std::move(keyForAuditData), blobId};
    }
}

std::tuple<SafeString, BlobId>
DatabaseFrontend::communicationKeyAndId(const std::string_view& identity, const db_model::HashedId& identityHashed)
{
    auto blobId = mHsmPool.acquire().session().getLatestCommunicationPersistenceId();
    auto salt = mBackend->retrieveSaltForAccount(identityHashed,
                                                 db_model::MasterKeyType::communication,
                                                 blobId);
    SafeString key;
    if (salt)
    {
        key = mDerivation.communicationKey(identity, identityHashed, blobId, *salt);
    }
    else
    {
        OptionalDeriveKeyData secondCallData;
        std::tie(key, secondCallData)
                = mDerivation.initialCommunicationKey(identity, identityHashed);
        auto dbSalt = mBackend->insertOrReturnAccountSalt(identityHashed,
                                                          db_model::MasterKeyType::communication,
                                                          secondCallData.blobId,
                                                          db_model::Blob{std::vector<uint8_t>{secondCallData.salt}});
        // there was a concurrent insert so we need to derive again with the
        // salt created by the concurrent, who was first to insert the salt.
        if (dbSalt)
        {
            key = mDerivation.communicationKey(identity,identityHashed, secondCallData.blobId, *dbSalt);
        }
        blobId = secondCallData.blobId;
    }
    return {std::move(key), blobId};
}
