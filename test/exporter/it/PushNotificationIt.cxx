/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "erp/database/push/PushDbModel.hxx"
#include "exporter/RunLoopScheduler.hxx"
#include "exporter/eventprocessing/push/PushEventProcessor.hxx"
#include "exporter/pc/MedicationExporterFactories.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "shared/model/push/EncryptionKey.hxx"
#include "shared/util/Hash.hxx"
#include "test/exporter/database/PostgresDatabaseTest.hxx"
#include "test/exporter/util/MedicationExporterStaticData.hxx"

#include <gtest/gtest.h>

class PushNotificationIt : public PostgresDatabaseTest
{
public:
    PushNotificationIt()
    {
        runLoop.getThreadPool().setUp(1, "medication-exporter-io");
        auto& ioContext = runLoop.getThreadPool().ioContext();
        auto fact = MedicationExporterStaticData::makeMockMedicationExporterFactories();
        serviceContext = std::make_shared<MedicationExporterServiceContext>(ioContext, Configuration::instance(), fact);
        kvnrHashed = serviceContext->getKeyDerivation().hashKvnr(kvnr);
    }
    void SetUp() override
    {
        if (! TestConfiguration::instance().getBoolValue(TestConfigurationKey::TEST_USE_POSTGRES) ||
            ! TestConfiguration::instance().getBoolValue(TestConfigurationKey::TEST_USE_PUSH_GATEWAY_CLIENT))
        {
            GTEST_SKIP();
        }
        cleanup();
    }
    void TearDown() override
    {
        if (! TestConfiguration::instance().getBoolValue(TestConfigurationKey::TEST_USE_POSTGRES))
        {
            return;
        }
        cleanup();
    }
    // keyIdentifier is the key for selection of a specific mock-template, although it is named pushKey in lz-config.
    // e.g. "pushKey": "rejected-200",
    void insertAppRegistration(std::string keyIdentifier)
    {
        auto erpDbTransaction = createErpDbTransaction();
        const auto hashedPushKey = db_model::HashedId::fromString(pushKey);
        const auto hashedAppId = db_model::HashedId::fromString(appId);
        auto [key, keyData] =
            serviceContext->getKeyDerivation().initialAppRegistrationKey(hashedPushKey, kvnrHashed);
        model::Pusher pusher{
            model::PushKey{pushKey},
            model::AppId{appId},
            "app-display-name",
            "device-display-name",
            model::Lang{"de"},
            model::PusherData{"https://localhost:19443/push/v1/", std::nullopt},
            model::Encryption{"2026-01", SafeString{"32-bytes-iss-1111111111111111111"}, keyIdentifier}};
        model::EncryptionKey encryptionKey =
            model::EncryptionKey::deriveMonthlyKey(pusher.encryption().iss, pusher.encryption().timeIssCreated);
        auto payload = mCodec.encode(pusher.dbPayloadJson(), key, Compression::DictionaryUse::Default_json);
        auto encryptedEncryptionKey =
            mCodec.encode(encryptionKey.serializeToCsv(keyIdentifier), key, Compression::DictionaryUse::Default_json);
        erpDbTransaction.exec(
            "INSERT INTO erp.app_registrations (pushkey_hashed, app_id_hashed, kvnr_hashed, blob_id, salt, payload, url, "
            "encryption_key, subscribed_channel, time_created, last_modified) VALUES ($1, $2, $3, $4, "
            "$5, $6, $7, $8, $9::erp.push_notification_channel[], $10, $10)",
            pqxx::params{hashedPushKey.binarystring(), hashedAppId.binarystring(), kvnrHashed.binarystring(), keyData.blobId, db_model::Blob{keyData.salt},
                         payload, pusher.pusherData().mUrl, encryptedEncryptionKey, //pusher.encryption().keyIdentifier,
                         "{erp.task.activate}", model::Timestamp::now().toXsDateTime()});
        erpDbTransaction.commit();
    }

    RunLoopScheduler runLoop;
    std::shared_ptr<MedicationExporterServiceContext> serviceContext;
    std::string pushKey{"Xp/MzCt8/9DcSNE9cuiaoT5Ac55job3TdLSSmtmYl4A="};
    std::string appId{"app-id"};
    model::Kvnr kvnr{"X123456789"};
    db_model::HashedKvnr kvnrHashed;
};

TEST_F(PushNotificationIt, goodcase)
{
    insertAppRegistration("keyidentifier");
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 1);
    auto eventId = insertPushEvent(kvnr, prescriptionId, "erp.task.activate", Uuid{});

    runLoop.serve(serviceContext, &PushEventProcessor::runloopWorker, 1);

    testutils::waitFor([eventId, this]() {
        auto&& txn = createTransaction();
        auto result = txn.exec("SELECT * FROM erp_event.push_notification_event WHERE id = $1", pqxx::params{eventId});
        txn.commit();
        // event has to be deleted due to successful processing
        return result.empty();
    });
    runLoop.shutDown();
    runLoop.getThreadPool().joinAllThreads();
}

TEST_F(PushNotificationIt, rejectedEventDeleted)
{
    // when the app-registration is rejected the corresponding event should be deleted
    // pushKey rejected-200
    insertAppRegistration("rejected-200");
    auto prescriptionId =
    model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 1);
    auto eventId = insertPushEvent(kvnr, prescriptionId, "erp.task.activate", Uuid{});

    runLoop.serve(serviceContext, &PushEventProcessor::runloopWorker, 1);

    testutils::waitFor([eventId, this]() {
        auto&& txn = createTransaction();
        auto result = txn.exec("SELECT * FROM erp_event.push_notification_event WHERE id = $1", pqxx::params{eventId});
        txn.commit();
        // event has to be deleted due to rejected app-registration
        return result.empty();
    });
    runLoop.shutDown();
    runLoop.getThreadPool().joinAllThreads();
}

TEST_F(PushNotificationIt, noRegistrationEventDeleted)
{
    // when the app-registration is not existing the corresponding event should be deleted
    auto prescriptionId =
    model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 1);
    auto eventId = insertPushEvent(kvnr, prescriptionId, "erp.task.activate", Uuid{});

    runLoop.serve(serviceContext, &PushEventProcessor::runloopWorker, 1);

    testutils::waitFor([eventId, this]() {
        auto&& txn = createTransaction();
        auto result = txn.exec("SELECT * FROM erp_event.push_notification_event WHERE id = $1", pqxx::params{eventId});
        txn.commit();
        // event has to be deleted due to missing app-registration
        return result.empty();
    });
    runLoop.shutDown();
    runLoop.getThreadPool().joinAllThreads();
}
