/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
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
#include "test/util/ResourceTemplates.hxx"

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
        makeMockTaskTable(mAccounts, PrescriptionType::apothekenpflichtigeArzneimittelPkv),
        makeMockTaskTable(mAccounts, PrescriptionType::direkteZuweisungPkv)})
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
        auto&& tasks = table.second.retrieveAllTasksForPatient(kvnrHashed, {}, false, false, false);
        std::move(tasks.begin(), tasks.end(), std::back_inserter(allTasks));
    }
    if (search.has_value())
    {
        return TestUrlArguments(search.value()).apply(std::move(allTasks));
    }
    return allTasks;
}

std::vector<db_model::Task>
MockDatabase::retrieveAll160TasksWithAccessCode(const db_model::HashedKvnr& kvnrHashed,
                                                          const std::optional<UrlArguments>& search)
{
    std::vector<db_model::Task> allTasks;
    for (const auto& table: mTasks)
    {
        auto&& tasks = table.second.retrieveAllTasksForPatient(kvnrHashed, {}, true, false, true);
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
    BlobId senderBlobId,
    const db_model::EncryptedBlob& messageForSender,
    BlobId recipientBlobId,
    const db_model::EncryptedBlob& messageForRecipient)
{
    return mCommunications.insertCommunication(prescriptionId, timeSent, messageType, sender, recipient,
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
    db_model::Blob saltBlob{derivationData.salt};
    (void)mAccounts.insertOrReturnAccountSalt(kvnr, db_model::MasterKeyType::medicationDispense,
                                              derivationData.blobId, saltBlob);
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
    db_model::Blob saltBlob{derivationData.salt};
    (void) mAccounts.insertOrReturnAccountSalt(kvnr, db_model::MasterKeyType::auditevent, derivationData.blobId,
                                               saltBlob);
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
    const auto kvnr = task.kvnr();
    if (kvnr.has_value())
    {
        newRow.kvnr = optionalEncrypt(taskKey, kvnr->id());
        newRow.kvnrHashed = mDerivation.hashKvnr(*kvnr);
    }
    else
    {
        newRow.kvnr = std::nullopt;
        newRow.kvnrHashed = std::nullopt;
    }
    if (task.status() > model::Task::Status::draft)
    {
        newRow.expiryDate = tryOptional(task, &model::Task::expiryDate);
        newRow.acceptDate = tryOptional(task, &model::Task::acceptDate);
    }
    newRow.status = task.status();
    newRow.taskKeyBlobId = derivationData.blobId;
    newRow.salt = db_model::Blob{derivationData.salt};
    newRow.accessCode = optionalEncrypt(taskKey, tryOptional(task, &model::Task::accessCode));
    newRow.secret = optionalEncrypt(taskKey, task.secret());
    newRow.owner = optionalEncrypt(taskKey, task.owner());
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

void MockDatabase::insertAuditEvent(const model::AuditEvent& auditEvent,
                                    model::AuditEventId id)
{
    const std::string sourceObserverReference = std::string(auditEvent.sourceObserverReference());
    const auto deviceId = std::stoi(sourceObserverReference.substr(sourceObserverReference.rfind('/')+1));
    const auto hashedKvnr = mDerivation.hashKvnr(model::Kvnr{std::string{auditEvent.entityName()}});
    auto blobId = mHsmPool.acquire().session().getLatestAuditLogPersistenceId();
    auto key = getAuditEventKey(hashedKvnr, blobId);
    mAuditEventData.emplace_back(
        auditEvent.agentType(),
        id,
        *optionalEncrypt(
            key,
            model::AuditMetaData(
                auditEvent.agentName(),
                std::get<1>(auditEvent.agentWho())).serializeToJsonString()),
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
    const auto& kbvBundle = CryptoHelper::toCadesBesSignature(ResourceTemplates::kbvBundleXml());
    std::string gematikVersionStr{
        v_str(model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>())};
    const auto dataPath(std::string{ TEST_DATA_DIR } + "/EndpointHandlerTest");

    const char* const kvnr = "X123456788";
    const auto taskId1 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4711);
    const auto task1 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId1, .kvnr = kvnr}));

    const auto taskId2 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4712);
    const auto task2 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId2, .kvnr = kvnr}));

    const auto taskId3 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto task3 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = taskId3}));

    const auto taskId4 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4714);
    auto task4 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId4}));

    const auto taskId5 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4715);
    const auto task5 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::InProgress, .prescriptionId = taskId5,
         .timestamp = model::Timestamp::fromFhirDateTime("2021-02-02T16:18:43.036+00:00")}));

    const auto taskId6 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4716);
    auto task6 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::InProgress, .prescriptionId = taskId6}));

    const auto taskId7 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4717);
    auto task7 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId7}));

    const auto taskId169 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisung, 4711);
    const auto task169 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId169, .kvnr = kvnr}));

    const auto taskId169Draft = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisung, 4718);
    const auto task169Draft = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = taskId169Draft, .kvnr = kvnr}));

    const auto healthCarePrescriptionUuid4 = task4.healthCarePrescriptionUuid().value();
    const auto healthCarePrescriptionBundle4 = model::Binary(healthCarePrescriptionUuid4, kbvBundle).serializeToJsonString();

    const auto& kbvBundle5 = CryptoHelper::toCadesBesSignature(ResourceTemplates::kbvBundleXml({.prescriptionId = taskId5}));
    const auto healthCarePrescriptionUuid5 = task5.healthCarePrescriptionUuid().value();
    const auto healthCarePrescriptionBundle5 = model::Binary(healthCarePrescriptionUuid5, kbvBundle5).serializeToJsonString();

    const auto medicationDispenseXml = ResourceTemplates::medicationDispenseXml({.prescriptionId = taskId1});
    const auto medicationDispense1Content = model::MedicationDispense::fromXmlNoValidation(medicationDispenseXml).serializeToJsonString();

    insertTask(task1, medicationDispense1Content);
    insertTask(task2);
    insertTask(task3);
    insertTask(task4, std::nullopt, healthCarePrescriptionBundle4);
    insertTask(task5, std::nullopt, healthCarePrescriptionBundle5);
    insertTask(task6);
    insertTask(task7);
    insertTask(task169);
    insertTask(task169Draft);

    // add static audit event entries
    const std::string auditEventFileName = dataPath + "/audit_event_" + gematikVersionStr + ".json";
    auto auditEvent = model::AuditEvent::fromJsonNoValidation(FileHelper::readFileAsString(auditEventFileName));
    insertAuditEvent(auditEvent, model::AuditEventId::POST_Task_activate);

    // add audit event with invalid event id to be able to check error case
    auditEvent.setId("9c994b00-0998-421c-9878-dc669d65ae1e");
    insertAuditEvent(auditEvent, static_cast<model::AuditEventId>(999));

    const std::string auditEventDeleteFileName = dataPath + "/audit_event_delete_task_" + gematikVersionStr + ".json";
    insertAuditEvent(model::AuditEvent::fromJsonNoValidation(FileHelper::readFileAsString(auditEventDeleteFileName)),
                     model::AuditEventId::Task_delete_expired_id);

    // PKV related test data:
    // PKV tasks
    // Activated task with consent and charge item (added below):
    const auto pkvTaskId1 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50000);
    const char* const pkvKvnr1 = "X500000056";
    auto taskPkv1 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = pkvTaskId1, .kvnr = pkvKvnr1}));
    // Activated task without consent:
    const auto pkvTaskId1a = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50001);
    const char* const pkvKvnr1a = "X500000017";
    auto taskPkv1a = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = pkvTaskId1a, .kvnr = pkvKvnr1a}));
    // Activated task with consent (added below):
    const auto pkvTaskId209 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisungPkv, 50002);
    const char* const pkvKvnr209 = "X500000029";
    auto taskPkv209 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = pkvTaskId209, .kvnr = pkvKvnr209}));
    // Activated task without consent:
    const auto pkvTaskId209a = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisungPkv, 50003);
    const char* const pkvKvnr209a = "X500000031";
    auto taskPkv209a = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = pkvTaskId209a, .kvnr = pkvKvnr209a}));
    // Created task:

    const auto pkvTaskId2 =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50010);
    const auto pkvTaskCreated = ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = pkvTaskId2});
    auto taskPkv2 = model::Task::fromJsonNoValidation(pkvTaskCreated);

    const auto pkvTaskIdCreated209 =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisungPkv, 50011);
    const auto pkvTaskCreated209 = ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = pkvTaskIdCreated209});
    auto taskPkvCreated209 = model::Task::fromJsonNoValidation(pkvTaskCreated209);

    // PKV KBV Bundles for the activated tasks:
    //const auto kbvBundlePkvTemplate = resourceManager.getStringResource(dataPath + "/kbv_pkv_bundle_template.xml");
    const auto kbvBundlePkv1Xml = ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId1, .kvnr = pkvKvnr1});
    const auto& kbvBundlePkv1 = CryptoHelper::toCadesBesSignature(kbvBundlePkv1Xml);
    auto healthCarePrescriptionBundlePkv1 = model::Binary(taskPkv1.healthCarePrescriptionUuid().value(), kbvBundlePkv1);
    const auto& kbvBundlePkv1a = CryptoHelper::toCadesBesSignature(
        ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId1a, .kvnr = pkvKvnr1a}));
    const auto healthCarePrescriptionBundlePkv1a =
        model::Binary(taskPkv1a.healthCarePrescriptionUuid().value(), kbvBundlePkv1a).serializeToJsonString();
    const auto& kbvBundlePkv209 = CryptoHelper::toCadesBesSignature(
        ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId209, .kvnr = pkvKvnr209}));
    const auto healthCarePrescriptionBundlePkv209 =
        model::Binary(taskPkv209.healthCarePrescriptionUuid().value(), kbvBundlePkv209).serializeToJsonString();
    const auto& kbvBundlePkv209a = CryptoHelper::toCadesBesSignature(
        ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId209a, .kvnr = pkvKvnr209a}));
    const auto healthCarePrescriptionBundlePkv209a =
        model::Binary(taskPkv209a.healthCarePrescriptionUuid().value(), kbvBundlePkv209a).serializeToJsonString();

    insertTask(taskPkv1, std::nullopt, healthCarePrescriptionBundlePkv1.serializeToJsonString());
    insertTask(taskPkv1a, std::nullopt, healthCarePrescriptionBundlePkv1a);
    insertTask(taskPkv209, std::nullopt, healthCarePrescriptionBundlePkv209);
    insertTask(taskPkv209a, std::nullopt, healthCarePrescriptionBundlePkv209a);
    insertTask(taskPkv2);
    insertTask(taskPkvCreated209);

    // add static consent entry for kvnr1
    const auto consentTemplate = resourceManager.getStringResource(dataPath + "/consent_template.json");
    const char* const dateTimeStr = "2021-06-01T07:13:00+05:00";
    const auto consent = model::Consent::fromJsonNoValidation(String::replaceAll(replaceKvnr(consentTemplate, pkvKvnr1), "##DATETIME##", dateTimeStr));
    storeConsent(mDerivation.hashKvnr(consent.patientKvnr()), consent.dateTime());
    // add static consent entry for pkvKvnr209
    const auto consent209 = model::Consent::fromJsonNoValidation(String::replaceAll(replaceKvnr(consentTemplate, pkvKvnr209), "##DATETIME##", dateTimeStr));
    storeConsent(mDerivation.hashKvnr(consent209.patientKvnr()), consent209.dateTime());

    // add static chargeItem entry for kvnr1
    const auto chargeItemInput = resourceManager.getStringResource(dataPath + "/charge_item_input_marked.json");
    auto chargeItem = model::ChargeItem::fromJsonNoValidation(chargeItemInput);
    const auto dispenseItemXML = ResourceTemplates::medicationDispenseBundleXml();
    const auto dispenseItemBundle = ::model::Bundle::fromXmlNoValidation(dispenseItemXML);

    chargeItem.setPrescriptionId(pkvTaskId1);
    chargeItem.setSubjectKvnr(pkvKvnr1);
    chargeItem.setEntererTelematikId("606358757");
    chargeItem.setAccessCode(mockAccessCode);

    {
        const auto [key, optionalDerivedKeyData] =
            mDerivation.initialChargeItemKey(chargeItem.prescriptionId().value());

        storeChargeInformation(
            ::db_model::ChargeItem{
                ::model::ChargeInformation{
                    ::std::move(chargeItem), ::std::move(healthCarePrescriptionBundlePkv1),
                    ::model::Bundle::fromXmlNoValidation(kbvBundlePkv1Xml),
                    ::model::Binary{dispenseItemBundle.getIdentifier().toString(),
                                    ::CryptoHelper::toCadesBesSignature(dispenseItemBundle.serializeToJsonString())},
                    ::model::AbgabedatenPkvBundle::fromXmlNoValidation(dispenseItemXML),
                    ::model::Bundle::fromJsonNoValidation(replacePrescriptionId(
                        ::FileHelper::readFileAsString(dataPath + "/receipt_template.json"), pkvTaskId1.toString()))},
                optionalDerivedKeyData.blobId, ::db_model::Blob{optionalDerivedKeyData.salt}, key, mCodec},
            mDerivation.hashKvnr(model::Kvnr{std::string{pkvKvnr1}}));
    }

    // data needed for creation of ChargeItem:
    // Closed task for insurant with consent (added above):
    const auto pkvTaskId3 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50020);
    const auto taskPkv3 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Completed, .prescriptionId = pkvTaskId3, .kvnr = pkvKvnr1}));
    const auto& kbvBundlePkv3 = CryptoHelper::toCadesBesSignature(
        ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId3, .kvnr = pkvKvnr1}));
    const auto healthCarePrescriptionBundlePkv3 =
        model::Binary(taskPkv3.healthCarePrescriptionUuid().value(), kbvBundlePkv3).serializeToJsonString();
    const auto receipt = replacePrescriptionId(FileHelper::readFileAsString(dataPath + "/receipt_template.json"), pkvTaskId3.toString());
    insertTask(taskPkv3, std::nullopt, healthCarePrescriptionBundlePkv3, receipt);
    // Closed task without consent:
    const auto pkvTaskId4 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50021);
    const auto taskPkv4 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Completed, .prescriptionId = pkvTaskId4, .kvnr = pkvKvnr1a}));
    insertTask(taskPkv4);

    // Closed task with consent but without charge item:
    const auto pkvTaskId5 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50022);
    const auto taskPkv5 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Completed, .prescriptionId = pkvTaskId5, .kvnr = pkvKvnr1}));
    const auto& kbvBundlePkv5 = CryptoHelper::toCadesBesSignature(
        ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId5, .kvnr = pkvKvnr1}));
    const auto healthCarePrescriptionBundlePkv5 =
        model::Binary(taskPkv5.healthCarePrescriptionUuid().value(), kbvBundlePkv3).serializeToJsonString();
    const auto receipt5 = replacePrescriptionId(FileHelper::readFileAsString(dataPath + "/receipt_template.json"), pkvTaskId5.toString());
    insertTask(taskPkv5, std::nullopt, healthCarePrescriptionBundlePkv5, receipt5);

    // static ChargeItem for task for insurant with consent (added above):
    const auto chargeItemXml =
        ResourceTemplates::chargeItemXml({.kvnr = model::Kvnr{pkvKvnr1},
                                          .prescriptionId = pkvTaskId3,
                                          .dispenseBundleBase64 = "",
                                          .operation = ResourceTemplates::ChargeItemOptions::OperationType::Put});
    auto chargeItemForManip = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
    chargeItemForManip.setPrescriptionId(pkvTaskId3);
    chargeItemForManip.deleteContainedBinary();
    chargeItemForManip.setAccessCode(mockAccessCode);

    const auto [key, optionalData] = mDerivation.initialChargeItemKey(chargeItemForManip.prescriptionId().value());

    storeChargeInformation(
        ::db_model::ChargeItem{
            ::model::ChargeInformation{
                ::std::move(chargeItemForManip),
                ::model::Binary{taskPkv3.healthCarePrescriptionUuid().value(),
                                CryptoHelper::toCadesBesSignature(
                                    ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId3, .kvnr = pkvKvnr1}))},
                ::model::Bundle::fromXmlNoValidation(
                    ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId3, .kvnr = pkvKvnr1})),
                ::model::Binary{dispenseItemBundle.getIdentifier().toString(),
                                ::CryptoHelper::toCadesBesSignature(dispenseItemBundle.serializeToXmlString())},
                ::model::AbgabedatenPkvBundle::fromXmlNoValidation(dispenseItemXML),
                ::model::Bundle::fromJsonNoValidation(replacePrescriptionId(
                    ::FileHelper::readFileAsString(dataPath + "/receipt_template.json"), pkvTaskId3.toString()))},
            optionalData.blobId, ::db_model::Blob{optionalData.salt}, key, mCodec},
        mDerivation.hashKvnr(model::Kvnr{std::string{pkvKvnr1}}));

    // ERP-17321 Charge item signed with an old certification
    {
        const char* qesOldFilename = "test/tsl/X509Certificate/80276883531000000027-C.HP.QES.pem";
        const char* erp17321kvnr = "E173210118";
        const auto pkvTaskId6 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50023);
        const auto taskPkv6 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
            {.taskType = ResourceTemplates::TaskType::Completed, .prescriptionId = pkvTaskId6, .kvnr = erp17321kvnr}));
        const auto& kbvBundlePkv6 = CryptoHelper::toCadesBesSignature(
            ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId6, .kvnr = erp17321kvnr}), qesOldFilename);
        const auto healthCarePrescriptionBundlePkv6 =
            model::Binary(taskPkv6.healthCarePrescriptionUuid().value(), kbvBundlePkv6).serializeToJsonString();
        const auto receipt6 = replacePrescriptionId(FileHelper::readFileAsString(dataPath + "/receipt_template.json"), pkvTaskId6.toString());
        insertTask(taskPkv6, std::nullopt, healthCarePrescriptionBundlePkv6, receipt6);
        TLOG(INFO) << pkvTaskId6.toString();

        const auto chargeItemXml =
            ResourceTemplates::chargeItemXml({.kvnr = model::Kvnr{pkvKvnr1},
                                            .prescriptionId = pkvTaskId6,
                                            .dispenseBundleBase64 = "",
                                            .operation = ResourceTemplates::ChargeItemOptions::OperationType::Put});
        auto chargeItemForManip = model::ChargeItem::fromXmlNoValidation(chargeItemXml);
        chargeItemForManip.setPrescriptionId(pkvTaskId6);
        chargeItemForManip.deleteContainedBinary();
        chargeItemForManip.setAccessCode(mockAccessCode);

        const auto [key, optionalData] = mDerivation.initialChargeItemKey(chargeItemForManip.prescriptionId().value());

        storeChargeInformation(
            ::db_model::ChargeItem{
                ::model::ChargeInformation{
                    ::std::move(chargeItemForManip),
                    ::model::Binary{taskPkv6.healthCarePrescriptionUuid().value(),
                                    CryptoHelper::toCadesBesSignature(
                                        ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId6, .kvnr = pkvKvnr1}),
                                        qesOldFilename)},
                    ::model::Bundle::fromXmlNoValidation(
                        ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId6, .kvnr = pkvKvnr1})),
                    ::model::Binary{dispenseItemBundle.getIdentifier().toString(),
                                    ::CryptoHelper::toCadesBesSignature(dispenseItemBundle.serializeToXmlString(),
                                        qesOldFilename)},
                    ::model::AbgabedatenPkvBundle::fromXmlNoValidation(dispenseItemXML),
                    ::model::Bundle::fromJsonNoValidation(replacePrescriptionId(
                        ::FileHelper::readFileAsString(dataPath + "/receipt_template.json"), pkvTaskId6.toString()))},
                optionalData.blobId, ::db_model::Blob{optionalData.salt}, key, mCodec},
            mDerivation.hashKvnr(model::Kvnr{std::string{pkvKvnr1}}));
    }

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
                                             const std::optional<db_model::EncryptedBlob>& taskSecret,
                                             const std::optional<db_model::EncryptedBlob>& owner)
{
    return mTasks.at(taskId.type()).updateTaskStatusAndSecret(taskId, taskStatus, lastModifiedDate, taskSecret, owner);
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
    auditData.recorded = model::Timestamp::now();
    auto dateEnc = auditData.recorded.toDatabaseSUuid();
    auto uuid = Uuid().toString();
    auto id = uuid.replace(0, 16, dateEnc.substr(0, 16));
    auditData.id = id;
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

std::optional<db_model::Task> MockDatabase::retrieveTaskAndReceipt(const model::PrescriptionId& taskId)
{
    return mTasks.at(taskId.type()).retrieveTaskAndReceipt(taskId);
}

std::optional<db_model::Task> MockDatabase::retrieveTaskAndPrescription(const model::PrescriptionId& taskId)
{
    return mTasks.at(taskId.type()).retrieveTaskAndPrescription(taskId);
}

std::optional<db_model::Task> MockDatabase::retrieveTaskWithSecretAndPrescription(const model::PrescriptionId& taskId)
{
    return mTasks.at(taskId.type()).retrieveTaskWithSecretAndPrescription(taskId);
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
bool MockDatabase::isCommitted() const
{
    return true;
}

std::string MockDatabase::retrieveSchemaVersion()
{
    return Database::expectedSchemaVersion;
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

void MockDatabase::storeChargeInformation(const ::db_model::ChargeItem& chargeItem, ::db_model::HashedKvnr kvnr)
{
    mChargeItems.storeChargeInformation(chargeItem, kvnr);
}

void MockDatabase::updateChargeInformation(const ::db_model::ChargeItem& chargeItem)
{
    mChargeItems.updateChargeInformation(chargeItem);
}

std::vector<db_model::ChargeItem>
MockDatabase::retrieveAllChargeItemsForInsurant(const db_model::HashedKvnr& requestingInsurant,
                                                const std::optional<UrlArguments>& search) const
{
    return mChargeItems.retrieveAllChargeItemsForInsurant(requestingInsurant, search);
}

::db_model::ChargeItem MockDatabase::retrieveChargeInformation(const model::PrescriptionId& id) const
{
    return mChargeItems.retrieveChargeInformation(id);
}

::db_model::ChargeItem MockDatabase::retrieveChargeInformationForUpdate(const model::PrescriptionId& id) const
{
    return mChargeItems.retrieveChargeInformationForUpdate(id);
}

void MockDatabase::deleteChargeInformation(const model::PrescriptionId& id)
{
    mChargeItems.deleteChargeInformation(id);
    mTasks.at(id.type()).deleteChargeItemSupportingInformation(id);
}

void MockDatabase::clearAllChargeInformation(const db_model::HashedKvnr& insurant)
{
    mChargeItems.clearAllChargeInformation(insurant);
    mTasks.at(::model::PrescriptionType::apothekenpflichtigeArzneimittelPkv)
        .clearAllChargeItemSupportingInformation(insurant);
    mTasks.at(::model::PrescriptionType::direkteZuweisungPkv)
        .clearAllChargeItemSupportingInformation(insurant);
}

void MockDatabase::clearAllChargeItemCommunications(const db_model::HashedKvnr& insurant)
{
    mCommunications.deleteChargeItemCommunicationsForKvnr(insurant);
}

void MockDatabase::deleteCommunicationsForChargeItem(const model::PrescriptionId& id)
{
    mCommunications.deleteCommunicationsForChargeItem(id);
}

uint64_t MockDatabase::countChargeInformationForInsurant(const db_model::HashedKvnr& insurant,
                                                         const std::optional<UrlArguments>& search)
{
    return mChargeItems.countChargeInformationForInsurant(insurant, search);
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
    bool usedInTasks = std::any_of(mTasks.cbegin(), mTasks.cend(), [blobId](const auto& taskTable) {
        return taskTable.second.isBlobUsed(blobId);
    });
    return mAccounts.isBlobUsed(blobId) || usedInTasks || mCommunications.isBlobUsed(blobId) ||
           mChargeItems.isBlobUsed(blobId);
}


std::optional<db_model::Blob> MockDatabase::retrieveSaltForAccount(const db_model::HashedId& accountId,
                                                                   db_model::MasterKeyType masterKeyType,
                                                                   BlobId blobId)
{
    return mAccounts.getSalt(accountId, masterKeyType, blobId);
}
