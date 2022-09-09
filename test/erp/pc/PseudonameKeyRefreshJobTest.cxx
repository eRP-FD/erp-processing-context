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
    class LocalHsmMockFactory : public ::HsmMockFactory
    {
    public:
        explicit LocalHsmMockFactory(std::unique_ptr<HsmClient>&& hsmClient, std::shared_ptr<BlobCache> blobCache,
                                     volatile const bool& forceFailFlag)
            : ::HsmMockFactory{std::move(hsmClient), blobCache}
            , mForceFailFlag{forceFailFlag}
        {
        }

        std::shared_ptr<HsmRawSession> rawConnect() override
        {
            if (mForceFailFlag)
            {
                throw std::runtime_error("hsm connect failure forced for test.");
            }
            return std::shared_ptr<HsmRawSession>();
        }

        volatile const bool& mForceFailFlag;
    };


    void SetUp() override
    {
        mBlobCache = MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm);
        mHsmPool = std::make_unique<HsmPool>(
            std::make_unique<LocalHsmMockFactory>(std::make_unique<HsmMockClient>(), mBlobCache, mForceFailFlag),
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
    EnvironmentVariableGuard failedCheck{"ERP_REPORT_LEIPS_FAILED_KEY_CHECK_INTERVAL_SECONDS", "2"};
    EnvironmentVariableGuard refresh{"ERP_REPORT_LEIPS_KEY_REFRESH_INTERVAL_SECONDS", "6"};
    EnvironmentVariableGuard enab{"ERP_REPORT_LEIPS_KEY_ENABLE", "true"};
    volatile bool mForceFailFlag = false;
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

TEST_F(PseudonameKeyRefreshJobTest, intervalWhileFailed)
{
    using namespace std::chrono_literals;
    EnvironmentVariableGuard enable{"ERP_REPORT_LEIPS_KEY_ENABLE", "true"};
    EnvironmentVariableGuard check{"ERP_REPORT_LEIPS_KEY_CHECK_INTERVAL_SECONDS", "2"};
    EnvironmentVariableGuard failedCheck{"ERP_REPORT_LEIPS_FAILED_KEY_CHECK_INTERVAL_SECONDS", "1"};
    EnvironmentVariableGuard refresh{"ERP_REPORT_LEIPS_KEY_REFRESH_INTERVAL_SECONDS", "6"};
    mForceFailFlag = true;

    auto job = PseudonameKeyRefreshJob::setupPseudonameKeyRefreshJob(*mHsmPool, *mBlobCache, *mConfiguration);
    ASSERT_TRUE(job);
    EXPECT_EQ(job->hkdf("top secret information"), "");
    EXPECT_EQ(std::chrono::duration_cast<std::chrono::seconds>(job->getInterval()).count(), 1);
    std::this_thread::sleep_for(2s);
    EXPECT_EQ(job->hkdf("top secret information"), "");
    EXPECT_EQ(job->getInterval(), 1s);

    mForceFailFlag = false;
    std::this_thread::sleep_for(2s);
    EXPECT_NE(job->hkdf("top secret information"), "");
    EXPECT_EQ(job->getInterval(), 2s);
    job->shutdown();
}
