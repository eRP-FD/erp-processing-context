/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_DATABASETESTFIXTURE_HXX
#define ERP_PROCESSING_CONTEXT_TEST_DATABASETESTFIXTURE_HXX

#include "erp/database/DatabaseFrontend.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/mock/MockDatabaseProxy.hxx"
#include "test/util/TestConfiguration.hxx"

#include <gtest/gtest.h>


template <typename TestClass = ::testing::Test>
class DatabaseTestFixture : public TestClass
{
protected:
    DatabaseFrontend database()
    {
        if (usePostgres())
        {
            return DatabaseFrontend(std::make_unique<PostgresBackend>(), mHsmPool, mKeyDerivation);
        }
        return DatabaseFrontend(std::make_unique<MockDatabaseProxy>(mMockDatabase), mHsmPool, mKeyDerivation);
    }

    const TestConfiguration& testConfiguration = TestConfiguration::instance();

    bool usePostgres()
    {
        return testConfiguration.getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false);
    }

    const KeyDerivation& keyDerivation()
    {
        return mKeyDerivation;
    }

private:
    HsmPool mHsmPool{
        std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(),
                                         MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm)),
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>()};
    KeyDerivation mKeyDerivation{mHsmPool};
    MockDatabase mMockDatabase{mHsmPool};
};


#endif// ERP_PROCESSING_CONTEXT_TEST_DATABASETESTFIXTURE_HXX
