/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/mock/MockDatabase.hxx"
#include "test_config.h"

#include "erp/compression/ZStd.hxx"
#include "erp/crypto/CMAC.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/database/DatabaseModel.hxx"
#include "erp/hsm/HsmClient.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Uuid.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/SafeString.hxx"
#include "erp/util/TLog.hxx"

#include "test/mock/TestUrlArguments.hxx"
#include "test/util/ResourceManager.hxx"

#include <tuple>
#include <test/util/CryptoHelper.hxx>

namespace {
std::shared_ptr<Compression> compressionInstance()
{
    static auto theCompressionInstance =
        std::make_shared<ZStd>(Configuration::instance().getStringValue(ConfigurationKey::ZSTD_DICTIONARY_DIR));
    return theCompressionInstance;
}

template <typename T>
auto tryOptional(const model::Task& task, T (model::Task::*member)() const) -> std::optional<std::remove_reference_t<T>>
{
   try
    {
        return std::make_optional((task.*member)());
    }
    catch (const model::ModelException&)
    {
        return std::nullopt;
    }
}

std::pair<model::PrescriptionType, MockTaskTable>
makeMockTaskTable(MockAccountTable& accounts, model::PrescriptionType type)
{
    return std::make_pair(type, MockTaskTable{accounts, type});
}

} // anonymous namespace



std::unique_ptr<Database> MockDatabase::createMockDatabase(HsmPool& hsmPool, KeyDerivation& keyDerivation)
{
    return std::make_unique<DatabaseFrontend>(std::make_unique<MockDatabase>(hsmPool), hsmPool, keyDerivation);
}
using namespace model;



MockDatabase::MockDatabase(HsmPool& hsmPool)
    : mAccounts{}
    , mTasks({
        makeMockTaskTable(mAccounts, PrescriptionType::apothekenpflichigeArzneimittel),
        makeMockTaskTable(mAccounts, PrescriptionType::direkteZuweisung),
        makeMockTaskTable(mAccounts, PrescriptionType::apothekenpflichtigeArzneimittelPkv)})
    , mCommunications{mAccounts}
    , mDerivation{hsmPool}
    , mHsmPool{hsmPool}
    , mCodec{compressionInstance()}
{

}

std::tuple<BlobId, db_model::Blob, model::Timestamp>
MockDatabase::getTaskKeyData(const model::PrescriptionId& taskId)
{
    return mTasks.at(taskId.type()).getTaskKeyData(taskId);
}

std::vector<db_model::Task> MockDatabase::retrieveAllTasksForPatient(const db_model::HashedKvnr& kvnrHashed,
                                                                     const std::optional<UrlArguments>& search)
{
    std::vector<db_model::Task> allTasks;
    for (const auto& table: mTasks)
    {
        auto&& tasks = table.second.retrieveAllTasksForPatient(kvnrHashed, {});
        std::move(tasks.begin(), tasks.end(), std::back_inserter(allTasks));
    }
    if (search.has_value())
    {
        return TestUrlArguments(search.value()).apply(std::move(allTasks));
    }
    return allTasks;
}

uint64_t MockDatabase::countAllTasksForPatient(const db_model::HashedKvnr& kvnr,
                                               const std::optional<UrlArguments>& search)
{
    uint64_t count = 0;
    for (const auto& table: mTasks)
    {
        count += table.second.countAllTasksForPatient(kvnr, search);
    }
    return count;
}

std::tuple<std::vector<db_model::MedicationDispense>, bool>
MockDatabase::retrieveAllMedicationDispenses(const db_model::HashedKvnr& kvnr,
                                             const std::optional<model::PrescriptionId>& prescriptionId,
                                             const std::optional<UrlArguments>& search)
{
    std::vector<TestUrlArguments::SearchMedicationDispense> medicationDispenses;
    bool hasNextPage = false;
    if (prescriptionId.has_value())
    {
        medicationDispenses =
            mTasks.at(prescriptionId->type()).retrieveAllMedicationDispenses(kvnr, prescriptionId);
    }
    else
    {
        for (const auto& table: mTasks)
        {
            auto&& dispenses = table.second.retrieveAllMedicationDispenses(kvnr, prescriptionId);
            std::move(dispenses.begin(), dispenses.end(), std::back_inserter(medicationDispenses));
        }
        if (search.has_value())
        {
            TestUrlArguments tua(search.value());
            medicationDispenses = tua.applySearch(std::move(medicationDispenses));
            medicationDispenses = tua.apply(std::move(medicationDispenses));
            hasNextPage = medicationDispenses.size() > search->pagingArgument().getCount();
            if (hasNextPage)
            {
                medicationDispenses.pop_back();
            }
        }
    }
    std::vector<db_model::MedicationDispense> resultDispenses;
    resultDispenses.reserve(medicationDispenses.size());
    for (auto&& md: medicationDispenses)
    {
        resultDispenses.emplace_back(std::move(md));
    }
    return {resultDispenses, hasNextPage};
}

