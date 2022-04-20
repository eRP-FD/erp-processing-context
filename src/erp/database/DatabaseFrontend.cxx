/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/database/DatabaseFrontend.hxx"

#include <boost/algorithm/string.hpp>

#include "erp/ErpRequirements.hxx"
#include "erp/compression/ZStd.hxx"
#include "erp/crypto/CMAC.hxx"
#include "erp/database/DatabaseBackend.hxx"
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
#include "erp/util/TLog.hxx"
#include "erp/util/search/UrlArguments.hxx"

using namespace model;

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

void DatabaseFrontend::healthCheck()
{
    mBackend->healthCheck();
}

std::tuple<std::vector<model::MedicationDispense>, bool>
DatabaseFrontend::retrieveAllMedicationDispenses(const std::string& kvnr,
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

std::optional<MedicationDispense> DatabaseFrontend::retrieveMedicationDispense(const std::string& kvnr,
                                                                               const model::MedicationDispenseId& id)
{
    auto hashedKvnr = mDerivation.hashKvnr(kvnr);

    const auto [encryptedResult, hasNextPage] =
        mBackend->retrieveAllMedicationDispenses(hashedKvnr, id.getPrescriptionId(), {});

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
    db_model::Blob salt{std::move(derivationData.salt)};
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
    A_19688.start("encrypt secret");
    auto key = taskKey(task.prescriptionId());
    std::optional<db_model::EncryptedBlob> secret;
    if (task.secret().has_value())
    {
        // TODO: do not compress secret
        secret = mCodec.encode(task.secret().value(), key, Compression::DictionaryUse::Default_json);
    }
    A_19688.finish();

    mBackend->updateTaskStatusAndSecret(task.prescriptionId(), task.status(), task.lastModifiedDate(), secret);
}

void DatabaseFrontend::activateTask(const Task& task, const Binary& healthCareProviderPrescription)
{
    A_19688.start("encrypt kvnr and presciption.");
    auto key = taskKey(task.prescriptionId());
    const auto encryptedPrescription = mCodec.encode(healthCareProviderPrescription.serializeToJsonString(), key,
                                                     Compression::DictionaryUse::Default_json);
    const auto& kvnr = task.kvnr();
    Expect(kvnr.has_value(), "KVNR not set in task during activate");
    auto encrypedKvnr = mCodec.encode(*kvnr, key, Compression::DictionaryUse::Default_json);

    const auto hashedKvnr = mDerivation.hashKvnr(*kvnr);
    A_19688.finish();

    mBackend->activateTask(task.prescriptionId(), encrypedKvnr, hashedKvnr, task.status(), task.lastModifiedDate(),
                           task.expiryDate(), task.acceptDate(), encryptedPrescription);
}

void DatabaseFrontend::updateTaskMedicationDispenseReceipt(
    const model::Task& task, const std::vector<model::MedicationDispense>& medicationDispenses,
    const model::ErxReceipt& receipt)
{
    A_19688.start("encrypt medication dispense and receipt");
    auto keyForTask = taskKey(task.prescriptionId());
    auto encryptReceipt = mCodec.encode(receipt.serializeToJsonString(), keyForTask, Compression::DictionaryUse::Default_json);
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

    model::Bundle medicationDispenseBundle{model::BundleType::collection, model::ResourceBase::NoProfile, Uuid()};
    for (const auto& medication : medicationDispenses)
    {
        medicationDispenseBundle.addResource({}, {}, {}, medication.jsonDocument());
    }

    /// MedicationDispense uses same derivation master key as task:
    auto [keyForMedicationDispense, blobId] = medicationDispenseKey(hashedKvnr);
    auto encryptedMedicationDispense =
        mCodec.encode(medicationDispenseBundle.serializeToJsonString(),
                      keyForMedicationDispense,
                      Compression::DictionaryUse::Default_json);
    mBackend->updateTaskMedicationDispenseReceipt(task.prescriptionId(),
                                                  task.status(),
                                                  task.lastModifiedDate(),
                                                  encryptedMedicationDispense,
                                                  blobId,
                                                  hashedTelematikId,
                                                  whenHandedOver,
                                                  whenPrepared,
                                                  encryptReceipt);
}

void DatabaseFrontend::updateTaskClearPersonalData(const model::Task& task)
{
    mBackend->updateTaskClearPersonalData(task.prescriptionId(), task.status(), task.lastModifiedDate());
}

std::string DatabaseFrontend::storeAuditEventData(model::AuditData& auditData)
{

    auto hashedKvnr = mDerivation.hashKvnr(auditData.insurantKvnr());

    std::optional<db_model::EncryptedBlob> encryptedMeta;
    std::optional<BlobId> auditEventBlobId;
    if (!auditData.metaData().isEmpty())
    {
        auto [keyForAuditData, blobId] = auditEventKey(hashedKvnr);
        encryptedMeta = mCodec.encode(auditData.metaData().serializeToJsonString(), keyForAuditData,
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
DatabaseFrontend::retrieveAuditEventData(const std::string& kvnr, const std::optional<Uuid>& id,
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

            ret.emplace_back(item.eventId, std::move(auditMetaData), item.action, item.agentType, kvnr, item.deviceId,
                             item.prescriptionId);
        }
        else
        {
            ret.emplace_back(item.eventId, AuditMetaData({}, {}), item.action, item.agentType, kvnr, item.deviceId,
                item.prescriptionId);
        }
        ret.back().setId(item.id);
        ret.back().setRecorded(item.recorded);
    }
    return ret;
}

uint64_t DatabaseFrontend::countAuditEventData(const std::string& kvnr,
                                               const std::optional<UrlArguments>& search)
{
    return mBackend->countAuditEventData(mDerivation.hashKvnr(kvnr), search);
}

std::optional<model::Task> DatabaseFrontend::retrieveTaskForUpdate(const PrescriptionId& taskId)
{
    const auto& dbTask = mBackend->retrieveTaskForUpdate(taskId);
    if (! dbTask)
    {
        return std::nullopt;
    }
    auto key = taskKey(*dbTask);
    return getModelTask(*dbTask, key);
}

::std::tuple<::std::optional<::model::Task>, ::std::optional<::model::Binary>>
DatabaseFrontend::retrieveTaskForUpdateAndPrescription(const ::model::PrescriptionId& taskId)
{
    const auto& dbTask = mBackend->retrieveTaskForUpdateAndPrescription(taskId);
    if (! dbTask)
    {
        return {};
    }
    const auto keyForTask = taskKey(*dbTask);
    return ::std::make_tuple(getModelTask(*dbTask, keyForTask),
                             keyForTask ? getHealthcareProviderPrescription(*dbTask, *keyForTask) : ::std::nullopt);
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

std::tuple<std::optional<Task>, std::optional<Binary>>
DatabaseFrontend::retrieveTaskAndPrescription(const PrescriptionId& taskId)
{
    const auto& dbTask = mBackend->retrieveTaskAndPrescription(taskId);
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

std::vector<model::Task> DatabaseFrontend::retrieveAllTasksForPatient(const std::string& kvnr,
                                                                      const std::optional<UrlArguments>& search)
{
    ErpExpect(kvnr.find('\0') == std::string::npos, HttpStatus::BadRequest, "null character in kvnr");
    ErpExpect(kvnr.size() == 10, HttpStatus::BadRequest, "kvnr must have 10 characters");

    auto dbTaskList = mBackend->retrieveAllTasksForPatient(mDerivation.hashKvnr(kvnr), search);
    std::vector<model::Task> allTasks;
    for (const auto& dbTask : dbTaskList)
    {
        auto modelTask = getModelTask(dbTask);
        modelTask.setKvnr(kvnr);
        allTasks.emplace_back(std::move(modelTask));
    }
    return allTasks;
}

uint64_t DatabaseFrontend::countAllTasksForPatient (const std::string& kvnr, const std::optional<UrlArguments>& search)
{
    return mBackend->countAllTasksForPatient(mDerivation.hashKvnr(kvnr), search);
}

std::optional<Uuid> DatabaseFrontend::insertCommunication(Communication& communication)
{
    const std::optional<std::string_view> sender = communication.sender();
    const std::optional<std::string_view> recipient = communication.recipient();
    const std::optional<model::Timestamp> timeSent = communication.timeSent();
    const std::optional<model::Timestamp> timeReceived = communication.timeReceived();

    ErpExpect(sender.has_value(), HttpStatus::InternalServerError, "communication object has no 'sender' value");
    ErpExpect(recipient.has_value(), HttpStatus::InternalServerError, "communication object has no 'recipient' value");
    ErpExpect(timeSent.has_value(), HttpStatus::InternalServerError, "communication object has no 'sent' value");

    const auto& messagePlain = communication.serializeToJsonString();

    const model::PrescriptionId& prescriptionId = communication.prescriptionId();
    const model::Communication::MessageType messageType = communication.messageType();
    const db_model::HashedId senderHashed = mDerivation.hashIdentity(sender.value());
    const db_model::HashedId recipientHashed = mDerivation.hashIdentity(recipient.value());
    auto [senderKey, senderBlobId] = communicationKeyAndId(sender.value(), senderHashed);
    auto [recipientKey, recipientBlobId] = communicationKeyAndId(recipient.value(), recipientHashed);
    auto messageForSender = mCodec.encode(messagePlain, senderKey, Compression::DictionaryUse::Default_json);
    auto messageForRecipient = mCodec.encode(messagePlain, recipientKey, Compression::DictionaryUse::Default_json);
    auto uuid = mBackend->insertCommunication(prescriptionId, timeSent.value(), messageType, senderHashed, recipientHashed,
                                              timeReceived, senderBlobId, messageForSender, recipientBlobId,
                                              messageForRecipient);
    if (uuid)
    {
        communication.setId(*uuid);
    }
    return uuid;
}

uint64_t DatabaseFrontend::countRepresentativeCommunications(const std::string& insurantA, const std::string& insurantB,
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

void DatabaseFrontend::deleteCommunicationsForTask(const model::PrescriptionId& taskId)
{
    return mBackend->deleteCommunicationsForTask(taskId);
}

void DatabaseFrontend::storeConsent(const Consent& consent)
{
    const auto& hashedKvnr = mDerivation.hashKvnr(consent.patientKvnr());
    return mBackend->storeConsent(hashedKvnr, consent.dateTime());
}

std::optional<model::Consent> DatabaseFrontend::retrieveConsent(const std::string_view& kvnr)
{
    const auto& hashedKvnr = mDerivation.hashKvnr(kvnr);
    auto dateTime = mBackend->retrieveConsentDateTime(hashedKvnr);
    if (dateTime.has_value())
    {
        return std::make_optional<model::Consent>(kvnr, *dateTime);
    }
    return std::nullopt;
}

bool DatabaseFrontend::clearConsent(const std::string_view& kvnr)
{
    const auto& hashedKvnr = mDerivation.hashKvnr(kvnr);
    return mBackend->clearConsent(hashedKvnr);
}

void DatabaseFrontend::storeChargeInformation(const std::string_view& pharmacyId, const model::ChargeItem& chargeItem,
                                              const model::Bundle& dispenseItem)
{
    //TODO ERP-8516: use appropriate encryption key
    const auto& prescriptionId = chargeItem.id();
    const auto& key = taskKey(prescriptionId);
    const auto& encrypedChargeItem =
            mCodec.encode(chargeItem.serializeToJsonString(), key, Compression::DictionaryUse::Default_json);
    const auto& encrypedDispenseItem =
            mCodec.encode(dispenseItem.serializeToJsonString(), key, Compression::DictionaryUse::Default_json);
    const auto& hashedPharmacyId = mDerivation.hashTelematikId(pharmacyId);
    mBackend->storeChargeInformation(hashedPharmacyId,
                                     prescriptionId,
                                     chargeItem.enteredDate(),
                                     encrypedChargeItem,
                                     encrypedDispenseItem);
}

std::vector<model::ChargeItem>
DatabaseFrontend::retrieveAllChargeItemsForPharmacy(const std::string_view& pharmacyTelematikId,
                                                    const std::optional<UrlArguments>& search) const
{
    const auto& hashedPharmacyId = mDerivation.hashTelematikId(pharmacyTelematikId);
    const auto& dbChargeItems = mBackend->retrieveAllChargeItemsForPharmacy(hashedPharmacyId, search);
    return decryptChargeItems(dbChargeItems);
}

std::vector<model::ChargeItem>
DatabaseFrontend::retrieveAllChargeItemsForInsurant(const std::string_view& kvnr,
                                                    const std::optional<UrlArguments>& search) const
{
    const auto& insurantHash = mDerivation.hashKvnr(kvnr);
    const auto& dbChargeItems = mBackend->retrieveAllChargeItemsForInsurant(insurantHash, search);
    return decryptChargeItems(dbChargeItems);
}

std::tuple<model::ChargeItem, model::Bundle>
DatabaseFrontend::retrieveChargeInformation(const model::PrescriptionId& id) const
{
    const auto& [dbChargeItem, dbDispenseItem] = mBackend->retrieveChargeInformation(id);
    //TODO ERP-8516: use appropriate encryption key
    const auto& key = mDerivation.taskKey(dbChargeItem.prescriptionId, dbChargeItem.authoredOn, dbChargeItem.blobId,
                                          dbChargeItem.salt);
    const auto& chargeItemJson = mCodec.decode(dbChargeItem.chargeItem, key);
    const auto& dispenseItemJson = mCodec.decode(dbDispenseItem, key);
    return std::make_tuple(model::ChargeItem::fromJsonNoValidation(chargeItemJson),
                           model::Bundle::fromJsonNoValidation(dispenseItemJson));
}

std::tuple<model::ChargeItem, model::Bundle>
DatabaseFrontend::retrieveChargeInformationForUpdate(const model::PrescriptionId& id) const
{
    const auto& [dbChargeItem, dbDispenseItem] = mBackend->retrieveChargeInformationForUpdate(id);
    //TODO ERP-8516: use appropriate encryption key
    const auto& key = mDerivation.taskKey(dbChargeItem.prescriptionId, dbChargeItem.authoredOn, dbChargeItem.blobId,
                                          dbChargeItem.salt);
    const auto& chargeItemJson = mCodec.decode(dbChargeItem.chargeItem, key);
    const auto& dispenseItemJson = mCodec.decode(dbDispenseItem, key);
    return std::make_tuple(model::ChargeItem::fromJsonNoValidation(chargeItemJson),
                           model::Bundle::fromJsonNoValidation(dispenseItemJson));
}

void DatabaseFrontend::deleteChargeInformation(const model::PrescriptionId& id)
{
    mBackend->deleteChargeInformation(id);
}

void DatabaseFrontend::clearAllChargeInformation(const std::string_view& kvnr)
{
    const auto& hashedKvnr = mDerivation.hashKvnr(kvnr);
    mBackend->clearAllChargeInformation(hashedKvnr);
}

uint64_t DatabaseFrontend::countChargeInformationForInsurant(const std::string& kvnr, 
                                                             const std::optional<UrlArguments>& search)
{
    return mBackend->countChargeInformationForInsurant(mDerivation.hashKvnr(kvnr), search);
}

uint64_t DatabaseFrontend::countChargeInformationForPharmacy(const std::string& pharmacyTelematikId,
                                                             const std::optional<UrlArguments>& search)
{
    return mBackend->countChargeInformationForPharmacy(mDerivation.hashTelematikId(pharmacyTelematikId), search);
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
        modelTask.setKvnr(mCodec.decode(*dbTask.kvnr, *key));
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



SafeString DatabaseFrontend::taskKey(const model::PrescriptionId& taskId)
{
    A_19700.start("Get key derivation infromation for updates.");
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
    A_19700.start("Get key derivation infromation for retrievals.");
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

std::vector<model::ChargeItem>
DatabaseFrontend::decryptChargeItems(const std::vector<db_model::ChargeItem>& dbChargeItems) const
{
    std::vector<model::ChargeItem> result;
    for (const auto& dbItem : dbChargeItems)
    {
        //TODO ERP-8516: use appropriate encryption key
        const auto& key = mDerivation.taskKey(dbItem.prescriptionId, dbItem.authoredOn, dbItem.blobId, dbItem.salt);
        const auto& chargeItemJson = mCodec.decode(dbItem.chargeItem, key);
        result.emplace_back(model::ChargeItem::fromJsonNoValidation(chargeItemJson));
    }
    return result;
}
