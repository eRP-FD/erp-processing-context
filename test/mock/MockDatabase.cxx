/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/mock/MockDatabase.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/model/Task.hxx"
#include "shared/compression/ZStd.hxx"
#include "shared/crypto/CMAC.hxx"
#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/crypto/SignedPrescription.hxx"
#include "shared/database/DatabaseModel.hxx"
#include "shared/hsm/HsmClient.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "shared/model/Bundle.hxx"
#include "shared/model/MedicationDispense.hxx"
#include "shared/model/MedicationDispenseId.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/util/SafeString.hxx"
#include "shared/util/TLog.hxx"
#include "shared/util/Uuid.hxx"
#include "test_config.h"
#include "test/mock/TestUrlArguments.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"

#include <boost/algorithm/string/replace.hpp>
#include <test/util/CryptoHelper.hxx>
#include <tuple>


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

std::map<model::PrescriptionType, MockTaskTable> makeMockTaskTables(MockAccountTable& accounts)
{
    std::map<model::PrescriptionType, MockTaskTable> result;
    for (auto type : magic_enum::enum_values<model::PrescriptionType>())
    {
        result.emplace(makeMockTaskTable(accounts, type));
    }
    return result;
}

} // anonymous namespace



std::unique_ptr<Database> MockDatabase::createMockDatabase(HsmPool& hsmPool, KeyDerivation& keyDerivation)
{
    return std::make_unique<DatabaseFrontend>(std::make_unique<MockDatabase>(hsmPool), hsmPool, keyDerivation);
}
using namespace model;


MockDatabase::MockDatabase(HsmPool& hsmPool)
    : mAccounts{}
    , mTasks(makeMockTaskTables(mAccounts))
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

uint64_t MockDatabase::countAll160Tasks(const db_model::HashedKvnr& kvnr, const std::optional<UrlArguments>& search)
{
    uint64_t count = 0;
    for (const auto& table : mTasks)
    {
        count += table.second.countAll160Tasks(kvnr, search);
    }
    return count;
}
std::vector<db_model::Task> MockDatabase::retrieveAllTasksForEu(const db_model::HashedKvnr& kvnr,
                                                                const std::optional<UrlArguments>& search)
{
    std::vector<db_model::Task> allTasks;
    for (const auto& table : mTasks)
    {
        auto&& tasks = table.second.retrieveAllTasksForEu(kvnr, search);
        std::move(tasks.begin(), tasks.end(), std::back_inserter(allTasks));
    }
    if (search.has_value())
    {
        return TestUrlArguments(search.value()).apply(std::move(allTasks));
    }
    return allTasks;
}

uint64_t MockDatabase::countAllTasksForEu(const db_model::HashedKvnr& kvnr, const std::optional<UrlArguments>& search)
{
    uint64_t count = 0;
    for (const auto& table : mTasks)
    {
        count += table.second.countAllTasksForEu(kvnr, search);
    }
    return count;
}

std::vector<db_model::MedicationDispense>
MockDatabase::retrieveAllMedicationDispenses(const db_model::HashedKvnr& kvnr,
                                             const std::optional<model::PrescriptionId>& prescriptionId,
                                             const std::optional<UrlArguments>& search)
{
    std::vector<TestUrlArguments::SearchMedicationDispense> medicationDispenses;
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
        }
    }
    std::vector<db_model::MedicationDispense> resultDispenses;
    resultDispenses.reserve(medicationDispenses.size());
    for (auto&& md: medicationDispenses)
    {
        resultDispenses.emplace_back(std::move(md));
    }
    return resultDispenses;
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

