/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/crypto/Seeder.hxx"

#include "erp/hsm/HsmPool.hxx"

#include "mock/hsm/HsmMockFactory.hxx"

#include <gtest/gtest.h>


TEST(SeederTest, seeds)
{
    static constexpr size_t threads = 20;
    std::string seedBlock;
    while (Seeder::seedBlockSize > seedBlock.size())
    {
        seedBlock.append(HsmMockClient::defaultRandomData);
    }
    seedBlock.resize(Seeder::seedBlockSize);
    std::string expectedSeeds;
    while (Seeder::seedBytes * threads > expectedSeeds.size())
    {
        expectedSeeds.append(seedBlock);
    }
    HsmPool mockHsmPool(
        std::make_unique<HsmMockFactory>(),
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory());
    Seeder seeder(mockHsmPool);
    for (size_t i = 0; i < threads; ++i)
    {
        const auto& seed = seeder.getNextSeed();
        EXPECT_EQ(seed.size(), Seeder::seedBytes);
        SafeString expected{expectedSeeds.substr(i * Seeder::seedBytes, Seeder::seedBytes)};
        EXPECT_EQ(seed, expected);
    }
}
