/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/EventProcessor.hxx"
#include "exporter/MedicationExporterMain.hxx"
#include "exporter/database/CommitGuard.hxx"
#include "exporter/database/MedicationExporterDatabaseFrontend.hxx"
#include "exporter/model/EventKvnr.hxx"
#include "exporter/pc/MedicationExporterFactories.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "gtest/gtest.h"
#include "mock/crypto/MockCryptography.hxx"
#include "shared/util/Configuration.hxx"
#include "test/exporter/database/PostgresDatabaseTest.hxx"

#include <memory>

/**
 * Tests that a given timestamp is within the range of another timestamp plus some added seconds.
 * Math: after in [before, before + expectedPlus + maxAdded]
 *
 * @param after Timestamp with to be in the desired range
 * @param before Timestamp defining the start of the desired range (and its end with the added seconds)
 * @param expectedPlus seconds added to the end of the range
 * @param maxAdded add. seconds added to the end of the range
 */
testing::AssertionResult equalsPlus(model::Timestamp after, model::Timestamp before, std::chrono::seconds expectedPlus,
                                    std::chrono::seconds maxAdded)
{
    if (before + expectedPlus <= after && after <= before + expectedPlus + maxAdded)
    {
        return testing::AssertionSuccess();
    }
    return testing::AssertionFailure() << "Time " << after << " is not in the range of " << (before + expectedPlus)
                                       << "-" << (before + expectedPlus + maxAdded);
}


class EventProcessorTest : public PostgresDatabaseTest
{

public:
    const std::string healthcareProviderPrescription = ResourceTemplates::kbvBundleXml();
    const std::string medicationDispenseBundle =
        ::model::Bundle::fromXmlNoValidation(ResourceTemplates::medicationDispenseBundleXml()).serializeToJsonString();
    JwtBuilder jwtBuilder = JwtBuilder(MockCryptography::getIdpPrivateKey());


    void SetUp() override
    {
        PostgresDatabaseTest::SetUp();

        /*
         * Requirements to execute the different EventProcessorTest's tests successfully:
         * - X011999971 must be configured as 409/Conflict on provideDispensation
         * - X011300011 must be configured to have a 5s delay on providePrescription (processOne_epa_timeout_retry_first_event)
         * - X011300012 must be configured to have a 5s delay on providePrescription and provideDispensation (processOne_epa_timeout_retry and processOne_epa_timeout_retry_two_events)
         * - X011300013 must be configured to have a 5s delay provideDispensation (processOne_epa_timeout_retry_second_event)
         * - X011300021 must be configured to be responded with template: prescription- and dispensationoutput_MEDICATIONSVC_NO_VALID_STRUCTURE.json for providePrescription and -Dispensation
         * The required config for the epa-test-mock is given in test/ePATestMock/lzcfg.yaml.
         */
        bool preparedEpaForTests = false;
        if (! preparedEpaForTests)
        {
            GTEST_SKIP() << "EventProcessorTest skipped. Unskip if KVNRs are prepared to run the tests successfully.";
        }
    }

    static inline ThreadPool ioThreadPool;
    static inline std::shared_ptr<MedicationExporterServiceContext> serviceContext;
    static inline MedicationExporterMain::RunLoop runLoop;

    static const inline model::PrescriptionId prescriptionId1 =
        model::PrescriptionId::fromString("160.000.100.000.001.05");
    static const inline model::PrescriptionId prescriptionId2 =
        model::PrescriptionId::fromString("160.000.100.000.002.02");


    static void SetUpTestSuite()
    {

        if (! usePostgres())
        {
            GTEST_SKIP() << "Skipped. Postgres DB needed for this test suite";
        }
        ioThreadPool.setUp(1, "medication-exporter-io");
        serviceContext = std::make_shared<MedicationExporterServiceContext>(
            ioThreadPool.ioContext(), Configuration::instance(), MedicationExporterMain::createProductionFactories());
        runLoop.getThreadPool().setUp(1, "threadpool runner");
        co_spawn(runLoop.getThreadPool().ioContext(), setupEpaClientPool(*serviceContext), boost::asio::use_future)
            .get();
    }