std::optional<Uuid> MockDatabase::insertCommunication(
    const model::PrescriptionId& prescriptionId,
    const model::Timestamp& timeSent,
    const model::Communication::MessageType messageType,
    const db_model::HashedId& sender,
    const db_model::HashedId& recipient,
    const std::optional<model::Timestamp>& timeReceived,
    BlobId senderBlobId,
    const db_model::EncryptedBlob& messageForSender,
    BlobId recipientBlobId,
    const db_model::EncryptedBlob& messageForRecipient)
{
    return mCommunications.insertCommunication(prescriptionId, timeSent, messageType, sender, recipient, timeReceived,
                                               senderBlobId, messageForSender, recipientBlobId, messageForRecipient);
}

uint64_t MockDatabase::countRepresentativeCommunications(
    const db_model::HashedKvnr& insurantA,
    const db_model::HashedKvnr& insurantB,
    const PrescriptionId& prescriptionId)
{
    return mCommunications.countRepresentativeCommunications(insurantA, insurantB, prescriptionId);
}


bool MockDatabase::existCommunication(const Uuid& communicationId)
{
    return mCommunications.existCommunication(communicationId);
}


std::vector<db_model::Communication> MockDatabase::retrieveCommunications (
    const db_model::HashedId& recipient,
    const std::optional<Uuid>& communicationId,
    const std::optional<UrlArguments>& search)
{
    return mCommunications.retrieveCommunications(recipient, communicationId, search);
}


uint64_t MockDatabase::countCommunications(
    const db_model::HashedId& recipient,
    const std::optional<UrlArguments>& search)
{
    return mCommunications.countCommunications(recipient, search);
}


std::vector<Uuid> MockDatabase::retrieveCommunicationIds(const db_model::HashedId& recipient)
{
    return mCommunications.retrieveCommunicationIds(recipient);
}


std::tuple<std::optional<Uuid>, std::optional<Timestamp>>
MockDatabase::deleteCommunication(const Uuid& communicationId, const db_model::HashedId& sender)
{
    return mCommunications.deleteCommunication(communicationId, sender);
}

void MockDatabase::markCommunicationsAsRetrieved (
    const std::vector<Uuid>& communicationIds,
    const Timestamp& retrieved,
    const db_model::HashedId& recipient)
{
    mCommunications.markCommunicationsAsRetrieved(communicationIds, retrieved, recipient);
}

void MockDatabase::deleteCommunicationsForTask (const model::PrescriptionId& taskId)
{
    mCommunications.deleteCommunicationsForTask(taskId);
}

CmacKey MockDatabase::acquireCmac(const date::sys_days& validDate, const CmacKeyCategory& cmacType, RandomSource& randomSource)
{
    std::lock_guard lock(mutex);
    std::ostringstream dateTypeKey;
    dateTypeKey << date::year_month_day{validDate};
    dateTypeKey << "_";
    dateTypeKey << magic_enum::enum_name(cmacType);
    auto key = mCmac.find( dateTypeKey.str() );
    if (key != mCmac.end())
    {
        return key->second;
    }
    auto newKey = CmacKey::randomKey(randomSource);
    mCmac.emplace(dateTypeKey.str(), newKey);
    return newKey;
}

std::optional<db_model::EncryptedBlob> MockDatabase::optionalEncrypt(const SafeString& key,
                                                                           const std::optional<std::string_view>& data,
                                                                           Compression::DictionaryUse dictUse) const
{
    return data?std::make_optional(mCodec.encode(*data, key, dictUse)):std::nullopt;
}


