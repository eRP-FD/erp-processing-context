/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/database/CommitGuard.hxx"
#include "exporter/EventProcessor.hxx"
#include "exporter/MedicationExporterMain.hxx"
#include "exporter/database/MedicationExporterDatabaseFrontend.hxx"
#include "exporter/model/EventKvnr.hxx"
#include "exporter/pc/MedicationExporterFactories.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "gtest/gtest.h"
#include "mock/crypto/MockCryptography.hxx"
#include "shared/util/Configuration.hxx"
#include "test/exporter/database/PostgresDatabaseTest.hxx"


class EventProcessorTest : public PostgresDatabaseTest
{
};

TEST_F(EventProcessorTest, process)
{
    model::Kvnr kvnr{"X000000012"};

    {// verify: no events in DB
        auto transaction = createTransaction();
        const auto results = transaction.exec_params(
            "SELECT prescription_id, state FROM erp_event.task_event ORDER BY last_modified ASC");
        transaction.commit();
        ASSERT_EQ(results.size(), 0);
    }

    const auto healthcareProviderPrescription = ResourceTemplates::kbvBundleXml();
    const auto medicationDispensePrescription = ResourceTemplates::medicationDispenseBundleXml();
    const auto medicationDispenseBundle = ::model::Bundle::fromXmlNoValidation(medicationDispensePrescription);

    JwtBuilder jwtBuilder(MockCryptography::getIdpPrivateKey());
    insertTaskKvnr(kvnr);
    const auto prescriptionIdDeadLetter = model::PrescriptionId::fromString("160.000.100.000.001.05");
    insertTaskEvent(kvnr, prescriptionIdDeadLetter.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::deadLetterQueue, healthcareProviderPrescription,
                    medicationDispenseBundle.serializeToJsonString(), mDoctorIdentity, std::nullopt);
    insertTaskEvent(kvnr, prescriptionIdDeadLetter.toString(), model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, healthcareProviderPrescription,
                    medicationDispenseBundle.serializeToJsonString(), mDoctorIdentity, mPharmacyIdentity);

    const auto prescriptionIdPending = model::PrescriptionId::fromString("160.000.100.000.002.02");
    insertTaskEvent(kvnr, prescriptionIdPending.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, healthcareProviderPrescription,
                    medicationDispenseBundle.serializeToJsonString(), mDoctorIdentity, std::nullopt);
    insertTaskEvent(kvnr, prescriptionIdPending.toString(), model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, healthcareProviderPrescription,
                    medicationDispenseBundle.serializeToJsonString(), mDoctorIdentity, mPharmacyIdentity);

    {
        MedicationExporterPostgresBackend db;
        auto result = db.getAllEventsForKvnr(
            model::EventKvnr(kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::processing));
        EXPECT_EQ(result.size(), 3);
    }

    auto ioThreadPool = ThreadPool();

    ioThreadPool.setUp(1, "medication-exporter-io");
    auto& ioContext = ioThreadPool.ioContext();
    auto serviceContext = std::make_shared<MedicationExporterServiceContext>(
        ioContext, Configuration::instance(), MedicationExporterMain::createProductionFactories());
    MedicationExporterMain::RunLoop runLoop;
    runLoop.getThreadPool().setUp(1, "threadpool runner");
    co_spawn(runLoop.getThreadPool().ioContext(), setupEpaClientPool(*serviceContext), boost::asio::use_future).get();

    EventProcessor eventProcessor(serviceContext);
    bool processed = eventProcessor.process();
    EXPECT_EQ(processed, true);

    {// verify: events from prescription that with deadletterQueue events marked deadletterQueue and still in DB
        auto transaction = createTransaction();
        const auto results = transaction.exec_params(
            "SELECT prescription_id, state FROM erp_event.task_event ORDER BY last_modified ASC");
        ASSERT_EQ(results.size(), 2);
        EXPECT_EQ(results[0].at(0).as<std::int64_t>(), prescriptionIdDeadLetter.toDatabaseId());
        EXPECT_EQ(results[0].at(1).as<std::string>(), "deadLetterQueue");
        EXPECT_EQ(results[1].at(0).as<std::int64_t>(), prescriptionIdDeadLetter.toDatabaseId());
        EXPECT_EQ(results[1].at(1).as<std::string>(), "deadLetterQueue");
        transaction.commit();
    }

    model::Kvnr kvnrWithoutEvents{"X000000013"};
    insertTaskKvnr(kvnrWithoutEvents);
    model::EventKvnr eventKvnrWithoutEvents(kvnrHashed(kvnr), std::nullopt, std::nullopt,
                                            model::EventKvnr::State::processing);
    processed = eventProcessor.process();

    {// verify: events from prescription that with deadletterQueue events marked deadletterQueue and still in DB
        auto transaction = createTransaction();
        const auto results = transaction.exec_params("SELECT state FROM erp_event.kvnr WHERE kvnr_hashed = $1",
                                                     eventKvnrWithoutEvents.kvnrHashed());
        ASSERT_EQ(results.size(), 1);
        EXPECT_EQ(magic_enum::enum_cast<model::EventKvnr::State>(results.at(0, 0).as<std::string>()).value(),
                  model::EventKvnr::State::processed);
        transaction.commit();
    }
}