    static void TearDownTestSuite()
    {
        if (usePostgres())
        {
            runLoop.shutDown();
            serviceContext.reset();
            ioThreadPool.shutDown();
        }
    }
};

TEST_F(EventProcessorTest, process)
{
    model::Kvnr kvnr{"X000000012"};
    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, prescriptionId1.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::deadLetterQueue, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, std::nullopt);
    insertTaskEvent(kvnr, prescriptionId1.toString(), model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, mPharmacyIdentity);

    insertTaskEvent(kvnr, prescriptionId2.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, std::nullopt);
    insertTaskEvent(kvnr, prescriptionId2.toString(), model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, mPharmacyIdentity);

    EventProcessor eventProcessor(serviceContext);
    bool processed = eventProcessor.process();
    EXPECT_EQ(processed, true);

    {// verify: events from prescription that with deadletterQueue events marked deadletterQueue and still in DB
        auto transaction = createTransaction();
        const auto results = transaction.exec_params(
            "SELECT prescription_id, state FROM erp_event.task_event ORDER BY last_modified ASC");
        ASSERT_EQ(results.size(), 2);
        EXPECT_EQ(results[0].at(0).as<std::int64_t>(), prescriptionId1.toDatabaseId());
        EXPECT_EQ(results[0].at(1).as<std::string>(), "deadLetterQueue");
        EXPECT_EQ(results[1].at(0).as<std::int64_t>(), prescriptionId1.toDatabaseId());
        EXPECT_EQ(results[1].at(1).as<std::string>(), "deadLetterQueue");
        transaction.commit();
    }

    model::Kvnr kvnrWithoutEvents{"X000000013"};
    model::EventKvnr eventKvnrWithoutEvents(kvnrHashed(kvnr), std::nullopt, std::nullopt,
                                            model::EventKvnr::State::processing, 0);
    insertTaskKvnr(kvnrWithoutEvents);

    processed = eventProcessor.process();
    EXPECT_EQ(processed, true);

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
    model::EventKvnr eventKvnr(kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0);
    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, prescriptionId1.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, std::nullopt);
    insertTaskEvent(kvnr, prescriptionId1.toString(), model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, mPharmacyIdentity);

    EventProcessor eventProcessor(serviceContext);
    eventProcessor.processOne(eventKvnr);

    {// verify: events removed, kvnr processed
        auto transaction = createTransaction();
        {
            const auto results = transaction.exec_params(
                "SELECT COUNT(*) FROM erp_event.task_event WHERE kvnr_hashed = $1", eventKvnr.kvnrHashed());
            EXPECT_EQ(results.at(0, 0).as<std::int64_t>(), 0);
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

TEST_F(EventProcessorTest, processOne_jsonlog_failure_providePrescription)
{
    model::Kvnr kvnr{"X011300021"};
    model::EventKvnr eventKvnr(kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0);
    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, prescriptionId1.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, std::nullopt);

    EventProcessor eventProcessor(serviceContext);
    eventProcessor.processOne(eventKvnr);
    // unfortunately there currently is no automated way to check for the required log message
}

TEST_F(EventProcessorTest, processOne_jsonlog_failure_provideDispensation)
{
    model::Kvnr kvnr{"X011300021"};
    model::EventKvnr eventKvnr(kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0);
    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, prescriptionId1.toString(), model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, mPharmacyIdentity);

    EventProcessor eventProcessor(serviceContext);
    eventProcessor.processOne(eventKvnr);
    // unfortunately there currently is no automated way to check for the required log message
}

