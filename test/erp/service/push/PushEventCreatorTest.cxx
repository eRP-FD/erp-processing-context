/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/push/PushErpDatabase.hxx"
#include "erp/database/push/PushExporterDatabase.hxx"
#include "erp/database/push/PushEventDataCollector.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "erp/service/push/PushEventCreator.hxx"
#include "shared/server/BaseHttpsServer.hxx"
#include "shared/util/Expect.hxx"
#include "test/mock/MockPushExporterDatabaseProxy.hxx"
#include "test/mock/PushExporterMockBackend.hxx"
#include "test/mock/PushErpBackendProxy.hxx"
#include "test/mock/PushErpMockBackend.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <future>
#include <latch>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace std::chrono_literals;

class PushEventCreatorTest : public testing::Test
{
protected:
    void SetUp() override
    {

        mServiceContext = std::make_unique<PcServiceContext>(Configuration::instance(), factories());

        // start 5 theads so the strand actually has to synchronize something
        mServiceContext->getTeeServer().serve(5, "testserver");
    }

    virtual Factories factories()
    {
        auto factories = StaticData::makeMockFactories();

        mPushErpMockBackend = std::make_shared<testing::NiceMock<PushErpMockBackend>>();
        factories.readOnlyPushErpDatabaseFactory = [this](HsmPool& hsmPool, KeyDerivation& keyDerivation) {
            return std::make_unique<PushErpDatabase>(std::make_unique<PushErpBackendProxy>(mPushErpMockBackend),
                                                     hsmPool, keyDerivation);
        };

        mPushExporterMockBackend = std::make_shared<testing::NiceMock<PushExporterMockBackend>>();
        factories.pushExporterDatabaseFactory =
            [this](HsmPool& hsmPool, KeyDerivation& keyDerivation) -> std::unique_ptr<PushExporterDatabase> {
            return std::make_unique<PushExporterDatabase>(
                std::make_unique<MockPushExporterDatabaseProxy>(mPushExporterMockBackend), hsmPool, keyDerivation);
        };
        return factories;
    }

    PushEventDataCollector createSampleEventData()
    {
        PushEventDataCollector collector;
        collector.setPrescriptionId(
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 123));
        collector.setNotificationIdentifier("abcde");
        collector.setHashedKvnr(db_model::HashedKvnr{1});
        collector.setChannelId(model::ChannelId::erp_chargeitem_create);
        collector.validate();
        EXPECT_TRUE(collector.isReadyForPushEvent());
        return collector;
    }

    std::shared_ptr<PushErpMockBackend> mPushErpMockBackend;
    std::shared_ptr<PushExporterMockBackend> mPushExporterMockBackend;
    std::unique_ptr<PcServiceContext> mServiceContext;
};

TEST_F(PushEventCreatorTest, createsPushEventWhenRegistered)
{
    using testing::_;
    ON_CALL(*mPushErpMockBackend, isPushRegistered(_, _)).WillByDefault(testing::Return(true));
    std::promise<void> p;
    std::future<void> f = p.get_future();
    EXPECT_CALL(*mPushExporterMockBackend, createPushEvent(testing::_)).WillOnce(::testing::Invoke([&]{
        p.set_value();
    }));
    mServiceContext->getPushEventCreator().tryPostCreatePushEvent(createSampleEventData(), *mServiceContext);
    auto res = f.wait_for(std::chrono::seconds{1});
    EXPECT_EQ(res, std::future_status::ready);
}

TEST_F(PushEventCreatorTest, skipsPushEventWhenNotRegistered)
{
    using testing::_;
    ON_CALL(*mPushErpMockBackend, isPushRegistered(_, _)).WillByDefault(testing::Return(false));
    EXPECT_CALL(*mPushExporterMockBackend, createPushEvent(testing::_)).Times(0);
    mServiceContext->getPushEventCreator().tryPostCreatePushEvent(createSampleEventData(), *mServiceContext);
    std::this_thread::sleep_for(std::chrono::seconds{1});
}

