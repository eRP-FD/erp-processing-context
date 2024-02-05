/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Kvnr.hxx"
#include "erp/model/Iknr.hxx"
#include "erp/model/Lanr.hxx"
#include "erp/model/Pzn.hxx"

#include <gtest/gtest.h>


TEST(KvnrTest, isKvnr)
{
    ASSERT_TRUE(model::Kvnr::isKvnr("X123456788"));
    ASSERT_TRUE(model::Kvnr::isKvnr("X234567890"));
    ASSERT_FALSE(model::Kvnr::isKvnr("XY34567890"));
    ASSERT_FALSE(model::Kvnr::isKvnr("X3456789"));
    ASSERT_FALSE(model::Kvnr::isKvnr("a123456789"));
}

TEST(KvnrTest, checksum)
{
    using model::Kvnr;
    EXPECT_TRUE(Kvnr{"A000500015"}.validChecksum());
    EXPECT_TRUE(Kvnr{"C000500021"}.validChecksum());
    EXPECT_TRUE(Kvnr{"B123457897"}.validChecksum());
    EXPECT_TRUE(Kvnr{"C123457899"}.validChecksum());
    EXPECT_TRUE(Kvnr{"B043496447"}.validChecksum());
    EXPECT_TRUE(Kvnr{"U029243874"}.validChecksum());
    EXPECT_TRUE(Kvnr{"Z111122223"}.validChecksum());
    EXPECT_TRUE(Kvnr{"A123457895"}.validChecksum());
    EXPECT_TRUE(Kvnr{"X234567891"}.validChecksum());
    EXPECT_TRUE(Kvnr{"X123456788"}.validChecksum());
    EXPECT_TRUE(Kvnr{"X500000017"}.validChecksum());
    EXPECT_TRUE(Kvnr{"X500000029"}.validChecksum());
    EXPECT_TRUE(Kvnr{"X100000013"}.validChecksum());
}

TEST(IknrTest, checksum)
{
    using model::Iknr;
    EXPECT_TRUE(Iknr{"109911114"}.validChecksum());
    EXPECT_TRUE(Iknr{"260326822"}.validChecksum());

}

TEST(LanrTest, checksum)
{
    using model::Lanr;
    EXPECT_TRUE(Lanr{"000000000"}.validChecksum());
    EXPECT_TRUE(Lanr{"000001100"}.validChecksum());
    EXPECT_TRUE(Lanr{"000010600"}.validChecksum());
    EXPECT_TRUE(Lanr{"111111100"}.validChecksum());
    EXPECT_TRUE(Lanr{"123456699"}.validChecksum());
    EXPECT_TRUE(Lanr{"333333300"}.validChecksum());
    EXPECT_TRUE(Lanr{"444444400"}.validChecksum());
    EXPECT_TRUE(Lanr{"444444455"}.validChecksum());
    EXPECT_TRUE(Lanr{"555555500"}.validChecksum());
    EXPECT_TRUE(Lanr{"555555999"}.validChecksum());
    EXPECT_TRUE(Lanr{"654321199"}.validChecksum());
    EXPECT_TRUE(Lanr{"999999900"}.validChecksum());
    EXPECT_TRUE(Lanr{"999999991"}.validChecksum());
}

TEST(PznTest, checksum)
{
    using model::Pzn;
    EXPECT_TRUE(Pzn{"27580899"}.validChecksum());
    EXPECT_TRUE(Pzn{"12345678"}.validChecksum());
    EXPECT_TRUE(Pzn{"10000001"}.validChecksum());
    EXPECT_TRUE(Pzn{"12345678"}.validChecksum());
    EXPECT_FALSE(Pzn{"27580891"}.validChecksum());
    EXPECT_TRUE(Pzn{"06461110"}.validChecksum());
    EXPECT_TRUE(Pzn{"00271348"}.validChecksum());
}