std::string MockDatabase::receiptJson(const model::Task& task, const std::string& telematikId,
                                     const CadesBesSignature& prescription)
{
    const auto& config = Configuration::instance();
    const Uuid digestId{};
    const Uuid authorIdentifier{};
    const std::optional metaVersionId =
        config.getOptionalStringValue(ConfigurationKey::SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_VERSION_ID);
    const auto& prescriptionId = task.prescriptionId();

    const model::Composition compositionResource{telematikId, task.lastStatusChangeDate(), model::Timestamp::now(),
                                                 authorIdentifier.toUrn(), digestId.toUrn()};
    auto digest = Base64::encode(prescription.getMessageDigest());
    auto linkBase = Configuration::instance().getStringValue(ConfigurationKey::PUBLIC_E_PRESCRIPTION_SERVICE_URL);
    const auto prescriptionDigestResource =
        ::model::Binary{digestId.toString(), digest, ::model::Binary::Type::Digest, metaVersionId};

    const auto link = linkBase + "/Task/" + prescriptionId.toString() + "/$close/";
    model::ErxReceipt responseReceipt(Uuid(value(task.receiptUuid())), link, prescriptionId, compositionResource,
                                      authorIdentifier.toUrn(), model::Device{}, digestId.toUrn(),
                                      prescriptionDigestResource);
    auto hsmSession = mHsmPool.acquire();
    auto [key, _] = hsmSession.session().getVauSigPrivateKey({}, {});
    const std::string base64SignatureData =
        CadesBesSignature(hsmSession.session().getVauSigCertificate(), key, responseReceipt.serializeToXmlString())
            .getBase64();
    const model::Signature signature(base64SignatureData, model::Timestamp::now(), authorIdentifier.toUrn());
    responseReceipt.setSignature(signature);
    return responseReceipt.serializeToJsonString();
}

std::tuple<Certificate, shared_EVP_PKEY> MockDatabase::certAndKey(const std::optional<std::string>& certPemFilename)
{
    auto pem = CryptoHelper::qesCertificatePem(certPemFilename);
    auto cert = Certificate::fromPem(pem);
    return std::make_tuple(std::move(cert),
                           EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{pem.data(), pem.size()}));

}


void MockDatabase::insertTask(const model::Task& task,
                              const std::optional<std::string>& medicationDispense,
                              const std::optional<std::string>& healthCareProviderPrescription,
                              const std::optional<std::string>& receipt )
{
    TVLOG(3) << "insertTask: " << task.prescriptionId().toString();

    auto& newRow =
        mTasks.at(task.type())
            .createRow(task.prescriptionId(), task.lastModifiedDate(), task.authoredOn(), task.lastStatusChangeDate());
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
        auto expiryDate = tryOptional(task, &model::Task::expiryDate);
        newRow.expiryDate = expiryDate
                                ?std::make_optional(date::year_month_day{expiryDate->localDay(model::Timestamp::GermanTimezone)})
                                :std::nullopt;
        auto acceptDate = tryOptional(task, &model::Task::acceptDate);
        newRow.acceptDate = acceptDate
                                ?std::make_optional(date::year_month_day{acceptDate->localDay(model::Timestamp::GermanTimezone)})
                                :std::nullopt;
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
        newRow.lastMedicationDispense = model::Timestamp::now();
    }
    newRow.healthcareProviderPrescription = optionalEncrypt(taskKey, healthCareProviderPrescription);

    newRow.receipt = optionalEncrypt(taskKey, receipt);

    if ((task.prescriptionId().type() == PrescriptionType::apothekenpflichigeArzneimittel ||
         task.prescriptionId().type() == PrescriptionType::apothekenpflichtigeArzneimittelPkv) &&
        healthCareProviderPrescription.has_value() && task.status() > Task::Status::draft)
    {
        newRow.isEuRedeemable = true;
    }
    else
    {
        newRow.isEuRedeemable = false;
    }
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
                std::get<1>(auditEvent.agentWho()),{}).serializeToJsonString()),
        auditEvent.action(),
        hashedKvnr,
        gsl::narrow_cast<int16_t>(deviceId),
        model::PrescriptionId::fromString(auditEvent.entityDescription()),
        blobId);
    mAuditEventData.back().id = std::string(auditEvent.id());
    mAuditEventData.back().recorded = auditEvent.recorded();
}

std::optional<db_model::Blob> MockDatabase::retrieveMedicationDispenseSalt(const model::PrescriptionId& taskId)
{
    return mTasks.at(taskId.type()).retrieveMedicationDispenseSalt(taskId);
}

bool MockDatabase::existsCountryCode(const std::string& countryCode)
{
    return mEuAccessPermissions.existsCountryCode(countryCode);
}

void MockDatabase::deleteEuAccessPermission(const db_model::HashedKvnr& kvnr)
{
    mEuAccessPermissions.deleteEuAccessPermission(kvnr);
}

void MockDatabase::createEuAccessPermission(const db_model::HashedKvnr& kvnr, const std::string& countryCode,
                                            const db_model::EncryptedBlob& accessCode, BlobId blobId,
                                            const db_model::Blob& salt, const model::Timestamp& validUntil)
{
    mEuAccessPermissions.createEuAccessPermission(kvnr, countryCode, accessCode, blobId, salt, validUntil);
}