SafeString MockDatabase::getMedicationDispenseKey(const db_model::HashedKvnr& kvnr, BlobId blobId)
{
    auto salt = mAccounts.getSalt(kvnr, db_model::MasterKeyType::medicationDispense, blobId);
    if (salt)
    {
        return mDerivation.medicationDispenseKey(kvnr, blobId, *salt);
    }
    auto [key, derivationData] = mDerivation.initialMedicationDispenseKey(kvnr);
    db_model::Blob saltBlob{derivationData.salt.begin(), derivationData.salt.end()};
    (void)mAccounts.insertOrReturnAccountSalt(kvnr, db_model::MasterKeyType::medicationDispense,
                                              derivationData.blobId, std::move(saltBlob));
    return std::move(key);
}


SafeString MockDatabase::getAuditEventKey(const db_model::HashedKvnr& kvnr, BlobId& blobId)
{
    auto salt = mAccounts.getSalt(kvnr, db_model::MasterKeyType::auditevent, blobId);
    if (salt)
    {
        return mDerivation.auditEventKey(kvnr, blobId, *salt);
    }
    auto [key, derivationData] = mDerivation.initialAuditEventKey(kvnr);
    db_model::Blob saltBlob{derivationData.salt.begin(), derivationData.salt.end()};
    (void) mAccounts.insertOrReturnAccountSalt(kvnr, db_model::MasterKeyType::auditevent, derivationData.blobId,
                                               std::move(saltBlob));
    blobId = derivationData.blobId;
    return std::move(key);
}


void MockDatabase::insertTask(const model::Task& task,
                              const std::optional<std::string>& medicationDispense,
                              const std::optional<std::string>& healthCareProviderPrescription,
                              const std::optional<std::string>& receipt )
{
    TVLOG(3) << "insertTask: " << task.prescriptionId().toString();

    auto& newRow = mTasks.at(task.type()).createRow(task.prescriptionId(), task.lastModifiedDate(), task.authoredOn());
    auto [taskKey, derivationData] = mDerivation.initialTaskKey(task.prescriptionId(), task.authoredOn());
    newRow.kvnr = optionalEncrypt(taskKey, task.kvnr());
    newRow.kvnrHashed =
        task.kvnr()
            ? std::make_optional(mDerivation.hashKvnr(*task.kvnr()))
            : std::nullopt;
    if (task.status() > model::Task::Status::draft)
    {
        newRow.expiryDate = tryOptional(task, &model::Task::expiryDate);
        newRow.acceptDate = tryOptional(task, &model::Task::acceptDate);
    }
    newRow.status = task.status();
    newRow.taskKeyBlobId = derivationData.blobId;
    newRow.salt = db_model::Blob{derivationData.salt.begin(), derivationData.salt.end()};
    newRow.accessCode = optionalEncrypt(taskKey, tryOptional(task, &model::Task::accessCode));
    newRow.secret = optionalEncrypt(taskKey, task.secret());
    if (medicationDispense)
    {
        Expect3(newRow.kvnrHashed, "KVNR required to encrypt MedicationDispense", std::logic_error);
        BlobId blobId = mHsmPool.acquire().session().getLatestTaskPersistenceId();
        auto key = getMedicationDispenseKey(*newRow.kvnrHashed, blobId);
        newRow.medicationDispenseBundle = mCodec.encode(*medicationDispense, key,
                                                                Compression::DictionaryUse::Default_json);
        newRow.medicationDispenseKeyBlobId = blobId;
        auto medicationDispenseModel = model::MedicationDispense::fromJsonNoValidation(*medicationDispense);
        newRow.whenHandedOver = medicationDispenseModel.whenHandedOver();
        newRow.whenPrepared = medicationDispenseModel.whenPrepared();
        newRow.performer = mDerivation.hashTelematikId(medicationDispenseModel.telematikId());
    }
    newRow.healthcareProviderPrescription = optionalEncrypt(taskKey, healthCareProviderPrescription);

    newRow.receipt = optionalEncrypt(taskKey, receipt);
}

void MockDatabase::insertChargeItem(const model::PrescriptionId& prescriptionId,
                                    const model::ChargeItem& chargeItem,
                                    const std::string& dispenseItemXML)
{
    auto dispenseItem = model::Bundle::fromXmlNoValidation(dispenseItemXML);
    const auto [blobId, salt, authoredOn] = getTaskKeyData(prescriptionId);
    const auto key = mDerivation.taskKey(prescriptionId, authoredOn, blobId, salt);
    const auto& encrypedChargeItem =
        mCodec.encode(chargeItem.serializeToJsonString(), key, Compression::DictionaryUse::Default_json);
    const auto& encrypedDispenseItem =
        mCodec.encode(dispenseItem.serializeToJsonString(), key, Compression::DictionaryUse::Default_json);

    const auto hashedTelematikId = mDerivation.hashTelematikId(chargeItem.entererTelematikId());
    storeChargeInformation(hashedTelematikId, prescriptionId, model::Timestamp::now(), encrypedChargeItem, encrypedDispenseItem);
}

