#include "test/util/BlobDatabaseHelper.hxx"

#include "erp/database/PostgresBackend.hxx"
#include "mock/hsm/MockBlobDatabase.hxx"
#include "mock/util/MockConfiguration.hxx"


void BlobDatabaseHelper::clearBlobDatabase (void)
{
    if ( ! MockConfiguration::instance().getOptionalBoolValue(MockConfigurationKey::MOCK_USE_BLOB_DATABASE_MOCK, true))
    {
        auto connection = pqxx::connection(PostgresBackend::defaultConnectString());
        pqxx::work transaction (connection);
        transaction.exec("DELETE FROM erp.blob WHERE true");
        transaction.commit();
    }
}