std::optional<db_model::EuAccessPermission> MockDatabase::retrieveEuAccessPermission(const db_model::HashedKvnr& kvnr)
{
    return mEuAccessPermissions.retrieveEuAccessPermission(kvnr);
}

void MockDatabase::addCountry(const std::string& country)
{
    mEuAccessPermissions.addCountry(country);
}

namespace {

std::string replaceKvnr(const std::string& templateStr, const std::string& kvnr)
{
    return String::replaceAll(templateStr, "##KVNR##", kvnr);
}


}

void MockDatabase::fillWithStaticData ()
{
    auto& resourceManager = ResourceManager::instance();
    const auto [certificate, privKey] = certAndKey();

    const auto& kbvBundle = CryptoHelper::toCadesBesSignature(ResourceTemplates::kbvBundleXml());
    std::string gematikVersionStr{ResourceTemplates::Versions::GEM_ERP_current().renderVersion()};
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

    const auto taskId8 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4718);
    auto task8 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::InProgress, .prescriptionId = taskId8}));

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

    const auto& kbvBundle8 = CryptoHelper::toCadesBesSignature(ResourceTemplates::kbvBundleXml({.prescriptionId = taskId8}));
    const auto healthCarePrescriptionUuid8 = task8.healthCarePrescriptionUuid().value();
    const auto healthCarePrescriptionBundle8 = model::Binary(healthCarePrescriptionUuid8, kbvBundle8).serializeToJsonString();

    const auto medicationDispense1Xml = ResourceTemplates::medicationDispenseXml({.prescriptionId = taskId1});
    auto medicationDispense1 = model::MedicationDispense::fromXmlNoValidation(medicationDispense1Xml);
    medicationDispense1.setId(model::MedicationDispenseId{taskId1, 0});
    const auto medicationDispense1Content = medicationDispense1.serializeToJsonString();

    const auto medicationDispense8Xml = ResourceTemplates::medicationDispenseXml({.prescriptionId = taskId8});
    auto medicationDispense8 = model::MedicationDispense::fromXmlNoValidation(medicationDispense8Xml);
    medicationDispense8.setId(model::MedicationDispenseId{taskId8, 0});
    const auto medicationDispense8Content = medicationDispense8.serializeToJsonString();

    insertTask(task1, medicationDispense1Content);
    insertTask(task2);
    insertTask(task3);
    insertTask(task4, std::nullopt, healthCarePrescriptionBundle4);
    insertTask(task5, std::nullopt, healthCarePrescriptionBundle5);
    insertTask(task6);
    insertTask(task7);
    insertTask(task8, medicationDispense8Content, healthCarePrescriptionBundle8);
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
    taskPkv1.setReceiptUuid();
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
    const CadesBesSignature kbvBundlePkv1{certificate, privKey, kbvBundlePkv1Xml};
    auto healthCarePrescriptionBundlePkv1 = model::Binary(taskPkv1.healthCarePrescriptionUuid().value(), kbvBundlePkv1.getBase64());
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
    storeConsent(mDerivation.hashKvnr(consent.patientKvnr()), consent.dateTime(), db_model::ConsentCategory::CHARGCONS);
    // add static consent entry for pkvKvnr209
    const auto consent209 = model::Consent::fromJsonNoValidation(String::replaceAll(replaceKvnr(consentTemplate, pkvKvnr209), "##DATETIME##", dateTimeStr));
    storeConsent(mDerivation.hashKvnr(consent209.patientKvnr()), consent209.dateTime(), db_model::ConsentCategory::CHARGCONS);

    // add static chargeItem entry for kvnr1
    auto chargeItemInput = ResourceTemplates::chargeItemXml({
        .kvnr = model::Kvnr{pkvKvnr1, model::Kvnr::Type::pkv},
        .prescriptionId = pkvTaskId1,
        .dispenseBundleBase64 = "UEtWIGRpc3BlbnNlIGl0ZW0gYnVuZGxl",
        .operation = ResourceTemplates::ChargeItemOptions::OperationType::Put,
    });
    boost::replace_nth(chargeItemInput, R"(valueBoolean value="false")", 2, R"(valueBoolean value="true")");
    auto chargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemInput);
    const auto dispenseItemBundle1 =
        ::model::Bundle::fromXmlNoValidation(ResourceTemplates::davDispenseItemXml({.prescriptionId = pkvTaskId1}));

    chargeItem.setEntererTelematikId("606358757");
    chargeItem.setAccessCode(mockAccessCode);

    {
        const auto [key, optionalDerivedKeyData] =
            mDerivation.initialChargeItemKey(chargeItem.prescriptionId().value());

        storeChargeInformation(
            ::db_model::ChargeItem{
                ::model::ChargeInformation{
                    ::std::move(chargeItem), std::move(healthCarePrescriptionBundlePkv1),
                    ::model::Bundle::fromXmlNoValidation(kbvBundlePkv1Xml),
                    ::model::Binary{std::string{dispenseItemBundle1.getId()},
                                    ::CryptoHelper::toCadesBesSignature(dispenseItemBundle1.serializeToJsonString())},
                    ::model::AbgabedatenPkvBundle::fromJson(dispenseItemBundle1.jsonDocument()),
                    ::model::Bundle::fromJsonNoValidation(
                        receiptJson(taskPkv1, "3-SMC-B-Testkarte-883110000129070", kbvBundlePkv1))},
                optionalDerivedKeyData.blobId, ::db_model::Blob{optionalDerivedKeyData.salt}, key, mCodec},
            mDerivation.hashKvnr(model::Kvnr{std::string{pkvKvnr1}}));
    }

    // data needed for creation of ChargeItem:
    // Closed task for insurant with consent (added above):
    const auto pkvTaskId3 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50020);
    const auto taskPkv3 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Completed, .prescriptionId = pkvTaskId3, .kvnr = pkvKvnr1}));
    CadesBesSignature kbvBundlePkv3{certificate, privKey,
                                    ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId3, .kvnr = pkvKvnr1}),
                                    model::Timestamp::now()};
    const auto healthCarePrescriptionBundlePkv3 =
        model::Binary(taskPkv3.healthCarePrescriptionUuid().value(), kbvBundlePkv3.getBase64()).serializeToJsonString();
    const auto receiptPkv3 = receiptJson(taskPkv3, "3-SMC-B-Testkarte-883110000129070", kbvBundlePkv3);
    insertTask(taskPkv3, std::nullopt, healthCarePrescriptionBundlePkv3, receiptPkv3);
    // Closed task without consent:
    const auto pkvTaskId4 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50021);
    const auto taskPkv4 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Completed, .prescriptionId = pkvTaskId4, .kvnr = pkvKvnr1a}));
    insertTask(taskPkv4);

    // Closed task with consent but without charge item:
    const auto pkvTaskId5 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50022);
    const auto taskPkv5 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Completed, .prescriptionId = pkvTaskId5, .kvnr = pkvKvnr1}));
    const CadesBesSignature kbvBundlePkv5{certificate, privKey,
                                          ResourceTemplates::kbvBundleXml({
                                              .prescriptionId = pkvTaskId5,
                                              .kvnr = pkvKvnr1,
                                          })};
    const auto healthCarePrescriptionBundlePkv5 =
        model::Binary(taskPkv5.healthCarePrescriptionUuid().value(), kbvBundlePkv3.getBase64()).serializeToJsonString();
    const auto receipt5 = receiptJson(taskPkv5, "", kbvBundlePkv5);
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

    const auto dispenseItemBundle3 =
        ::model::Bundle::fromXmlNoValidation(ResourceTemplates::davDispenseItemXml({.prescriptionId = pkvTaskId3}));

    storeChargeInformation(
        ::db_model::ChargeItem{
            ::model::ChargeInformation{
                ::std::move(chargeItemForManip),
                ::model::Binary{taskPkv3.healthCarePrescriptionUuid().value(),
                                CryptoHelper::toCadesBesSignature(
                                    ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId3, .kvnr = pkvKvnr1}))},
                ::model::Bundle::fromXmlNoValidation(
                    ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId3, .kvnr = pkvKvnr1})),
                ::model::Binary{std::string{dispenseItemBundle3.getId()},
                                ::CryptoHelper::toCadesBesSignature(dispenseItemBundle3.serializeToXmlString())},
                ::model::AbgabedatenPkvBundle::fromJson(dispenseItemBundle3.jsonDocument()),
                ::model::Bundle::fromJsonNoValidation(receiptPkv3)},
            optionalData.blobId, ::db_model::Blob{optionalData.salt}, key, mCodec},
        mDerivation.hashKvnr(model::Kvnr{std::string{pkvKvnr1}}));

    // ERP-17321 Charge item signed with an old certification
    {
        const char* qesOldFilename = "test/tsl/X509Certificate/80276883531000000027-C.HP.QES.pem";
        const char* erp17321kvnr = "E173210118";
        const auto& [oldCert, oldKey] = certAndKey(qesOldFilename);
        const auto pkvTaskId6 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50023);
        const auto taskPkv6 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
            {.taskType = ResourceTemplates::TaskType::Completed, .prescriptionId = pkvTaskId6, .kvnr = erp17321kvnr}));
        const CadesBesSignature kbvBundlePkv6{oldCert, oldKey,
                                              ResourceTemplates::kbvBundleXml({
                                                  .prescriptionId = pkvTaskId6,
                                                  .kvnr = erp17321kvnr,
                                              })};
        const auto healthCarePrescriptionBundlePkv6 =
            model::Binary(taskPkv6.healthCarePrescriptionUuid().value(), kbvBundlePkv6.getBase64()).serializeToJsonString();
        const auto receipt6 = receiptJson(taskPkv6, "1-61b0d8d1dc0a583cdc0471e475aee79a60e9f57f", kbvBundlePkv6);
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
        chargeItemForManip.setMarkingFlags(model::ChargeItemMarkingFlags{true, std::nullopt, false});

        const auto [key, optionalData] = mDerivation.initialChargeItemKey(chargeItemForManip.prescriptionId().value());

        const auto dispenseItemBundle6 =
            ::model::Bundle::fromXmlNoValidation(ResourceTemplates::davDispenseItemXml({.prescriptionId = pkvTaskId6}));

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
                    ::model::Binary{std::string{dispenseItemBundle6.getId()},
                                    ::CryptoHelper::toCadesBesSignature(dispenseItemBundle6.serializeToXmlString(),
                                        qesOldFilename)},
                    ::model::AbgabedatenPkvBundle::fromJson(dispenseItemBundle6.jsonDocument()),
                    ::model::Bundle::fromJsonNoValidation(receipt6)},
                optionalData.blobId, ::db_model::Blob{optionalData.salt}, key, mCodec},
            mDerivation.hashKvnr(model::Kvnr{std::string{pkvKvnr1}}));

        // DIGA
        const auto taskIdDiga1 =
            model::PrescriptionId::fromDatabaseId(PrescriptionType::digitaleGesundheitsanwendungen, 6001);
        const auto taskDiga1 = Task::fromJsonNoValidation(ResourceTemplates::taskJson(
            {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = taskIdDiga1, .kvnr = kvnr}));
        insertTask(taskDiga1);
        const auto taskIdDiga2 = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::digitaleGesundheitsanwendungen, 6002);
        const auto taskDiga2 = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson(
            {.taskType = ResourceTemplates::TaskType::InProgress, .prescriptionId = taskIdDiga2,
             .timestamp = model::Timestamp::fromFhirDateTime("2021-02-02T16:18:43.036+00:00")}));
        const auto& kbvBundleDiga2 = CryptoHelper::toCadesBesSignature(ResourceTemplates::evdgaBundleXml({.prescriptionId = taskIdDiga2.toString()}));
        const auto prescriptionUuidDiga2 = taskDiga2.healthCarePrescriptionUuid().value();
        const auto prescriptionBundleDiga2 = model::Binary(prescriptionUuidDiga2, kbvBundleDiga2).serializeToJsonString();
        insertTask(taskDiga2, std::nullopt, prescriptionBundleDiga2);
    }

}