void MockDatabase::insertAuditEvent(const model::AuditEvent& auditEvent,
                                    model::AuditEventId id)
{
    const std::string sourceObserverReference = std::string(auditEvent.sourceObserverReference());
    const auto deviceId = std::stoi(sourceObserverReference.substr(sourceObserverReference.rfind('/')+1));
    const auto hashedKvnr = mDerivation.hashKvnr(auditEvent.entityName());
    auto blobId = mHsmPool.acquire().session().getLatestAuditLogPersistenceId();
    auto key = getAuditEventKey(hashedKvnr, blobId);
    mAuditEventData.emplace_back(
        auditEvent.agentType(),
        id,
        *optionalEncrypt(
            key, model::AuditMetaData(auditEvent.agentName(), std::get<1>(auditEvent.agentWho())).serializeToJsonString()),
        auditEvent.action(),
        hashedKvnr,
        gsl::narrow_cast<int16_t>(deviceId),
        model::PrescriptionId::fromString(auditEvent.entityDescription()),
        blobId);
    mAuditEventData.back().id = std::string(auditEvent.id());
    mAuditEventData.back().recorded = auditEvent.recorded();
}

namespace {

std::string replacePrescriptionId(const std::string& templateStr, const std::string& prescriptionId)
{
    return String::replaceAll(templateStr, "##PRESCRIPTION_ID##", prescriptionId);
}
std::string replaceKvnr(const std::string& templateStr, const std::string& kvnr)
{
    return String::replaceAll(templateStr, "##KVNR##", kvnr);
}

}