TEST_F(EventProcessorTest, calculateExponentialBackoffDelay)
{
    EXPECT_EQ(EventProcessor::calculateExponentialBackoffDelay(-1), std::chrono::minutes(1));
    EXPECT_EQ(EventProcessor::calculateExponentialBackoffDelay(0), std::chrono::minutes(1));
    EXPECT_EQ(EventProcessor::calculateExponentialBackoffDelay(1), std::chrono::minutes(3));
    EXPECT_EQ(EventProcessor::calculateExponentialBackoffDelay(2), std::chrono::minutes(9));
    EXPECT_EQ(EventProcessor::calculateExponentialBackoffDelay(3), std::chrono::minutes(27));
    EXPECT_EQ(EventProcessor::calculateExponentialBackoffDelay(4), std::chrono::minutes(81)); // 1:21 hours
    EXPECT_EQ(EventProcessor::calculateExponentialBackoffDelay(5), std::chrono::minutes(243)); // 4:03 hours
    EXPECT_EQ(EventProcessor::calculateExponentialBackoffDelay(6), std::chrono::minutes(729)); // 12:09 hours
    EXPECT_EQ(EventProcessor::calculateExponentialBackoffDelay(7), std::chrono::minutes(2187)); // 1,5 days
    EXPECT_EQ(EventProcessor::calculateExponentialBackoffDelay(10), std::chrono::minutes(59049)); // > 41 days
}

TEST_F(EventProcessorTest, processOne_retry)
{
    model::Kvnr kvnr{"X011999971"};
    model::EventKvnr eventKvnr(kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0);
    insertTaskKvnr(kvnr);

    insertTaskEvent(kvnr, prescriptionId1.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, std::nullopt);
    insertTaskEvent(kvnr, prescriptionId1.toString(), model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, mPharmacyIdentity);

    EventProcessor eventProcessor(serviceContext);

    const auto next_export_before = model::Timestamp::now();
    eventProcessor.processOne(eventKvnr);

    {// verify: provideDescription event removed, provideDispensation not removed, kvnr pending
        auto transaction = createTransaction();
        {
            const auto results =
                transaction.exec_params("SELECT prescription_id, usecase, state FROM erp_event.task_event WHERE "
                                        "kvnr_hashed = $1 ORDER BY prescription_id ASC, last_modified ASC",
                                        eventKvnr.kvnrHashed());
            ASSERT_EQ(results.size(), 1);
            EXPECT_EQ(results.at(0, 0).as<std::int64_t>(), prescriptionId1.toDatabaseId());
            EXPECT_EQ(magic_enum::enum_cast<model::TaskEvent::UseCase>(results.at(0, 1).as<std::string>()),
                      model::TaskEvent::UseCase::provideDispensation);
            EXPECT_EQ(magic_enum::enum_cast<model::TaskEvent::State>(results.at(0, 2).as<std::string>()).value(),
                      model::TaskEvent::State::pending);
        }
        {
            const auto results = transaction.exec_params("SELECT state, retry_count, EXTRACT(EPOCH FROM next_export)  "
                                                         "FROM erp_event.kvnr WHERE kvnr_hashed = $1",
                                                         eventKvnr.kvnrHashed());
            EXPECT_EQ(magic_enum::enum_cast<model::EventKvnr::State>(results.at(0, 0).as<std::string>()).value(),
                      model::EventKvnr::State::pending);
            EXPECT_EQ(results.at(0, 1).as<std::int32_t>(), 1);

            const auto next_export_after = model::Timestamp(results.at(0).at(2).as<double>());
            // next_export shall be 2^0 minutes (+ some short add. delay introduced by the processing in the DB)
            const auto expectedDelay = 1min;
            const auto maxDelayByDbProcessing = 30s;
            EXPECT_TRUE(equalsPlus(next_export_after, next_export_before, expectedDelay, maxDelayByDbProcessing));
        }
        transaction.commit();
    }
}

