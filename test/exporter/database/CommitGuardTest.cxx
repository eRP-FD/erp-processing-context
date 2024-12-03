/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/database/CommitGuard.hxx"
#include "exporter/MedicationExporterMain.hxx"
#include "exporter/database/MedicationExporterDatabaseBackend.hxx"
#include "exporter/database/MedicationExporterDatabaseFrontend.hxx"
#include "exporter/model/EventKvnr.hxx"
#include "exporter/pc/MedicationExporterFactories.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "gtest/gtest.h"
#include "test/exporter/database/PostgresDatabaseTest.hxx"


class CommitGuardTest : public PostgresDatabaseTest
{
};

TEST_F(CommitGuardTest, only_one_transaction_allowed)//NOLINT(readability-function-cognitive-complexity)
{
    auto ioThreadPool = ThreadPool();

    ioThreadPool.setUp(1, "medication-exporter-io");
    auto& ioContext = ioThreadPool.ioContext();
    auto serviceContext = std::make_shared<MedicationExporterServiceContext>(
        ioContext, Configuration::instance(), MedicationExporterMain::createProductionFactories());

    // expected default usage: one transaction, committed, after end of line
    MedicationExporterCommitGuard(serviceContext->medicationExporterDatabaseFactory()).db().processNextKvnr();

    // one transaction, committed, after end of code
    {
        MedicationExporterCommitGuard cg(serviceContext->medicationExporterDatabaseFactory());
        const auto kvnrs = cg.db().processNextKvnr();
    }

    // one transaction, 3 queries, committed after end of block
    {
        MedicationExporterCommitGuard cg(serviceContext->medicationExporterDatabaseFactory());
        const auto kvnrs1 = cg.db().processNextKvnr();
        const auto kvnrs2 = cg.db().processNextKvnr();
        const auto kvnrs3 = cg.db().processNextKvnr();
    }

    // two transactions, throws on creation of 2nd transaction
    {
        MedicationExporterCommitGuard cg1(serviceContext->medicationExporterDatabaseFactory());
        EXPECT_THROW(MedicationExporterCommitGuard cg2(serviceContext->medicationExporterDatabaseFactory()), std::exception);
        cg1.db().processNextKvnr();
    }
}

TEST_F(CommitGuardTest, create_and_query)//NOLINT(readability-function-cognitive-complexity)
{
    const model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.000.100.000.001.05");
    const std::string kvnr_str{"X000000012"};
    const model::Kvnr kvnr{kvnr_str};
    const auto hashed_kvnr = kvnrHashed(kvnr);
    model::EventKvnr eventKvnr{hashed_kvnr, std::nullopt, std::nullopt, model::EventKvnr::State::processing};

    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, prescriptionId.toString(), model::TaskEvent::UseCase::cancelPrescription,
                    model::TaskEvent::State::pending, ResourceTemplates::kbvBundleXml(), std::nullopt, mDoctorIdentity,
                    std::nullopt);

    auto ioThreadPool = ThreadPool();

    ioThreadPool.setUp(1, "medication-exporter-io");
    auto& ioContext = ioThreadPool.ioContext();
    auto serviceContext = std::make_shared<MedicationExporterServiceContext>(
        ioContext, Configuration::instance(), MedicationExporterMain::createProductionFactories());

    // create by service context
    EXPECT_EQ(MedicationExporterCommitGuard(serviceContext->medicationExporterDatabaseFactory()).db()
        .getAllEventsForKvnr(eventKvnr).size(), 1);

    // create by DB pointer
    auto db = serviceContext->medicationExporterDatabaseFactory();
    EXPECT_EQ(MedicationExporterCommitGuard(std::move(db)).db()
        .getAllEventsForKvnr(eventKvnr).size(), 1);

    // create by default constructor (only if DB provides it)
    CommitGuard<MedicationExporterPostgresBackend, commit_guard_policies::ExceptionCommitStrategy> cgPostgres;
    EXPECT_EQ(cgPostgres.db().getAllEventsForKvnr(eventKvnr).size(), 1);

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

    model::EventKvnr eventKvnr{hashed_kvnr, std::nullopt, std::nullopt, model::EventKvnr::State::processing};

    using commitguard_t = CommitGuard<MedicationExporterPostgresBackend, commit_guard_policies::ExceptionCommitStrategy>;
    std::vector<db_model::TaskEvent> events;
    try
    {
        commitguard_t guard;
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

    model::EventKvnr eventKvnr{hashed_kvnr, std::nullopt, std::nullopt, model::EventKvnr::State::processing};

    using commitguard_t = CommitGuard<MedicationExporterPostgresBackend, commit_guard_policies::ExceptionAbortStrategy>;
    std::vector<db_model::TaskEvent> events;
    try
    {
        commitguard_t guard;
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
