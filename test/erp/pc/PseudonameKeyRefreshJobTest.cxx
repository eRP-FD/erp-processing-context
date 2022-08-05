/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/hsm/BlobCache.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/pc/telematik_report_pseudonym/PseudonameKeyRefreshJob.hxx"
#include "erp/util/Configuration.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "mock/util/MockConfiguration.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/TestConfiguration.hxx"

#include <gtest/gtest.h>
#include <memory>

class PseudonameKeyRefreshJobTest : public testing::Test
{
public:
    void SetUp() override
    {
        mBlobCache = MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm);
        mHsmPool =
            std::make_unique<HsmPool>(std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(), mBlobCache),
                                      TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>());
        mConfiguration = &Configuration::instance();
    }

    void TearDown() override
    {
    }

    std::shared_ptr<BlobCache> mBlobCache;
    std::unique_ptr<HsmPool> mHsmPool;
    const Configuration* mConfiguration;
    EnvironmentVariableGuard check{"ERP_REPORT_LEIPS_KEY_CHECK_INTERVAL_SECONDS", "2"};
    EnvironmentVariableGuard refresh{"ERP_REPORT_LEIPS_KEY_REFRESH_INTERVAL_SECONDS", "6"};
    EnvironmentVariableGuard enab{"ERP_REPORT_LEIPS_KEY_ENABLE", "true"};
};

TEST_F(PseudonameKeyRefreshJobTest, IdenticalKey_EQ_Hashes)
{
    auto job = PseudonameKeyRefreshJob::setupPseudonameKeyRefreshJob(*mHsmPool, *mBlobCache, *mConfiguration);
    if (job)
    {
        std::string hash_1 = job->hkdf("top secret information");
        std::string hash_2 = job->hkdf("top secret information");
        EXPECT_NE(hash_1, "");
        EXPECT_EQ(hash_1, hash_2);
        job->shutdown();
    }
}

TEST_F(PseudonameKeyRefreshJobTest, DifferentKey_NE_Hashes)
{
    auto job = PseudonameKeyRefreshJob::setupPseudonameKeyRefreshJob(*mHsmPool, *mBlobCache, *mConfiguration);
    if (job)
    {
        std::string hash_1 = job->hkdf("top secret information");
        std::this_thread::sleep_for(std::chrono::seconds(8));
        std::string hash_2 = job->hkdf("top secret information");
        EXPECT_NE(hash_1, "");
        EXPECT_NE(hash_2, "");
        EXPECT_NE(hash_1, hash_2);
        job->shutdown();
    }
}

TEST_F(PseudonameKeyRefreshJobTest, DisableJob)
{
    EnvironmentVariableGuard enab{"ERP_REPORT_LEIPS_KEY_ENABLE", "false"};
    auto job = PseudonameKeyRefreshJob::setupPseudonameKeyRefreshJob(*mHsmPool, *mBlobCache, *mConfiguration);
    EXPECT_FALSE(job);
}

TEST_F(PseudonameKeyRefreshJobTest, HashWithMissingKey)
{
    EnvironmentVariableGuard check{"ERP_REPORT_LEIPS_KEY_CHECK_INTERVAL_SECONDS", "2"};
    EnvironmentVariableGuard refresh{"ERP_REPORT_LEIPS_KEY_REFRESH_INTERVAL_SECONDS", "6"};
    EnvironmentVariableGuard enab{"ERP_REPORT_LEIPS_KEY_ENABLE", "true"};

    auto job = PseudonameKeyRefreshJob::setupPseudonameKeyRefreshJob(*mHsmPool, *mBlobCache, *mConfiguration);
    if (job)
    {
        std::string hash_1 = job->hkdf("top secret information");
        job->shutdown();
        std::string hash_2 = job->hkdf("top secret information");
        EXPECT_NE(hash_1, "");
        EXPECT_EQ(hash_2, "");
    }
}
