#include "erp/hsm/HsmSession.hxx"

#include "erp/hsm/HsmIdentity.hxx"
#include "erp/hsm/production/HsmRawSession.hxx"
#include "erp/hsm/HsmSessionExpiredException.hxx"
#include "erp/hsm/production/HsmProductionClient.hxx"

#include "mock/hsm/HsmMockClient.hxx"
#include "mock/hsm/MockBlobCache.hxx"
#include "test/util/HsmTestBase.hxx"


#include <gtest/gtest.h>


class HsmSessionReconnectTest : public testing::Test
{
};


/**
 * Verify that the HsmSessionExpiredException triggers a call to HsmClient::reconnect.
 */
TEST_F(HsmSessionReconnectTest, triggerReconnect_fail)
{
    class TestClient : public HsmMockClient
    {
    public:
        size_t reconnectCallCount = 0;

        virtual ErpVector getRndBytes (const HsmRawSession&, size_t) override
        {
            throw HsmSessionExpiredException("session expired", ERP_UTIMACO_SESSION_EXPIRED);
        }

        virtual void reconnect (HsmRawSession&) override
        {
            ++reconnectCallCount;
        }
    };

    TestClient client;
    auto blobCache = MockBlobCache::createBlobCache(MockBlobCache::MockTarget::MockedHsm);
    auto session = HsmSession (client, *blobCache, {});

    ASSERT_EQ(client.reconnectCallCount, 0u);

    ASSERT_ANY_THROW(
        session.getRandomData(0));

    ASSERT_EQ(client.reconnectCallCount, 2u);
}


/**
 * Verify that the HsmSessionExpiredException triggers a call to HsmClient::reconnect.
 */
TEST_F(HsmSessionReconnectTest, triggerReconnect_successAfterReconnect)
{
    class TestClient : public HsmMockClient
    {
    public:
        size_t reconnectCallCount = 0;

        virtual ErpVector getRndBytes (const HsmRawSession& session, const size_t count) override
        {
            if (reconnectCallCount == 0)
                throw HsmSessionExpiredException("session expired", ERP_UTIMACO_SESSION_EXPIRED);
            else
                return HsmMockClient::getRndBytes(session, count);
        }

        virtual void reconnect (HsmRawSession&) override
        {
            ++reconnectCallCount;
        }
    };

    TestClient client;
    auto blobCache = MockBlobCache::createBlobCache(MockBlobCache::MockTarget::MockedHsm);
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
    if ( ! HsmTestBase::isHsmSimulatorSupportedAndConfigured())
        GTEST_SKIP();

    HsmProductionClient client;
    HsmRawSession rawSession;

    client.reconnect(rawSession);

    ASSERT_EQ(rawSession.rawSession.status, hsmclient::HSMSessionStatus_t::HSMLoggedIn);
}
