/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/hsm/HsmPoolSessionRemover.hxx"

#include "mock/hsm/HsmMockFactory.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/util/BlobDatabaseHelper.hxx"

#include <gtest/gtest.h>


class HsmPoolSessionRemoverTest : public testing::Test
{
public:
    void SetUp (void) override
    {
        BlobDatabaseHelper::removeUnreferencedBlobs();
    }
};



TEST_F(HsmPoolSessionRemoverTest, removeSession)
{
    std::unique_ptr<HsmSession> expected;
    HsmPoolSessionRemover remover ([&expected](std::unique_ptr<HsmSession>&& session){expected = std::move(session);});

    ASSERT_EQ(expected, nullptr);

    auto session = HsmMockFactory(std::make_unique<HsmMockClient>(),
                                  MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm)).connect();
    ASSERT_NE(session, nullptr);

    // Verify that removeSession ...
    remover.removeSession(std::move(session));

    // ... passes its argument via the callback into `expected`.
    ASSERT_NE(expected, nullptr);
    ASSERT_EQ(session, nullptr);
}




TEST_F(HsmPoolSessionRemoverTest, notifyPoolRelease)
{
    std::unique_ptr<HsmSession> expected;
    HsmPoolSessionRemover remover ([&expected](std::unique_ptr<HsmSession>&& session){expected = std::move(session);});

    ASSERT_EQ(expected, nullptr);

    auto session = HsmMockFactory(std::make_unique<HsmMockClient>(),
                                  MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm)).connect();
    ASSERT_NE(session, nullptr);

    // Verify that after a call to notifyPoolRelease ...
    remover.notifyPoolRelease();
    // ... the call to removeSession ...
    remover.removeSession(std::move(session));

    // ... will NOT pass its argument via the callback into `expected`.
    ASSERT_EQ(expected, nullptr);
    ASSERT_NE(session, nullptr);
}