TEST_F(EventProcessorTest, processOne_epa_timeout_retry)
{
    model::Kvnr kvnr{"X011300012"};
    model::EventKvnr eventKvnr(kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0);

    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, prescriptionId1.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, {}, mDoctorIdentity,
                    std::nullopt);

    EventProcessor eventProcessor(serviceContext);

    const auto next_export_before = model::Timestamp::now();
    eventProcessor.processOne(eventKvnr);

    {
        // verify: event not removed, kvnr pending
        auto transaction = createTransaction();
        {
            auto results = transaction.exec_params(
                "SELECT prescription_id, usecase, state FROM erp_event.task_event WHERE kvnr_hashed = $1",
                eventKvnr.kvnrHashed());
            ASSERT_EQ(results.size(), 1);
            EXPECT_EQ(results.at(0, 0).as<std::int64_t>(), prescriptionId1.toDatabaseId());
            EXPECT_EQ(magic_enum::enum_cast<model::TaskEvent::UseCase>(results.at(0, 1).as<std::string>()),
                      model::TaskEvent::UseCase::providePrescription);
        }

        {
            const auto results = transaction.exec_params("SELECT state, retry_count, EXTRACT(EPOCH FROM next_export)  "
                                                         "FROM erp_event.kvnr WHERE kvnr_hashed = $1",
                                                         eventKvnr.kvnrHashed());
            EXPECT_EQ(magic_enum::enum_cast<model::EventKvnr::State>(results.at(0, 0).as<std::string>()).value(),
                      model::EventKvnr::State::pending);
            EXPECT_EQ(results.at(0, 1).as<std::int32_t>(), 1);

            const auto next_export_after = model::Timestamp(results.at(0).at(2).as<double>());
            // next_export shall be 2^0 minutes (+ some short add. delay introduced by the processing in the DB)
            const auto expectedDelay = 1min;
            const auto maxDelayByDbProcessing = 30s;
            EXPECT_TRUE(equalsPlus(next_export_after, next_export_before, expectedDelay, maxDelayByDbProcessing));
        }
        transaction.commit();
    }
}

TEST_F(EventProcessorTest, processOne_epa_timeout_retry3)
{
    model::Kvnr kvnr{"X011300012"};
    model::EventKvnr eventKvnr(kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::processing, 2);

    insertTaskKvnr(kvnr, 2);
    insertTaskEvent(kvnr, prescriptionId1.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, {}, mDoctorIdentity,
                    std::nullopt);

    EventProcessor eventProcessor(serviceContext);

    const auto next_export_before = model::Timestamp::now();
    eventProcessor.processOne(eventKvnr);

    {
        // verify: event not removed, kvnr pending
        auto transaction = createTransaction();
        {
            auto results = transaction.exec_params(
                "SELECT prescription_id, usecase, state FROM erp_event.task_event WHERE kvnr_hashed = $1",
                eventKvnr.kvnrHashed());
            ASSERT_EQ(results.size(), 1);
            EXPECT_EQ(results.at(0, 0).as<std::int64_t>(), prescriptionId1.toDatabaseId());
            EXPECT_EQ(magic_enum::enum_cast<model::TaskEvent::UseCase>(results.at(0, 1).as<std::string>()),
                      model::TaskEvent::UseCase::providePrescription);
        }

        {
            const auto results = transaction.exec_params("SELECT state, retry_count, EXTRACT(EPOCH FROM next_export)  "
                                                         "FROM erp_event.kvnr WHERE kvnr_hashed = $1",
                                                         eventKvnr.kvnrHashed());
            EXPECT_EQ(magic_enum::enum_cast<model::EventKvnr::State>(results.at(0, 0).as<std::string>()).value(),
                      model::EventKvnr::State::pending);
            EXPECT_EQ(results.at(0, 1).as<std::int32_t>(), 3);

            const auto next_export_after = model::Timestamp(results.at(0).at(2).as<double>());
            // next_export shall be 3^2 minutes (+ some short add. delay introduced by the processing in the DB)
            const auto expectedDelay = 9min;
            const auto maxDelayByDbProcessing = 30s;
            EXPECT_TRUE(equalsPlus(next_export_after, next_export_before, expectedDelay, maxDelayByDbProcessing));
        }
        transaction.commit();
    }
}


