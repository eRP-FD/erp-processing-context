#include "erp/hsm/HsmPoolSessionRemover.hxx"

#include "mock/hsm/HsmMockFactory.hxx"
#include "test/util/BlobDatabaseHelper.hxx"

#include <gtest/gtest.h>


class HsmPoolSessionRemoverTest : public testing::Test
{
public:
    virtual void SetUp (void) override
    {
        BlobDatabaseHelper::clearBlobDatabase();
    }
};



TEST_F(HsmPoolSessionRemoverTest, removeSession)
{
    std::unique_ptr<HsmSession> expected;
    HsmPoolSessionRemover remover ([&expected](std::unique_ptr<HsmSession>&& session){expected = std::move(session);});

    ASSERT_EQ(expected, nullptr);

    auto session = HsmMockFactory().connect();
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

    auto session = HsmMockFactory().connect();
    ASSERT_NE(session, nullptr);

    // Verify that after a call to notifyPoolRelease ...
    remover.notifyPoolRelease();
    // ... the call to removeSession ...
    remover.removeSession(std::move(session));

    // ... will NOT pass its argument via the callback into `expected`.
    ASSERT_EQ(expected, nullptr);
    ASSERT_NE(session, nullptr);
}