TEST_F(EventProcessorTest, processOne)
{
    model::Kvnr kvnr{"X000000012"};
    model::EventKvnr eventKvnr(kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::processing);

    const auto healthcareProviderPrescription = ResourceTemplates::kbvBundleXml();
    const auto medicationDispensePrescription = ResourceTemplates::medicationDispenseBundleXml();
    const auto medicationDispenseBundle = ::model::Bundle::fromXmlNoValidation(medicationDispensePrescription);

    JwtBuilder jwtBuilder(MockCryptography::getIdpPrivateKey());
    insertTaskKvnr(kvnr);
    const auto prescriptionIdDeadLetter = model::PrescriptionId::fromString("160.000.100.000.001.05");
    insertTaskEvent(kvnr, prescriptionIdDeadLetter.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, healthcareProviderPrescription,
                    medicationDispenseBundle.serializeToJsonString(), mDoctorIdentity, std::nullopt);
    insertTaskEvent(kvnr, prescriptionIdDeadLetter.toString(), model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, healthcareProviderPrescription,
                    medicationDispenseBundle.serializeToJsonString(), mDoctorIdentity, mPharmacyIdentity);
    auto ioThreadPool = ThreadPool();

    ioThreadPool.setUp(1, "medication-exporter-io");
    auto& ioContext = ioThreadPool.ioContext();
    auto serviceContext = std::make_shared<MedicationExporterServiceContext>(
        ioContext, Configuration::instance(), MedicationExporterMain::createProductionFactories());
    MedicationExporterMain::RunLoop runLoop;
    runLoop.getThreadPool().setUp(1, "threadpool runner");
    co_spawn(runLoop.getThreadPool().ioContext(), setupEpaClientPool(*serviceContext), boost::asio::use_future).get();

    EventProcessor eventProcessor(serviceContext);
    eventProcessor.processOne(eventKvnr);

    {// verify: events removed, kvnr processed
        auto transaction = createTransaction();
        {
            const auto results = transaction.exec_params("SELECT * FROM erp_event.task_event WHERE kvnr_hashed = $1",
                                                         eventKvnr.kvnrHashed());
            EXPECT_EQ(results.size(), 0);
        }
        {
            const auto results = transaction.exec_params("SELECT state FROM erp_event.kvnr WHERE kvnr_hashed = $1",
                                                         eventKvnr.kvnrHashed());
            EXPECT_EQ(magic_enum::enum_cast<model::EventKvnr::State>(results.at(0, 0).as<std::string>()).value(),
                      model::EventKvnr::State::processed);
        }
        transaction.commit();
    }
}