TEST_F(EventProcessorTest, processOne_epa_timeout_retry_first_event)
{
    /*
     * 2 Events for KVNR and Event1 Outcome 'Retry'
     * The 1st Event was not transmitted
     * The 2nd Event will not be transmitted
     * KVNR's state is updated to pending
     * next_export time of the KVNR is NOW + 2^(retry_count) Minutes.
     */
    model::Kvnr kvnr{"X011300011"};
    model::EventKvnr eventKvnr(kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0);

    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, prescriptionId1.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, {}, mDoctorIdentity,
                    std::nullopt);
    insertTaskEvent(kvnr, prescriptionId1.toString(), model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, mPharmacyIdentity);

    EventProcessor eventProcessor(serviceContext);
    const auto next_export_before = model::Timestamp::now();
    eventProcessor.processOne(eventKvnr);

    {// verify: events not removed, kvnr pending
        auto transaction = createTransaction();
        {
            const auto results =
                transaction.exec_params("SELECT prescription_id, usecase, state FROM erp_event.task_event WHERE "
                                        "kvnr_hashed = $1 ORDER BY prescription_id ASC, last_modified ASC",
                                        eventKvnr.kvnrHashed());
            ASSERT_EQ(results.size(), 2);
            EXPECT_EQ(results.at(0, 0).as<std::int64_t>(), prescriptionId1.toDatabaseId());
            EXPECT_EQ(magic_enum::enum_cast<model::TaskEvent::UseCase>(results.at(0, 1).as<std::string>()),
                      model::TaskEvent::UseCase::providePrescription);
            EXPECT_EQ(magic_enum::enum_cast<model::TaskEvent::State>(results.at(0, 2).as<std::string>()).value(),
                      model::TaskEvent::State::pending);

            EXPECT_EQ(results.at(1, 0).as<std::int64_t>(), prescriptionId1.toDatabaseId());
            EXPECT_EQ(magic_enum::enum_cast<model::TaskEvent::UseCase>(results.at(1, 1).as<std::string>()),
                      model::TaskEvent::UseCase::provideDispensation);
            EXPECT_EQ(magic_enum::enum_cast<model::TaskEvent::State>(results.at(1, 2).as<std::string>()).value(),
                      model::TaskEvent::State::pending);
        }
        {
            const auto results = transaction.exec_params("SELECT state, retry_count, EXTRACT(EPOCH FROM next_export)  "
                                                         "FROM erp_event.kvnr WHERE kvnr_hashed = $1",
                                                         eventKvnr.kvnrHashed());
            EXPECT_EQ(magic_enum::enum_cast<model::EventKvnr::State>(results.at(0, 0).as<std::string>()).value(),
                      model::EventKvnr::State::pending);
            EXPECT_EQ(results.at(0, 1).as<std::int32_t>(), 1);

            const auto next_export_after = model::Timestamp(results.at(0, 2).as<double>());
            // next_export shall be 3^0 = 1 minute (+ some short add. delay introduced by the processing ion the DB)
            const auto expectedDelay = 1min;
            const auto maxDelayByDbProcessing = 30s;
            EXPECT_TRUE(equalsPlus(next_export_after, next_export_before, expectedDelay, maxDelayByDbProcessing));
        }
        transaction.commit();
    }
}

TEST_F(EventProcessorTest, processOne_epa_timeout_retry_second_event)
{
    /*
     * 2 Events for KVNR and Event1 Outcome 'success', Event2 Outcome 'Retry'
     * The 1st Event was transmitted and removed
     * The 2nd Event was not transmitted
     * KVNR's state is updated to pending
     * next_export time of the KVNR is NOW + 2^(retry_count) Minutes.
     */
    model::Kvnr kvnr{"X011300013"};
    model::EventKvnr eventKvnr(kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::processing, 1);

    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, prescriptionId1.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, {}, mDoctorIdentity,
                    std::nullopt);
    insertTaskEvent(kvnr, prescriptionId1.toString(), model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, mPharmacyIdentity);

    EventProcessor eventProcessor(serviceContext);
    const auto next_export_before = model::Timestamp::now();
    eventProcessor.processOne(eventKvnr);

    {// verify: events not removed, kvnr pending
        auto transaction = createTransaction();
        {
            const auto results = transaction.exec_params(
                "SELECT prescription_id, usecase, state FROM erp_event.task_event WHERE kvnr_hashed = $1",
                eventKvnr.kvnrHashed());
            ASSERT_EQ(results.size(), 1);
            EXPECT_EQ(results.at(0, 0).as<std::int64_t>(), prescriptionId1.toDatabaseId());
            EXPECT_EQ(magic_enum::enum_cast<model::TaskEvent::UseCase>(results.at(0, 1).as<std::string>()),
                      model::TaskEvent::UseCase::provideDispensation);
            EXPECT_EQ(magic_enum::enum_cast<model::TaskEvent::State>(results.at(0, 2).as<std::string>()).value(),
                      model::TaskEvent::State::pending);
        }
        {
            const auto results = transaction.exec_params("SELECT state, retry_count, EXTRACT(EPOCH FROM next_export)  "
                                                         "FROM erp_event.kvnr WHERE kvnr_hashed = $1",
                                                         eventKvnr.kvnrHashed());
            EXPECT_EQ(magic_enum::enum_cast<model::EventKvnr::State>(results.at(0, 0).as<std::string>()).value(),
                      model::EventKvnr::State::pending);
            EXPECT_EQ(results.at(0, 1).as<std::int32_t>(), 2);

            const auto next_export_after = model::Timestamp(results.at(0).at(2).as<double>());
            // next_export shall be 3^1 = 3 minute (+ some short add. delay introduced by the processing ion the DB)
            const auto expectedDelay = 3min;
            const auto maxDelayByDbProcessing = 30s;
            EXPECT_TRUE(equalsPlus(next_export_after, next_export_before, expectedDelay, maxDelayByDbProcessing));
        }
        transaction.commit();
    }
}


