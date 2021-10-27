/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/erp/database/PostgresDatabaseTest.hxx"

#include <gtest/gtest.h>

#include "erp/database/DatabaseModel.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/util/Configuration.hxx"
#include "test_config.h"

class TransactionTest : public PostgresDatabaseTest
{
public:
};

TEST_F(TransactionTest, rollback)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }
    std::optional<model::PrescriptionId> prescriptionId;
    {
        PostgresBackend database;
        prescriptionId.emplace(std::get<0>(
                database.createTask(model::Task::Status::draft, model::Timestamp::now(), model::Timestamp::now())));
        database.updateTask(*prescriptionId, db_model::EncryptedBlob(pqxx::binarystring("accessCode")), 0,
                            db_model::Blob(pqxx::binarystring("salt")));
    }// closing scope performs rollback

    ASSERT_TRUE(prescriptionId);

    {
        PostgresBackend database;
        auto taskOptional = database.retrieveTaskBasics(*prescriptionId);
        ASSERT_FALSE(taskOptional);
    }
}

TEST_F(TransactionTest, commit)
{
    if (!usePostgres())// NOLINT
    {
        GTEST_SKIP();
    }
    std::optional<model::PrescriptionId> prescriptionId;
    {
        PostgresBackend database;
        prescriptionId.emplace(std::get<0>(
            database.createTask(model::Task::Status::draft, model::Timestamp::now(), model::Timestamp::now())));
        database.updateTask(*prescriptionId, db_model::EncryptedBlob(pqxx::binarystring("accessCode")), 0,
                            db_model::Blob(pqxx::binarystring("salt")));
        database.commitTransaction();
    }

    ASSERT_TRUE(prescriptionId);

    {
        PostgresBackend database;
        auto taskOptional = database.retrieveTaskBasics(*prescriptionId);
        ASSERT_TRUE(taskOptional);
    }
}