void MockDatabase::fillWithStaticData ()
{
    auto& resourceManager = ResourceManager::instance();
    const auto& kbvBundle = CryptoHelper::toCadesBesSignature(
            resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle.xml")
        );
    const auto dataPath(std::string{ TEST_DATA_DIR } + "/EndpointHandlerTest");
    auto task1 = model::Task::fromJsonNoValidation(FileHelper::readFileAsString(dataPath + "/task1.json"));
    auto task2 = model::Task::fromJsonNoValidation(FileHelper::readFileAsString(dataPath + "/task2.json"));
    auto task3 = model::Task::fromJsonNoValidation(FileHelper::readFileAsString(dataPath + "/task3.json"));
    auto task4 = model::Task::fromJsonNoValidation(FileHelper::readFileAsString(dataPath + "/task4.json"));
    task4.setHealthCarePrescriptionUuid();
    auto task5 = model::Task::fromJsonNoValidation(FileHelper::readFileAsString(dataPath + "/task5.json"));
    auto task6 = model::Task::fromJsonNoValidation(FileHelper::readFileAsString(dataPath + "/task6.json"));
    auto task7 = model::Task::fromJsonNoValidation(FileHelper::readFileAsString(dataPath + "/task7.json"));
    auto task169 = model::Task::fromJsonNoValidation(FileHelper::readFileAsString(dataPath + "/task169.json"));

    const auto healthCarePrescriptionUuid4 = task4.healthCarePrescriptionUuid().value();
    const auto healthCarePrescriptionBundle4 =
            model::Binary(healthCarePrescriptionUuid4, kbvBundle).serializeToJsonString();

    const auto& kbvBundle5 = CryptoHelper::toCadesBesSignature(
        resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_task5.xml"));
    const auto healthCarePrescriptionUuid5 = task5.healthCarePrescriptionUuid().value();
    const auto healthCarePrescriptionBundle5 =
        model::Binary(healthCarePrescriptionUuid5, kbvBundle5).serializeToJsonString();

    const auto medicationDispense1Content = FileHelper::readFileAsString(dataPath + "/medication_dispense1.json");

    insertTask(task1, medicationDispense1Content);
    insertTask(task2);
    insertTask(task3);
    insertTask(task4, std::nullopt, healthCarePrescriptionBundle4);
    insertTask(task5, std::nullopt, healthCarePrescriptionBundle5);
    insertTask(task6);
    insertTask(task7);
    insertTask(task169);

    // add static audit event entries
    insertAuditEvent(
        model::AuditEvent::fromJsonNoValidation(FileHelper::readFileAsString(dataPath + "/audit_event.json")),
        model::AuditEventId::POST_Task_activate);
    insertAuditEvent(model::AuditEvent::fromJsonNoValidation(
                         FileHelper::readFileAsString(dataPath + "/audit_event_delete_task.json")),
        model::AuditEventId::Task_delete_expired_id);

    // PKV related test data:
    // PKV tasks
    // Activated task with consent (added below):
    const auto pkvTaskActivatedTemplate = resourceManager.getStringResource(dataPath + "/task_pkv_activated_template.json");
    const auto pkvTaskId1 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50000);
    const auto pkvKvnr1 = "X500000000";
    auto taskPkv1 = model::Task::fromJsonNoValidation(
        replaceKvnr(replacePrescriptionId(pkvTaskActivatedTemplate, pkvTaskId1.toString()), pkvKvnr1));
    taskPkv1.setHealthCarePrescriptionUuid();
    // Activated task without consent:
    const auto pkvTaskId1a = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50001);
    const auto pkvKvnr1a = "X500000001";
    auto taskPkv1a = model::Task::fromJsonNoValidation(
        replaceKvnr(replacePrescriptionId(pkvTaskActivatedTemplate, pkvTaskId1a.toString()), pkvKvnr1a));
    taskPkv1a.setHealthCarePrescriptionUuid();
    // Created task:
    const auto pkvTaskCreatedTemplate = resourceManager.getStringResource(dataPath + "/task_pkv_created_template.json");
    const auto pkvTaskId2 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50010);
    const auto pkvKvnr2 = "X500000010";
    auto taskPkv2 = model::Task::fromJsonNoValidation(
        replaceKvnr(replacePrescriptionId(pkvTaskCreatedTemplate, pkvTaskId2.toString()), pkvKvnr2));

    // PKV KBV Bundles for the activated tasks:
    const auto kbvBundlePkvTemplate = resourceManager.getStringResource(dataPath + "/kbv_pkv_bundle_template.xml");
    const auto& kbvBundlePkv1 = CryptoHelper::toCadesBesSignature(
        replaceKvnr(replacePrescriptionId(kbvBundlePkvTemplate, pkvTaskId1.toString()), pkvKvnr1));
    const auto healthCarePrescriptionBundlePkv1 =
        model::Binary(taskPkv1.healthCarePrescriptionUuid().value(), kbvBundlePkv1).serializeToJsonString();
    const auto& kbvBundlePkv1a = CryptoHelper::toCadesBesSignature(
        replaceKvnr(replacePrescriptionId(kbvBundlePkvTemplate, pkvTaskId1a.toString()), pkvKvnr1a));
    const auto healthCarePrescriptionBundlePkv1a =
        model::Binary(taskPkv1a.healthCarePrescriptionUuid().value(), kbvBundlePkv1a).serializeToJsonString();

    insertTask(taskPkv1, std::nullopt, healthCarePrescriptionBundlePkv1);
    insertTask(taskPkv1a, std::nullopt, healthCarePrescriptionBundlePkv1a);
    insertTask(taskPkv2);

    // add static consent entry for kvnr1
    const auto consentTemplate = resourceManager.getStringResource(dataPath + "/consent_template.json");
    const auto dateTimeStr = "2021-06-01T07:13:00+05:00";
    const auto consent = model::Consent::fromJsonNoValidation(String::replaceAll(replaceKvnr(consentTemplate, pkvKvnr1), "##DATETIME##", dateTimeStr));
    storeConsent(mDerivation.hashKvnr(consent.patientKvnr()), consent.dateTime());

    // add static chargeItem entry for kvnr1
    const auto chargeItemInput = resourceManager.getStringResource(dataPath + "/charge_item_input_marked.json");
    auto chargeItem = model::ChargeItem::fromJsonNoValidation(chargeItemInput);
    const auto dispenseItemXML = resourceManager.getStringResource("test/EndpointHandlerTest/dispense_item.xml");

    chargeItem.setSubjectKvnr(pkvKvnr1);
    chargeItem.setEntererTelematikId("606358757");

    insertChargeItem(pkvTaskId1, chargeItem, dispenseItemXML);

    // data needed for creation of ChargeItem:
    // Closed task for insurant with consent (added above):
    const auto pkvTaskClosedTemplate = resourceManager.getStringResource(dataPath + "/task_pkv_closed_template.json");
    const auto pkvTaskId3 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50020);
    const auto taskPkv3 = model::Task::fromJsonNoValidation(
        replaceKvnr(replacePrescriptionId(pkvTaskClosedTemplate, pkvTaskId3.toString()), pkvKvnr1));
    const auto& kbvBundlePkv3 = CryptoHelper::toCadesBesSignature(
        replaceKvnr(replacePrescriptionId(kbvBundlePkvTemplate, pkvTaskId3.toString()), pkvKvnr1));
    const auto healthCarePrescriptionBundlePkv3 =
        model::Binary(taskPkv3.healthCarePrescriptionUuid().value(), kbvBundlePkv3).serializeToJsonString();
    const auto receipt = replacePrescriptionId(FileHelper::readFileAsString(dataPath + "/receipt_template.json"), pkvTaskId3.toString());
    insertTask(taskPkv3, std::nullopt, healthCarePrescriptionBundlePkv3, receipt);
    // Closed task without consent:
    const auto pkvTaskId4 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50021);
    const auto taskPkv4 = model::Task::fromJsonNoValidation(
        replaceKvnr(replacePrescriptionId(pkvTaskClosedTemplate, pkvTaskId4.toString()), pkvKvnr1a));
    insertTask(taskPkv4);

    // static ChargeItem for task for insurant with consent (added above):
    const auto chargeItemTemplateXml = resourceManager.getStringResource(dataPath + "/charge_item_PUT_template.xml");
    const auto chargeItemXml =
        String::replaceAll(replaceKvnr(replacePrescriptionId(chargeItemTemplateXml, pkvTaskId3.toString()), pkvKvnr1),
                           "##DISPENSE_BUNDLE##", Base64::encode(dispenseItemXML));
    auto chargeItemForManip = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
    chargeItemForManip.deleteContainedBinary();
    insertChargeItem(pkvTaskId3, chargeItemForManip, dispenseItemXML);
}