TEST_F(EventProcessorTest, processOne_epa_timeout_retry_two_events)
{
    model::Kvnr kvnr{"X011300012"};
    model::EventKvnr eventKvnr(kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::processing, 3);

    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, prescriptionId1.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, {}, mDoctorIdentity,
                    std::nullopt);
    insertTaskEvent(kvnr, prescriptionId1.toString(), model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, mPharmacyIdentity);

    EventProcessor eventProcessor(serviceContext);
    const auto next_export_before = model::Timestamp::now();
    eventProcessor.processOne(eventKvnr);

    {// verify: events not removed, kvnr pending
        auto transaction = createTransaction();
        {
            const auto results =
                transaction.exec_params("SELECT prescription_id, usecase, state FROM erp_event.task_event WHERE "
                                        "kvnr_hashed = $1 ORDER BY prescription_id ASC, last_modified ASC",
                                        eventKvnr.kvnrHashed());
            ASSERT_EQ(results.size(), 2);
            EXPECT_EQ(results.at(0, 0).as<std::int64_t>(), prescriptionId1.toDatabaseId());
            EXPECT_EQ(magic_enum::enum_cast<model::TaskEvent::UseCase>(results.at(0, 1).as<std::string>()),
                      model::TaskEvent::UseCase::providePrescription);
            auto s = results.at(0, 2).as<std::string>();
            EXPECT_EQ(magic_enum::enum_cast<model::TaskEvent::State>(results.at(0, 2).as<std::string>()).value(),
                      model::TaskEvent::State::pending);

            EXPECT_EQ(results.at(1, 0).as<std::int64_t>(), prescriptionId1.toDatabaseId());
            EXPECT_EQ(magic_enum::enum_cast<model::TaskEvent::UseCase>(results.at(1, 1).as<std::string>()),
                      model::TaskEvent::UseCase::provideDispensation);
            EXPECT_EQ(magic_enum::enum_cast<model::TaskEvent::State>(results.at(1, 2).as<std::string>()).value(),
                      model::TaskEvent::State::pending);
        }
        {
            const auto results = transaction.exec_params("SELECT state, retry_count, EXTRACT(EPOCH FROM next_export)  "
                                                         "FROM erp_event.kvnr WHERE kvnr_hashed = $1",
                                                         eventKvnr.kvnrHashed());
            EXPECT_EQ(magic_enum::enum_cast<model::EventKvnr::State>(results.at(0, 0).as<std::string>()).value(),
                      model::EventKvnr::State::pending);
            EXPECT_EQ(results.at(0, 1).as<std::int32_t>(), 4);

            const auto next_export_after = model::Timestamp(results.at(0).at(2).as<double>());
            // next_export shall be 3^3 = 27 minute (+ some short add. delay introduced by the processing ion the DB)
            const auto expectedDelay = 27min;
            const auto maxDelayByDbProcessing = 30s;
            EXPECT_TRUE(equalsPlus(next_export_after, next_export_before, expectedDelay, maxDelayByDbProcessing));
        }
        transaction.commit();
    }
}

