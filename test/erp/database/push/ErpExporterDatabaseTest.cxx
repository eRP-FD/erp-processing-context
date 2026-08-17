/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/push/PushExporterDatabase.hxx"
#include "erp/database/push/PushExporterPostgresBackend.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "shared/database/PostgresConnection.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "shared/hsm/KeyDerivation.hxx"
#include "shared/hsm/TeeTokenUpdater.hxx"
#include "shared/util/DurationConsumer.hxx"
#include "shared/util/Expect.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/util/TestConfiguration.hxx"

#include <gtest/gtest.h>
#include <pqxx/connection>
#include <pqxx/transaction>


class BlobCache;
class DurationConsumerGuard;

// Test for the database connection erp-processing-context -> erp-exporter-database

class ErpExporterDatabaseTest : public ::testing::Test
{
public:
    ErpExporterDatabaseTest()
    {
        mBlobCache = MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm);

        auto blobCache = mBlobCache;
        mHsmPool = std::make_unique<HsmPool>(
            std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(), std::move(blobCache)),
            TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>());
        mKeyDerivation = std::make_unique<KeyDerivation>(*mHsmPool);
        mDurationConsumerGuard = std::make_unique<DurationConsumerGuard>("PostgresDatabaseTest");
    }
    void SetUp() override
    {
        if (!usePostgres())
        {
            GTEST_SKIP();
        }
    }
    void TearDown() override
    {
        if (mDatabase)
        {
            mDatabase.reset();
        }
        if (mConnection)
        {
            mConnection.reset();
        }
    }
    static bool usePostgres()
    {
        return TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false);
    }
    PushExporterDatabase& database ()
    {
        if ( ! mDatabase)
        {
            Expect(usePostgres(), "database support is disabled, database should not be used");
            mDatabase = std::make_unique<PushExporterDatabase>(
                            std::make_unique<PushExporterPostgresBackend>(PushExporterPostgresBackend::mainConnection()), *mHsmPool, *mKeyDerivation);
        }
        return *mDatabase;
    }
    pqxx::connection& getConnection()
    {
        Expect(usePostgres(), "database support is disabled, database should not be used");
        if (!mConnection)
        {
            mConnection = std::make_unique<pqxx::connection>(PostgresConnection::defaultConnectParameters().str());
        }
        return *mConnection;
    }
    pqxx::work createTransaction()
    {
        return pqxx::work{getConnection()};
    }

    std::unique_ptr<HsmPool> mHsmPool;
    std::unique_ptr<KeyDerivation> mKeyDerivation;
    std::unique_ptr<pqxx::connection> mConnection;
    std::unique_ptr<PushExporterDatabase> mDatabase;
    std::shared_ptr<BlobCache> mBlobCache;
    std::unique_ptr<DurationConsumerGuard> mDurationConsumerGuard;
};

TEST_F(ErpExporterDatabaseTest, DISABLED_healthCheckEmptyTable)
{
    ASSERT_NO_THROW(database().healthCheck());
}
