/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "exporter/database/CommitGuard.hxx"
#include "exporter/database/MedicationExporterDatabaseBackend.hxx"
#include "exporter/database/MedicationExporterDatabaseFrontend.hxx"
#include "exporter/model/EventKvnr.hxx"
#include "exporter/pc/MedicationExporterFactories.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "test/exporter/database/PostgresDatabaseTest.hxx"
#include "test/exporter/util/MedicationExporterStaticData.hxx"


class CommitGuardTest : public PostgresDatabaseTest
{
};

TEST_F(CommitGuardTest, only_one_transaction_allowed)//NOLINT(readability-function-cognitive-complexity)
{
    auto ioThreadPool = ThreadPool();

    ioThreadPool.setUp(1, "medication-exporter-io");
    auto& ioContext = ioThreadPool.ioContext();
    auto serviceContext = std::make_shared<MedicationExporterServiceContext>(
        ioContext, Configuration::instance(), MedicationExporterStaticData::makeMockMedicationExporterFactories());

    // expected default usage: one transaction, committed, after end of line
    CommitGuard(serviceContext->medicationExporterDatabaseFactory(TransactionMode::transaction)).db().processNextKvnr();

    // one transaction, committed, after end of code
    {
        CommitGuard cg(serviceContext->medicationExporterDatabaseFactory(TransactionMode::transaction));
        const auto kvnrs = cg.db().processNextKvnr();
    }

    // one transaction, 3 queries, committed after end of block
    {
        CommitGuard cg(serviceContext->medicationExporterDatabaseFactory(TransactionMode::transaction));
        const auto kvnrs1 = cg.db().processNextKvnr();
        const auto kvnrs2 = cg.db().processNextKvnr();
        const auto kvnrs3 = cg.db().processNextKvnr();
    }

    // two transactions, throws on creation of 2nd transaction
    {
        CommitGuard cg1(serviceContext->medicationExporterDatabaseFactory(TransactionMode::transaction));
        EXPECT_THROW(CommitGuard cg2(serviceContext->medicationExporterDatabaseFactory(TransactionMode::transaction)),
                     std::exception);
        cg1.db().processNextKvnr();
    }
}

TEST_F(CommitGuardTest, create_and_query)//NOLINT(readability-function-cognitive-complexity)
{
    const model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.000.100.000.001.05");
    const std::string kvnr_str{"X000000012"};
    const model::Kvnr kvnr{kvnr_str};
    const auto hashed_kvnr = kvnrHashed(kvnr);
    model::EventKvnr eventKvnr{hashed_kvnr, std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0};

    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, prescriptionId.toString(), model::TaskEvent::UseCase::cancelPrescription,
                    model::TaskEvent::State::pending, ResourceTemplates::kbvBundleXml(), std::nullopt, mDoctorIdentity,
                    std::nullopt);

    auto ioThreadPool = ThreadPool();

    ioThreadPool.setUp(1, "medication-exporter-io");
    auto& ioContext = ioThreadPool.ioContext();
    auto serviceContext = std::make_shared<MedicationExporterServiceContext>(
        ioContext, Configuration::instance(), MedicationExporterStaticData::makeMockMedicationExporterFactories());

    // create by service context
    EXPECT_EQ(CommitGuard(serviceContext->medicationExporterDatabaseFactory(TransactionMode::transaction))
                  .db()
                  .getAllEventsForKvnr(eventKvnr)
                  .size(),
              1);

    // create by DB pointer
    auto db = serviceContext->medicationExporterDatabaseFactory(TransactionMode::transaction);
    EXPECT_EQ(CommitGuard(std::move(db)).db().getAllEventsForKvnr(eventKvnr).size(), 1);
}