TEST_F(EventProcessorTest, processOne_retry)
{
    GTEST_SKIP() << "Skipped. X011999971 must be configured as 409/Conflict to run this test successfully.";
    model::Kvnr kvnr{"X011999971"};
    model::EventKvnr eventKvnr(kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::processing);

    const auto healthcareProviderPrescription = ResourceTemplates::kbvBundleXml();
    const auto medicationDispensePrescription = ResourceTemplates::medicationDispenseBundleXml();
    const auto medicationDispenseBundle = ::model::Bundle::fromXmlNoValidation(medicationDispensePrescription);

    JwtBuilder jwtBuilder(MockCryptography::getIdpPrivateKey());
    insertTaskKvnr(kvnr);
    const auto prescriptionIdDeadLetter = model::PrescriptionId::fromString("160.000.100.000.001.05");
    insertTaskEvent(kvnr, prescriptionIdDeadLetter.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, healthcareProviderPrescription,
                    medicationDispenseBundle.serializeToJsonString(), mDoctorIdentity, std::nullopt);
    insertTaskEvent(kvnr, prescriptionIdDeadLetter.toString(), model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, healthcareProviderPrescription,
                    medicationDispenseBundle.serializeToJsonString(), mDoctorIdentity, mPharmacyIdentity);
    {
        auto transaction = createTransaction();
        const auto results = transaction.exec_params("SELECT prescription_id, prescription_type FROM erp_event.task_event WHERE kvnr_hashed = $1",
                                                     eventKvnr.kvnrHashed());
        ASSERT_EQ(results.size(), 2);
    }
    ThreadPool ioThreadPool;

    ioThreadPool.setUp(1, "medication-exporter-io");
    auto& ioContext = ioThreadPool.ioContext();
    auto serviceContext = std::make_shared<MedicationExporterServiceContext>(
        ioContext, Configuration::instance(), MedicationExporterMain::createProductionFactories());
    MedicationExporterMain::RunLoop runLoop;
    runLoop.getThreadPool().setUp(1, "threadpool runner");
    co_spawn(runLoop.getThreadPool().ioContext(), setupEpaClientPool(*serviceContext), boost::asio::use_future).get();

    EventProcessor eventProcessor(serviceContext);
    eventProcessor.processOne(eventKvnr);

    {// verify: events removed, kvnr processed
        auto transaction = createTransaction();
        {
            const auto results = transaction.exec_params("SELECT prescription_id, usecase FROM erp_event.task_event WHERE kvnr_hashed = $1",
                                                         eventKvnr.kvnrHashed());
            ASSERT_EQ(results.size(), 1);
            EXPECT_EQ(results.at(0, 0).as<std::int64_t>(),model::PrescriptionId::fromString("160.000.100.000.001.05").toDatabaseId());
            EXPECT_EQ(magic_enum::enum_cast<model::TaskEvent::UseCase>(results.at(0,1).as<std::string>()), model::TaskEvent::UseCase::provideDispensation);
        }
        {
            const auto results = transaction.exec_params("SELECT state FROM erp_event.kvnr WHERE kvnr_hashed = $1",
                                                         eventKvnr.kvnrHashed());
            EXPECT_EQ(magic_enum::enum_cast<model::EventKvnr::State>(results.at(0, 0).as<std::string>()).value(),
                      model::EventKvnr::State::pending);
        }
        transaction.commit();
    }
}


TEST_F(EventProcessorTest, processEpaConflict)
{
    model::Kvnr kvnr{"X000000012"};
    model::EventKvnr eventKvnr(kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::processing);

    const auto healthcareProviderPrescription = ResourceTemplates::kbvBundleXml();
    const auto medicationDispensePrescription = ResourceTemplates::medicationDispenseBundleXml();
    const auto medicationDispenseBundle = ::model::Bundle::fromXmlNoValidation(medicationDispensePrescription);

    JwtBuilder jwtBuilder(MockCryptography::getIdpPrivateKey());
    insertTaskKvnr(kvnr);

    auto next_export_before = model::Timestamp::now();
    auto next_export_after = model::Timestamp::now();
    {
        auto transaction = createTransaction();
        const auto results =
            transaction.exec_params("SELECT kvnr, EXTRACT(EPOCH FROM next_export) FROM erp_event.kvnr");
        transaction.commit();
        next_export_before = model::Timestamp(results.at(0).at(1).as<double>());
    }

    auto ioThreadPool = ThreadPool();
    // ioThreadPool.setUp(1, "medication-exporter-io");
    auto& ioContext = ioThreadPool.ioContext();
    auto serviceContext = std::make_shared<MedicationExporterServiceContext>(
        ioContext, Configuration::instance(), MedicationExporterMain::createProductionFactories());
    EventProcessor eventProcessor(serviceContext);
    eventProcessor.processEpaConflict(eventKvnr);

    {
        auto transaction = createTransaction();
        const auto results =
            transaction.exec_params("SELECT kvnr, EXTRACT(EPOCH FROM next_export) FROM erp_event.kvnr");
        transaction.commit();
        next_export_after = model::Timestamp(results.at(0).at(1).as<double>());
    }
    EXPECT_TRUE(next_export_before + 24h <= next_export_after && next_export_after <= next_export_before + 24h + 60min);
}