std::tuple<model::PrescriptionId, model::Timestamp> MockDatabase::createTask(model::PrescriptionType prescriptionType,
                                                                             model::Task::Status taskStatus,
                                                                             const model::Timestamp& lastUpdated,
                                                                             const model::Timestamp& created)
{
    return mTasks.at(prescriptionType).createTask(prescriptionType, taskStatus, lastUpdated, created);
}

void MockDatabase::activateTask(const model::PrescriptionId& taskId,
                                const db_model::EncryptedBlob& encryptedKvnr,
                                const db_model::HashedKvnr& hashedKvnr,
                                model::Task::Status taskStatus,
                                const model::Timestamp& lastModified,
                                const model::Timestamp& expiryDate,
                                const model::Timestamp& acceptDate,
                                const db_model::EncryptedBlob& healthCareProviderPrescription)
{
    mTasks.at(taskId.type())
        .activateTask(taskId, encryptedKvnr, hashedKvnr, taskStatus, lastModified, expiryDate, acceptDate,
                      healthCareProviderPrescription);
}

void MockDatabase::updateTask(const model::PrescriptionId& taskId,
                              const db_model::EncryptedBlob& accessCode,
                              BlobId blobId,
                              const db_model::Blob& salt)
{
    mTasks.at(taskId.type()).updateTask(taskId, accessCode, blobId, salt);;
}

void MockDatabase::updateTaskStatusAndSecret(const model::PrescriptionId& taskId,
                                             model::Task::Status taskStatus,
                                             const model::Timestamp& lastModifiedDate,
                                             const std::optional<db_model::EncryptedBlob>& taskSecret)
{
    return mTasks.at(taskId.type()).updateTaskStatusAndSecret(taskId, taskStatus, lastModifiedDate, taskSecret);
}

void MockDatabase::updateTaskMedicationDispenseReceipt(const model::PrescriptionId& taskId,
                                                       const model::Task::Status& taskStatus,
                                                       const model::Timestamp& lastModified,
                                                       const db_model::EncryptedBlob& medicationDispense,
                                                       BlobId medicationDispenseBlobId,
                                                       const db_model::HashedTelematikId& telematicId,
                                                       const model::Timestamp& whenHandedOver,
                                                       const std::optional<model::Timestamp>& whenPrepared,
                                                       const db_model::EncryptedBlob& taskReceipt)
{
    mTasks.at(taskId.type())
        .updateTaskMedicationDispenseReceipt(taskId, taskStatus, lastModified, medicationDispense,
                                             medicationDispenseBlobId, telematicId, whenHandedOver, whenPrepared,
                                             taskReceipt);
}