std::tuple<model::PrescriptionId, model::Timestamp> MockDatabase::createTask(model::PrescriptionType prescriptionType,
                                                                             model::Task::Status taskStatus,
                                                                             const model::Timestamp& lastUpdated,
                                                                             const model::Timestamp& created,
                                                                             const model::Timestamp& lastStatusUpdate)
{
    return mTasks.at(prescriptionType).createTask(prescriptionType, taskStatus, lastUpdated, created, lastStatusUpdate);
}

void MockDatabase::activateTask(const model::PrescriptionId& taskId,
                                const db_model::EncryptedBlob& encryptedKvnr,
                                const db_model::HashedKvnr& hashedKvnr,
                                model::Task::Status taskStatus,
                                const model::Timestamp& lastModified,
                                const model::Timestamp& expiryDate,
                                const model::Timestamp& acceptDate,
                                const db_model::EncryptedBlob& healthCareProviderPrescription,
                                const db_model::EncryptedBlob&,
                                const model::Timestamp& lastStatusUpdate,
                                bool euRedeemable)
{
    mTasks.at(taskId.type())
        .activateTask(taskId, encryptedKvnr, hashedKvnr, taskStatus, lastModified, expiryDate, acceptDate,
                      healthCareProviderPrescription, lastStatusUpdate, euRedeemable);
}

