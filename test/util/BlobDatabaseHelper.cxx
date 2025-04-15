/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/util/BlobDatabaseHelper.hxx"

#include "shared/database/PostgresConnection.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/util/TestConfiguration.hxx"


void BlobDatabaseHelper::removeUnreferencedBlobs (void)
{
    if ( TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false))
    {
        auto connection = pqxx::connection(PostgresConnection::defaultConnectString());
        pqxx::work transaction (connection);
        transaction.exec("DELETE FROM erp.blob WHERE type NOT IN (6,7,8,12)");
        transaction.exec("DELETE FROM erp.blob AS blob"
                         "  WHERE type = 6"
                         "  AND NOT EXISTS (SELECT 1 FROM erp.task_view"
                         "                  WHERE task_key_blob_id = blob.blob_id OR medication_dispense_blob_id = blob.blob_id)"
                         "  AND NOT EXISTS (SELECT 1 FROM erp.account WHERE blob_id = blob.blob_id)");
        transaction.exec("DELETE FROM erp.blob AS blob"
                         "  WHERE type = 7"
                         "  AND NOT EXISTS (SELECT 1 FROM erp.communication"
                         "                  WHERE recipient_blob_id = blob.blob_id OR sender_blob_id = blob.blob_id)"
                         "  AND NOT EXISTS (SELECT 1 FROM erp.account WHERE blob_id = blob.blob_id)");
        transaction.exec("DELETE FROM erp.blob AS blob"
                         "  WHERE type = 8"
                         "  AND NOT EXISTS (SELECT 1 FROM erp.auditevent WHERE blob_id = blob.blob_id)"
                         "  AND NOT EXISTS (SELECT 1 FROM erp.account WHERE blob_id = blob.blob_id)");
        transaction.exec("DELETE FROM erp.blob AS blob"
                         "  WHERE type = 12"
                         "  AND NOT EXISTS (SELECT 1 FROM erp.charge_item WHERE blob_id = blob.blob_id)");
        transaction.commit();
    }
}


void BlobDatabaseHelper::removeTestVsdmKeyBlobs(char operatorId)
{
    if (TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false))
    {
        auto connection = pqxx::connection(PostgresConnection::defaultConnectString());
        pqxx::work transaction(connection);
        transaction.exec_params("DELETE FROM erp.vsdm_key_blob WHERE operator LIKE $1", std::string{operatorId});
        transaction.commit();
    }
}