void MockDatabase::updateTaskClearPersonalData(const model::PrescriptionId& taskId, model::Task::Status taskStatus,
                                               const model::Timestamp& lastModified)
{
    mTasks.at(taskId.type()).updateTaskClearPersonalData(taskId, taskStatus, lastModified);
}

std::string MockDatabase::storeAuditEventData(db_model::AuditData& auditData)
{
    auto id = Uuid().toString();
    auditData.id = id;
    auditData.recorded = model::Timestamp::now();
    mAuditEventData.emplace_back(auditData);
    return id;
}

std::vector<db_model::AuditData> MockDatabase::retrieveAuditEventData(
    const db_model::HashedKvnr& kvnr,
    const std::optional<Uuid>& id,
    const std::optional<model::PrescriptionId>& prescriptionId,
    const std::optional<UrlArguments>& search)
{
    std::vector<db_model::AuditData> allAuditData;
    for (const auto& auditData : mAuditEventData)
    {
        if(auditData.insurantKvnr == kvnr &&
           (!id.has_value() || id.value().toString() == auditData.id) &&
            (!prescriptionId.has_value() || prescriptionId == auditData.prescriptionId))
        {
            allAuditData.emplace_back(auditData);
        }
    }

    if (search.has_value())
        return TestUrlArguments(search.value()).apply(std::move(allAuditData));
    else
        return allAuditData;
}

uint64_t MockDatabase::countAuditEventData(
    const db_model::HashedKvnr& kvnr,
    const std::optional<UrlArguments>& search)
{
    auto result = retrieveAuditEventData(kvnr, {}, {}, {});  // get all data for kvnr;
    if (search.has_value())
        return TestUrlArguments(search.value()).applySearch(std::move(result)).size();
    else
        return result.size();
}

std::optional<db_model::Task> MockDatabase::retrieveTaskAndReceipt(const model::PrescriptionId& taskId)
{
    return mTasks.at(taskId.type()).retrieveTaskAndReceipt(taskId);
}

std::optional<db_model::Task> MockDatabase::retrieveTaskAndPrescription(const model::PrescriptionId& taskId)
{
    return mTasks.at(taskId.type()).retrieveTaskAndPrescription(taskId);
}

std::optional<db_model::Task> MockDatabase::retrieveTaskAndPrescriptionAndReceipt(const model::PrescriptionId& taskId)
{
    return mTasks.at(taskId.type()).retrieveTaskAndPrescriptionAndReceipt(taskId);
}

void MockDatabase::deleteTask (const model::PrescriptionId& taskId)
{
    mTasks.at(taskId.type()).deleteTask(taskId);
}

void MockDatabase::deleteAuditEvent(const Uuid& eventId)
{
    mAuditEventData.remove_if(
        [&](const db_model::AuditData& elem) { return Uuid{elem.id} == eventId; });
}

void MockDatabase::deleteAuditEventDataForTask (const model::PrescriptionId& taskId)
{
    mAuditEventData.remove_if(
        [taskId](const auto& elem)
        {
            return elem.prescriptionId.has_value() && elem.prescriptionId->toDatabaseId() == taskId.toDatabaseId();
        });
}

void MockDatabase::commitTransaction()
{
}

void MockDatabase::closeConnection()
{
}

void MockDatabase::healthCheck()
{
}

std::optional<db_model::Task> MockDatabase::retrieveTaskForUpdate(const model::PrescriptionId& taskId)
{
    return mTasks.at(taskId.type()).retrieveTaskBasics(taskId);
}

::std::optional<::db_model::Task>
MockDatabase::retrieveTaskForUpdateAndPrescription(const ::model::PrescriptionId& taskId)
{
    return mTasks.at(taskId.type()).retrieveTaskForUpdateAndPrescription(taskId);
}

std::optional<db_model::Blob> MockDatabase::insertOrReturnAccountSalt(const db_model::HashedId& accountId,
                                                                      db_model::MasterKeyType masterKeyType,
                                                                      BlobId blobId,
                                                                      const db_model::Blob& salt)
{
    return mAccounts.insertOrReturnAccountSalt(accountId, masterKeyType, blobId, salt);
}

void MockDatabase::storeConsent(const db_model::HashedKvnr& kvnr, const model::Timestamp& creationTime)
{
    mConsents.storeConsent(kvnr, creationTime);
}

