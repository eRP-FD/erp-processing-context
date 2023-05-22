/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/erp/database/PostgresDatabaseTest.hxx"

#include <gtest/gtest.h>

#include "erp/database/DatabaseModel.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/hsm/BlobCache.hxx"
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
        BlobId blobId = blobCache()->getBlob(BlobType::TaskKeyDerivation).id;
        PostgresBackend database;
        prescriptionId.emplace(std::get<0>(database.createTask(model::PrescriptionType::apothekenpflichigeArzneimittel,
                                                               model::Task::Status::draft, model::Timestamp::now(),
                                                               model::Timestamp::now())));
        database.updateTask(
            *prescriptionId,
            db_model::EncryptedBlob(db_model::postgres_bytea_view(reinterpret_cast<const std::byte*>("accessCode"))),
            blobId, db_model::Blob(db_model::postgres_bytea_view(reinterpret_cast<const std::byte*>("salt"))));
    }// closing scope performs rollback

    ASSERT_TRUE(prescriptionId);

    {
        PostgresBackend database;
        auto taskOptional = database.retrieveTaskAndPrescription(*prescriptionId);
        ASSERT_FALSE(taskOptional);
    }
}

TEST_F(TransactionTest, commit)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    std::optional<model::PrescriptionId> prescriptionId;
    {
        BlobId blobId = blobCache()->getBlob(BlobType::TaskKeyDerivation).id;
        PostgresBackend database;
        prescriptionId.emplace(std::get<0>(database.createTask(model::PrescriptionType::apothekenpflichigeArzneimittel,
                                                               model::Task::Status::draft, model::Timestamp::now(),
                                                               model::Timestamp::now())));
        database.updateTask(
            *prescriptionId,
            db_model::EncryptedBlob(db_model::postgres_bytea_view(reinterpret_cast<const std::byte*>("accessCode"))),
            blobId, db_model::Blob(db_model::postgres_bytea_view(reinterpret_cast<const std::byte*>("salt"))));
        database.commitTransaction();
    }

    ASSERT_TRUE(prescriptionId);

    {
        PostgresBackend database;
        auto taskOptional = database.retrieveTaskAndPrescription(*prescriptionId);
        ASSERT_TRUE(taskOptional);
    }
}