void MockDatabase::updateTaskReceipt(const model::PrescriptionId& taskId, const model::Task::Status& taskStatus,
                                     const model::Timestamp& lastModified, const db_model::EncryptedBlob& receipt,
                                     const db_model::EncryptedBlob&,
                                     const model::Timestamp& lastStatusUpdate)
{
    mTasks.at(taskId.type()).updateTaskReceipt(taskId, taskStatus, lastModified, receipt, lastStatusUpdate);
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
                                             const std::optional<db_model::EncryptedBlob>& owner,
                                             const model::Timestamp& lastStatusUpdate)
{
    mTasks.at(taskId.type())
        .updateTaskStatusAndSecret(taskId, taskStatus, lastModifiedDate, taskSecret, owner, lastStatusUpdate);
}

void MockDatabase::updateTaskMedicationDispense(const model::PrescriptionId& taskId,
                                                const model::Timestamp& lastModified,
                                                const model::Timestamp& lastMedicationDispense,
                                                const db_model::EncryptedBlob& medicationDispense,
                                                BlobId medicationDispenseBlobId,
                                                const db_model::HashedTelematikId& telematikId,
                                                const model::Timestamp& whenHandedOver,
                                                const std::optional<model::Timestamp>& whenPrepared,
                                                const db_model::Blob& medicationDispenseSalt,
                                                const std::optional<model::Task::Status>& taskStatus /* = std::nullopt */)
{
    mTasks.at(taskId.type())
        .updateTaskMedicationDispense(taskId, lastModified, lastMedicationDispense, medicationDispense, medicationDispenseBlobId, telematikId,
                                      whenHandedOver, whenPrepared, medicationDispenseSalt, taskStatus);
}