std::optional<model::Timestamp> MockDatabase::retrieveConsentDateTime(const db_model::HashedKvnr& kvnr)
{
    return mConsents.getConsentDateTime(kvnr);
}

bool MockDatabase::clearConsent(const db_model::HashedKvnr& kvnr)
{
    return mConsents.clearConsent(kvnr);
}

void MockDatabase::storeChargeInformation(const db_model::HashedTelematikId& pharmacyId, model::PrescriptionId id,
                                          const model::Timestamp& enteredDate,
                                          const db_model::EncryptedBlob& chargeItem,
                                          const db_model::EncryptedBlob& dispenseItem)
{
    Expect3(id.type() == model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
            "MockDatabase::storeChargeInformation(...) called with wrong prescription type: " + id.toString(),
            std::logic_error);
    mTasks.at(PrescriptionType::apothekenpflichtigeArzneimittelPkv)
        .storeChargeInformation(pharmacyId, id, enteredDate, chargeItem, dispenseItem);
}

std::vector<db_model::ChargeItem>
MockDatabase::retrieveAllChargeItemsForPharmacy(const db_model::HashedTelematikId& requestingPharmacy,
                                                const std::optional<UrlArguments>& search) const
{
    return mTasks.at(PrescriptionType::apothekenpflichtigeArzneimittelPkv)
                .retrieveAllChargeItemsForPharmacy(requestingPharmacy, search);
}

std::vector<db_model::ChargeItem>
MockDatabase::retrieveAllChargeItemsForInsurant(const db_model::HashedKvnr& requestingInsurant,
                                                const std::optional<UrlArguments>& search) const
{
    return mTasks.at(PrescriptionType::apothekenpflichtigeArzneimittelPkv)
                .retrieveAllChargeItemsForInsurant(requestingInsurant, search);
}

std::tuple<db_model::ChargeItem, db_model::EncryptedBlob>
MockDatabase::retrieveChargeInformation(const model::PrescriptionId& id) const
{
    return mTasks.at(id.type()).retrieveChargeInformation(id);
}

std::tuple<db_model::ChargeItem, db_model::EncryptedBlob>
MockDatabase::retrieveChargeInformationForUpdate(const model::PrescriptionId& id) const
{
    return mTasks.at(id.type()).retrieveChargeInformationForUpdate(id);
}

void MockDatabase::deleteChargeInformation(const model::PrescriptionId& id)
{
    mTasks.at(id.type()).deleteChargeInformation(id);
}

void MockDatabase::clearAllChargeInformation(const db_model::HashedKvnr& insurant)
{
    mTasks.at(PrescriptionType::apothekenpflichtigeArzneimittelPkv).clearAllChargeInformation(insurant);
}

uint64_t MockDatabase::countChargeInformationForInsurant(const db_model::HashedKvnr& insurant,
                                                              const std::optional<UrlArguments>& search)
{
    uint64_t count = 0;
    for (const auto& table: mTasks)
    {
        count += table.second.countChargeInformationForInsurant(insurant, search);
    }
    return count;
}

uint64_t MockDatabase::countChargeInformationForPharmacy(const db_model::HashedTelematikId& requestingPharmacy,
                                                              const std::optional<UrlArguments>& search)
{
    uint64_t count = 0;
    for (const auto& table: mTasks)
    {
        count += table.second.countChargeInformationForPharmacy(requestingPharmacy, search);
    }
    return count;
}

bool MockDatabase::isBlobUsed(BlobId blobId) const
{
    auto hasBlobId = [blobId](const db_model::AuditData& data){ return data.blobId == blobId;};
    auto blobUser = find_if(mAuditEventData.begin(), mAuditEventData.end(), hasBlobId);
    if (blobUser != mAuditEventData.end())
    {
        TVLOG(0) << "Blob " << blobId << R"( is still in use by "auditdata":)" << blobUser->prescriptionId->toString();
        return true;
    }
    bool usedInTasks =
            std::any_of(mTasks.cbegin(), mTasks.cend(), [blobId](const auto& taskTable)
                                                        {return taskTable.second.isBlobUsed(blobId);}
        );
    return mAccounts.isBlobUsed(blobId) || usedInTasks || mCommunications.isBlobUsed(blobId);
}


std::optional<db_model::Blob> MockDatabase::retrieveSaltForAccount(const db_model::HashedId& accountId,
                                                                   db_model::MasterKeyType masterKeyType,
                                                                   BlobId blobId)
{
    return mAccounts.getSalt(accountId, masterKeyType, blobId);
}
