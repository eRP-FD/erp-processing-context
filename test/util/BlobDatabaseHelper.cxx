/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/util/BlobDatabaseHelper.hxx"

#include "erp/database/PostgresBackend.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/util/TestConfiguration.hxx"


void BlobDatabaseHelper::removeUnreferencedBlobs (void)
{
    if ( TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false))
    {
        auto connection = pqxx::connection(PostgresBackend::defaultConnectString());
        pqxx::work transaction (connection);
        transaction.exec("DELETE FROM erp.blob WHERE type NOT IN (6,7,8)");
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
        transaction.commit();
    }
}