void MockDatabase::updateTaskMedicationDispenseReceipt(
    const model::PrescriptionId& taskId, const model::Task::Status& taskStatus, const model::Timestamp& lastModified,
    const db_model::EncryptedBlob& medicationDispense, BlobId medicationDispenseBlobId,
    const db_model::HashedTelematikId& telematicId, const model::Timestamp& whenHandedOver,
    const std::optional<model::Timestamp>& whenPrepared, const db_model::EncryptedBlob& taskReceipt,
    const model::Timestamp& lastMedicationDispense, const db_model::Blob& medicationDispenseSalt,
    const db_model::EncryptedBlob&,
    const model::Timestamp& lastStatusUpdate)
{
    mTasks.at(taskId.type())
        .updateTaskMedicationDispenseReceipt(
            taskId, taskStatus, lastModified, medicationDispense, medicationDispenseBlobId, telematicId, whenHandedOver,
            whenPrepared, taskReceipt, lastMedicationDispense, medicationDispenseSalt, lastStatusUpdate);
}

void MockDatabase::updateTaskDeleteMedicationDispense(const model::PrescriptionId& taskId, const model::Timestamp& lastModified)
{
    mTasks.at(taskId.type()).updateTaskDeleteMedicationDispense(taskId, lastModified);
}

void MockDatabase::updateTaskClearPersonalData(const model::PrescriptionId& taskId, model::Task::Status taskStatus,
                                               const model::Timestamp& lastModified,
                                               const model::Timestamp& lastStatusUpdate)
{
    mTasks.at(taskId.type()).updateTaskClearPersonalData(taskId, taskStatus, lastModified, lastStatusUpdate);
}

void MockDatabase::updateTaskEuRedeemableByPatient(const model::PrescriptionId& taskId,
                                                   bool redeemable,
                                                   const model::Timestamp& lastModified)
{
    mTasks.at(taskId.type()).updateTaskEuRedeemableByPatient(taskId, redeemable, lastModified);
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

void MockDatabase::storeConsent(const db_model::HashedKvnr& kvnr, const model::Timestamp& creationTime, db_model::ConsentCategory category)
{
    mConsents.storeConsent(kvnr, creationTime, category);
}

std::vector<db_model::Consent> MockDatabase::retrieveAllConsents(const db_model::HashedKvnr& kvnr,
                                                                 const UrlArguments& searchArguments)
{
    return TestUrlArguments(searchArguments).apply(mConsents.all(kvnr));
}

bool MockDatabase::clearConsent(const db_model::HashedKvnr& kvnr, db_model::ConsentCategory category)
{
    return mConsents.clearConsent(kvnr, category);
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
