/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/hsm/HsmSession.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "shared/hsm/production/HsmRawSession.hxx"
#include "shared/hsm/HsmSessionExpiredException.hxx"
#include "shared/hsm/production/HsmProductionClient.hxx"

#include "mock/hsm/HsmMockClient.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/util/HsmTestBase.hxx"

#include <gtest/gtest.h>

class HsmSessionReconnectTest : public testing::Test
{
};


/**
 * Verify that the HsmSessionExpiredException triggers a call to HsmClient::reconnect.
 */
TEST_F(HsmSessionReconnectTest, triggerReconnect_fail)//NOLINT(readability-function-cognitive-complexity)
{
    class TestClient : public HsmMockClient
    {
    public:
        size_t reconnectCallCount = 0;

        ErpVector getRndBytes (const HsmRawSession&, size_t) override
        {
            throw HsmSessionExpiredException("session expired", ERP_UTIMACO_SESSION_EXPIRED);
        }

        void reconnect (HsmRawSession&) override
        {
            ++reconnectCallCount;
        }
    };

    TestClient client;
    auto blobCache = MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm);
    auto session = HsmSession (client, *blobCache, {});

    ASSERT_EQ(client.reconnectCallCount, 0u);

    ASSERT_ANY_THROW(
        session.getRandomData(0));

    ASSERT_EQ(client.reconnectCallCount, 2u);
}


/**
 * Verify that the HsmSessionExpiredException triggers a call to HsmClient::reconnect.
 */
TEST_F(HsmSessionReconnectTest, triggerReconnect_successAfterReconnect)//NOLINT(readability-function-cognitive-complexity)
{
    class TestClient : public HsmMockClient
    {
    public:
        size_t reconnectCallCount = 0;

        ErpVector getRndBytes (const HsmRawSession& session, const size_t count) override
        {
            if (reconnectCallCount == 0)
                throw HsmSessionExpiredException("session expired", ERP_UTIMACO_SESSION_EXPIRED);
            else
                return HsmMockClient::getRndBytes(session, count);
        }

        void reconnect (HsmRawSession&) override
        {
            ++reconnectCallCount;
        }
    };

    TestClient client;
    auto blobCache = MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm);
    auto session = HsmSession (client, *blobCache, {});

    ASSERT_EQ(client.reconnectCallCount, 0u);

    ASSERT_NO_THROW(
        session.getRandomData(0));

    ASSERT_EQ(client.reconnectCallCount, 1u);
}


/**
 * Verify that the reconnect works.
 * For this we need a production HSM client that talks to a real or simulated HSM.
 */
TEST_F(HsmSessionReconnectTest, reconnect)
{
    if ( ! HsmTestBase().isHsmSimulatorSupportedAndConfigured())
        GTEST_SKIP();

    HsmProductionClient client;
    HsmRawSession rawSession;

    client.reconnect(rawSession);

    ASSERT_EQ(rawSession.rawSession.status, hsmclient::HSMSessionStatus_t::HSMLoggedIn);
}