TEST_F(PushEventCreatorTest, discardsEventWhenQueueFull)
{
    using namespace std::chrono_literals;
    using testing::_;
    EnvironmentVariableGuard maxQueueGuard{ConfigurationKey::PUSH_EVENT_MAX_QUEUE, "3"};
    ON_CALL(*mPushErpMockBackend, isPushRegistered(_, _)).WillByDefault(testing::Return(true));
    std::promise<void> p;
    std::shared_future<void> f = p.get_future();
    std::latch l{3};
    std::atomic_size_t processing;
    ON_CALL(*mPushExporterMockBackend, createPushEvent(testing::_)).WillByDefault(::testing::Invoke([&] {
        ++processing;
        EXPECT_EQ(processing, 1) << "more than one event published at once (strand)";
        f.get();
        l.count_down();
        --processing;
    }));
    EXPECT_EQ(mServiceContext->getPushEventCreator().currentDepth(), 0);
    EXPECT_EQ(mServiceContext->getPushEventCreator().discardedSinceWarn(), 0);
    for (size_t i = 0; i < 6; ++i)
    {
        mServiceContext->getPushEventCreator().tryPostCreatePushEvent(createSampleEventData(), *mServiceContext);
    }
    testutils::waitFor([&]{return mServiceContext->getPushEventCreator().currentDepth() == 3;}, 1s);
    // only two because the first warning already resets counter zero
    testutils::waitFor([&]{return mServiceContext->getPushEventCreator().discardedSinceWarn() == 2;}, 1s);
    p.set_value();
    testutils::waitFor( [&] { return l.try_wait(); }, 1s);
}


TEST_F(PushEventCreatorTest, isPushRegisteredThrows)
{
    using namespace std::chrono_literals;
    using testing::_;
    EnvironmentVariableGuard maxQueueGuard{ConfigurationKey::PUSH_EVENT_MAX_QUEUE, "1"};
    std::promise<void> p;
    auto f = p.get_future();
    bool hasThrown = false;
    ON_CALL(*mPushErpMockBackend, isPushRegistered(_, _)).WillByDefault(testing::Invoke([&]() -> bool {
        f.get();
        hasThrown = true;
        throw std::runtime_error("Some DB problem");
    }));
    auto& pushCreator = mServiceContext->getPushEventCreator();
    pushCreator.tryPostCreatePushEvent(createSampleEventData(), *mServiceContext);
    testutils::waitFor( [&]() { return pushCreator.currentDepth() == 1; }, 1s);
    p.set_value();
    testutils::waitFor( [&]() { return pushCreator.currentDepth() == 0; }, 1s);
    EXPECT_TRUE(hasThrown);
}


TEST_F(PushEventCreatorTest, createPushEventThrows)
{
    using namespace std::chrono_literals;
    using testing::_;
    std::promise<void> p;
    auto f = p.get_future();
    bool hasThrown = false;
    ON_CALL(*mPushErpMockBackend, isPushRegistered(_, _)).WillByDefault(testing::Return(true));
    ON_CALL(*mPushExporterMockBackend, createPushEvent(_)).WillByDefault(::testing::Invoke([&] {
        f.get();
        hasThrown = true;
        throw std::runtime_error("Some DB problem");
    }));
    auto& pushCreator = mServiceContext->getPushEventCreator();
    pushCreator.tryPostCreatePushEvent(createSampleEventData(), *mServiceContext);
    testutils::waitFor( [&]() { return pushCreator.currentDepth() == 1; }, 1s);
    p.set_value();
    testutils::waitFor( [&]() { return pushCreator.currentDepth() == 0; }, 1s);
    EXPECT_TRUE(hasThrown);
}


class PushEventCreatorCrashTest : public PushEventCreatorTest
{
public:
    Factories factories() override
    {
        auto factories = PushEventCreatorTest::factories();
        factories.pushExporterDatabaseFactory = [this](HsmPool&, KeyDerivation&) -> std::unique_ptr<PushExporterDatabase>{
            f.get();
            hasThrown = true;
            try {
                Fail("root cause");
            }
            catch (...)
            {
                ErpFailWithDiagnostics(HttpStatus::BadRequest, "So Bad!", "Not see me in prod!");
            }
        };
        return factories;
    }
    std::promise<void> p;
    std::future<void> f = p.get_future();
    bool hasThrown = false;
};

TEST_F(PushEventCreatorCrashTest, pushExporterDatabaseFactoryThrows)
{
    using namespace std::chrono_literals;
    using testing::_;
    ON_CALL(*mPushErpMockBackend, isPushRegistered(_, _)).WillByDefault(testing::Return(true));
    auto& pushCreator = mServiceContext->getPushEventCreator();
    pushCreator.tryPostCreatePushEvent(createSampleEventData(), *mServiceContext);
    testutils::waitFor( [&]() { return pushCreator.currentDepth() == 1; }, 1s);
    p.set_value();
    testutils::waitFor( [&]() { return pushCreator.currentDepth() == 0; }, 1s);
    EXPECT_TRUE(hasThrown);
}