TEST_F(EventProcessorTest, processEpaDeniedOrNotFound)
{
    const auto healthcareProviderPrescription = ResourceTemplates::kbvBundleXml();
    const auto medicationDispensePrescription = ResourceTemplates::medicationDispenseBundleXml();
    const auto medicationDispenseBundle = ::model::Bundle::fromXmlNoValidation(medicationDispensePrescription);

    JwtBuilder jwtBuilder(MockCryptography::getIdpPrivateKey());
    model::Kvnr kvnr{"X000000012"};
    model::EventKvnr eventKvnr(kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::processing);
    insertTaskKvnr(kvnr);
    const auto prescriptionIdDeadLetter = model::PrescriptionId::fromString("160.000.100.000.001.05");
    insertTaskEvent(kvnr, prescriptionIdDeadLetter.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, healthcareProviderPrescription,
                    medicationDispenseBundle.serializeToJsonString(), mDoctorIdentity, std::nullopt);
    insertTaskEvent(kvnr, prescriptionIdDeadLetter.toString(), model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, healthcareProviderPrescription,
                    medicationDispenseBundle.serializeToJsonString(), mDoctorIdentity, mPharmacyIdentity);

    const auto prescriptionIdPending = model::PrescriptionId::fromString("160.000.100.000.002.02");
    insertTaskEvent(kvnr, prescriptionIdPending.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, healthcareProviderPrescription,
                    medicationDispenseBundle.serializeToJsonString(), mDoctorIdentity, std::nullopt);
    insertTaskEvent(kvnr, prescriptionIdPending.toString(), model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, healthcareProviderPrescription,
                    medicationDispenseBundle.serializeToJsonString(), mDoctorIdentity, mPharmacyIdentity);

    auto ioThreadPool = ThreadPool();
    // ioThreadPool.setUp(1, "medication-exporter-io");
    auto& ioContext = ioThreadPool.ioContext();
    auto serviceContext = std::make_shared<MedicationExporterServiceContext>(
        ioContext, Configuration::instance(), MedicationExporterMain::createProductionFactories());
    EventProcessor eventProcessor(serviceContext);

    eventProcessor.processEpaDeniedOrNotFound(eventKvnr);

    {
        auto transaction = createTransaction();
        const auto resultTaskEvents = transaction.exec_params(
            "SELECT count(*) FROM erp_event.task_event WHERE kvnr_hashed = $1", eventKvnr.kvnrHashed());
        EXPECT_EQ(resultTaskEvents.at(0, 0).as<int>(), 0);
        const auto resultKvnrs =
            transaction.exec_params("SELECT state FROM erp_event.kvnr WHERE kvnr_hashed = $1", eventKvnr.kvnrHashed());
        EXPECT_EQ(resultKvnrs.at(0, 0).as<std::string>(), "processed");
        transaction.commit();
    }
}

TEST_F(EventProcessorTest, processEpaUnkown)
{
    const auto healthcareProviderPrescription = ResourceTemplates::kbvBundleXml();
    const auto medicationDispensePrescription = ResourceTemplates::medicationDispenseBundleXml();
    const auto medicationDispenseBundle = ::model::Bundle::fromXmlNoValidation(medicationDispensePrescription);

    JwtBuilder jwtBuilder(MockCryptography::getIdpPrivateKey());
    model::Kvnr kvnr{"X000000012"};
    model::EventKvnr eventKvnr(kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::processing);
    insertTaskKvnr(kvnr);
    const auto prescriptionIdDeadLetter = model::PrescriptionId::fromString("160.000.100.000.001.05");
    insertTaskEvent(kvnr, prescriptionIdDeadLetter.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, healthcareProviderPrescription,
                    medicationDispenseBundle.serializeToJsonString(), mDoctorIdentity, std::nullopt);
    insertTaskEvent(kvnr, prescriptionIdDeadLetter.toString(), model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, healthcareProviderPrescription,
                    medicationDispenseBundle.serializeToJsonString(), mDoctorIdentity, mPharmacyIdentity);

    const auto prescriptionIdPending = model::PrescriptionId::fromString("160.000.100.000.002.02");
    insertTaskEvent(kvnr, prescriptionIdPending.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, healthcareProviderPrescription,
                    medicationDispenseBundle.serializeToJsonString(), mDoctorIdentity, std::nullopt);
    insertTaskEvent(kvnr, prescriptionIdPending.toString(), model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, healthcareProviderPrescription,
                    medicationDispenseBundle.serializeToJsonString(), mDoctorIdentity, mPharmacyIdentity);

    auto ioThreadPool = ThreadPool();
    // ioThreadPool.setUp(1, "medication-exporter-io");
    auto& ioContext = ioThreadPool.ioContext();
    auto serviceContext = std::make_shared<MedicationExporterServiceContext>(
        ioContext, Configuration::instance(), MedicationExporterMain::createProductionFactories());
    EventProcessor eventProcessor(serviceContext);

    eventProcessor.processEpaUnknown(eventKvnr);

    {
        // MedicationExporterPostgresBackend db;
        const auto events = database().getAllEventsForKvnr(
            model::EventKvnr(kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::processing));
        ASSERT_EQ(events.size(), 4);
        EXPECT_EQ(events[0]->getState(), model::TaskEvent::State::pending);
        database().commitTransaction();
    }
}
