/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/hsm/BlobCache.hxx"
#include "shared/hsm/ErpTypes.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "erp/pc/telematik_report_pseudonym/PseudonameKeyRefreshJob.hxx"
#include "shared/util/Configuration.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "mock/util/MockConfiguration.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/TestConfiguration.hxx"

#include <gtest/gtest.h>
#include <functional>
#include <memory>

class FailingMockClient : public HsmMockClient
{
public:
    ErpArray<Aes256Length> unwrapPseudonameKey(const HsmRawSession& session, UnwrapHashKeyInput&& input) override
    {
        if (unwrapPseudonameKeyCallback)
            unwrapPseudonameKeyCallback();
        return HsmMockClient::unwrapPseudonameKey(session, std::move(input));
    }

    void setUnwrapPseudonameKeyCallback(const std::function<void()>& callback)
    {
        unwrapPseudonameKeyCallback = callback;
    }

private:
    std::function<void()> unwrapPseudonameKeyCallback;
};

class PseudonameKeyRefreshJobTest : public testing::Test
{
public:

    std::unique_ptr<HsmClient> createHsmClient()
    {
        auto hsmMock = std::make_unique<FailingMockClient>();
        hsmMock->setUnwrapPseudonameKeyCallback([this]() {
            if (mForceFailFlag)
            {
                throw std::runtime_error("hsm connect failure forced for test.");
            }
        });
        return hsmMock;
    }


    void SetUp() override
    {
        mBlobCache = MockBlobCache::createAndSetup(MockBlobCache::MockTarget::MockedHsm, std::make_unique<MockBlobDatabase>());
        mHsmPool =
            std::make_unique<HsmPool>(std::make_unique<HsmMockFactory>(createHsmClient(), mBlobCache),
                                      TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>());
        mConfiguration = &Configuration::instance();
    }

    void TearDown() override
    {
    }

    std::shared_ptr<BlobCache> mBlobCache;
    std::unique_ptr<HsmPool> mHsmPool;
    const Configuration* mConfiguration{};
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