TEST_F(EventProcessorTest, processEpaConflict)
{
    model::Kvnr kvnr{"X000000012"};
    model::EventKvnr eventKvnr(kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0);

    insertTaskKvnr(kvnr);

    const auto next_export_before = model::Timestamp::now();
    EventProcessor eventProcessor(serviceContext);
    eventProcessor.processEpaConflict(eventKvnr);

    {
        auto transaction = createTransaction();
        {
            // EXPECT_TRUE(next_export_before + 24h <= next_export_after && next_export_after <= next_export_before + 24h + 60min);
            const auto results = transaction.exec_params("SELECT state, retry_count, EXTRACT(EPOCH FROM next_export)  "
                                                         "FROM erp_event.kvnr WHERE kvnr_hashed = $1",
                                                         eventKvnr.kvnrHashed());
            EXPECT_EQ(magic_enum::enum_cast<model::EventKvnr::State>(results.at(0, 0).as<std::string>()).value(),
                      model::EventKvnr::State::pending);
            EXPECT_EQ(results.at(0, 1).as<std::int32_t>(), 0);

            const auto next_export_after = model::Timestamp(results.at(0).at(2).as<double>());
            // next_export shall be 24h (+ some short add. delay introduced by the processing ion the DB)
            const auto expectedDelay = 24h;
            const auto maxDelayByDbProcessing = 30s;
            EXPECT_TRUE(equalsPlus(next_export_after, next_export_before, expectedDelay, maxDelayByDbProcessing));
        }
        transaction.commit();
    }
}


TEST_F(EventProcessorTest, processEpaDeniedOrNotFound)
{
    model::Kvnr kvnr{"X000000012"};
    model::EventKvnr eventKvnr(kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0);
    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, prescriptionId1.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, std::nullopt);
    insertTaskEvent(kvnr, prescriptionId1.toString(), model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, mPharmacyIdentity);

    insertTaskEvent(kvnr, prescriptionId2.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, std::nullopt);
    insertTaskEvent(kvnr, prescriptionId2.toString(), model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, mPharmacyIdentity);

    EventProcessor eventProcessor(serviceContext);
    eventProcessor.processEpaDeniedOrNotFound(eventKvnr);

    {
        auto transaction = createTransaction();
        const auto resultTaskEvents = transaction.exec_params(
            "SELECT COUNT(*) FROM erp_event.task_event WHERE kvnr_hashed = $1", eventKvnr.kvnrHashed());
        EXPECT_EQ(resultTaskEvents.at(0, 0).as<int>(), 0);
        const auto resultKvnrs =
            transaction.exec_params("SELECT state FROM erp_event.kvnr WHERE kvnr_hashed = $1", eventKvnr.kvnrHashed());
        EXPECT_EQ(resultKvnrs.at(0, 0).as<std::string>(), "processed");
        transaction.commit();
    }
}

TEST_F(EventProcessorTest, processEpaUnkown)
{
    model::Kvnr kvnr{"X000000012"};
    model::EventKvnr eventKvnr(kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0);
    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, prescriptionId1.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, std::nullopt);
    insertTaskEvent(kvnr, prescriptionId1.toString(), model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, mPharmacyIdentity);

    insertTaskEvent(kvnr, prescriptionId2.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, std::nullopt);
    insertTaskEvent(kvnr, prescriptionId2.toString(), model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, healthcareProviderPrescription, medicationDispenseBundle,
                    mDoctorIdentity, mPharmacyIdentity);

    EventProcessor eventProcessor(serviceContext);
    eventProcessor.processEpaUnknown(eventKvnr);

    {
        // MedicationExporterPostgresBackend db;
        const auto events = database().getAllEventsForKvnr(
            model::EventKvnr(kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0));
        ASSERT_EQ(events.size(), 4);
        EXPECT_EQ(events[0]->getState(), model::TaskEvent::State::pending);
        EXPECT_EQ(events[1]->getState(), model::TaskEvent::State::pending);
        EXPECT_EQ(events[2]->getState(), model::TaskEvent::State::pending);
        EXPECT_EQ(events[3]->getState(), model::TaskEvent::State::pending);
        database().commitTransaction();
    }
}