TEST_F(CommitGuardTest, exceptionstrategy_commit)//NOLINT(readability-function-cognitive-complexity)
{
    const model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.000.100.000.001.05");
    const std::string kvnr_str{"X000000012"};
    const model::Kvnr kvnr{kvnr_str};
    const auto hashed_kvnr = kvnrHashed(kvnr);

    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, prescriptionId.toString(), model::TaskEvent::UseCase::cancelPrescription,
                    model::TaskEvent::State::pending, ResourceTemplates::kbvBundleXml(), std::nullopt, mDoctorIdentity,
                    std::nullopt);

    model::EventKvnr eventKvnr{hashed_kvnr, std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0};

    using commitguard_t = CommitGuard<MedicationExporterPostgresBackend, commit_guard_policies::ExceptionCommitStrategy>;
    std::vector<db_model::TaskEvent> events;
    try
    {
        commitguard_t guard{std::make_unique<MedicationExporterPostgresBackend>(TransactionMode::transaction)};
        EXPECT_EQ(guard.db().getAllEventsForKvnr(eventKvnr).size(), 1);
        guard.db().deleteAllEventsForKvnr(eventKvnr);
        EXPECT_EQ(guard.db().getAllEventsForKvnr(eventKvnr).size(), 0);
        throw std::runtime_error("Some failure");
    }
    catch (...)
    {
    }
    auto transaction = pqxx::work{getConnection()};
    auto r = transaction.exec_params("SELECT COUNT(*) FROM erp_event.task_event WHERE kvnr_hashed = $1",
                                     eventKvnr.kvnrHashed());
    EXPECT_EQ(r.at(0, 0).as<int>(), 0);
    transaction.commit();
}

TEST_F(CommitGuardTest, exceptionstrategy_abort)//NOLINT(readability-function-cognitive-complexity)
{
    const model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.000.100.000.001.05");
    const std::string kvnr_str{"X000000012"};
    const model::Kvnr kvnr{kvnr_str};
    const auto hashed_kvnr = kvnrHashed(kvnr);

    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, prescriptionId.toString(), model::TaskEvent::UseCase::cancelPrescription,
                    model::TaskEvent::State::pending, ResourceTemplates::kbvBundleXml(), std::nullopt, mDoctorIdentity,
                    std::nullopt);

    model::EventKvnr eventKvnr{hashed_kvnr, std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0};

    using commitguard_t = CommitGuard<MedicationExporterPostgresBackend, commit_guard_policies::ExceptionAbortStrategy>;
    std::vector<db_model::TaskEvent> events;
    try
    {
        commitguard_t guard{std::make_unique<MedicationExporterPostgresBackend>(TransactionMode::transaction)};
        EXPECT_EQ(guard.db().getAllEventsForKvnr(eventKvnr).size(), 1);
        guard.db().deleteAllEventsForKvnr(eventKvnr);
        EXPECT_EQ(guard.db().getAllEventsForKvnr(eventKvnr).size(), 0);
        throw std::runtime_error("Some failure");
    }
    catch (...)
    {
    }
    auto transaction = pqxx::work{getConnection()};
    auto r = transaction.exec_params("SELECT COUNT(*) FROM erp_event.task_event WHERE kvnr_hashed = $1",
                                     eventKvnr.kvnrHashed());
    EXPECT_EQ(r.at(0, 0).as<int>(), 1);
    transaction.commit();
}

class CommitGuardDestructorTest : public testing::Test {
protected:
    struct MockDb {
        MOCK_METHOD(void, commitTransaction, ());
        MOCK_METHOD(void, abortTransaction, ());
    };
    template <typename>
    struct MockStrategy {
        static void onException(MockDb& mockDB)
        {
            mockDB.abortTransaction();
        }
    };
};

TEST_F(CommitGuardDestructorTest, commitTransaction)
{
    auto commitTransaction = std::make_unique<MockDb>();
    EXPECT_CALL(*commitTransaction, commitTransaction()).Times(1);
    ASSERT_NO_THROW(CommitGuard guard{std::move(commitTransaction)});

}

TEST_F(CommitGuardDestructorTest, exception)
{
    auto commitTransaction = std::make_unique<MockDb>();
    EXPECT_CALL(*commitTransaction, commitTransaction()).Times(0);
    EXPECT_CALL(*commitTransaction, abortTransaction()).Times(1);
    try {
        CommitGuard<MockDb, MockStrategy> guard{std::move(commitTransaction)};
        throw std::runtime_error("Exception");
    }
    catch (const std::runtime_error&)
    {}
}

TEST_F(CommitGuardDestructorTest, exceptionInCommitTransaction)
{
    struct ThrowInCommitTransaction {
        void commitTransaction()
        {
            throw std::runtime_error("Exception");
        }
    };
    ASSERT_ANY_THROW(CommitGuard guard{std::make_unique<ThrowInCommitTransaction>()});
}
