/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/Uuid.hxx"

#include <gtest/gtest.h>


static const unsigned int SAMPLE_SIZE = 10'000;


TEST(UuidTest, uuidsAreUnique)
{
    std::set<std::string> uuids;

    for (unsigned int i = 0; i < SAMPLE_SIZE; ++i)
    {
        uuids.emplace(Uuid().toString());
    }

    EXPECT_EQ(uuids.size(), SAMPLE_SIZE);
}


TEST(UuidTest, uuidFormat)//NOLINT(readability-function-cognitive-complexity)
{
    // This is the format we expect.
    EXPECT_TRUE(Uuid("e6f79c0b-8d08-40c9-88ee-6a157afac6c7").isValidIheUuid());
    EXPECT_TRUE(Uuid("733fa3dd-1c84-47bf-a758-f7a51442daef").isValidIheUuid());

    // Various invalid UUIDs (length mismatch, invalid characters, uppercase etc.)
    EXPECT_FALSE(Uuid("").isValidIheUuid());
    EXPECT_FALSE(Uuid("1eeb5bb9-7ada-4a53-af72-99613c482ee").isValidIheUuid());
    EXPECT_FALSE(Uuid("84def410e-4258-4702-b050-4c8e0363b96b").isValidIheUuid());
    EXPECT_FALSE(Uuid("d009982b-39d844-b6-88fe-5bf9eaec09ae").isValidIheUuid());
    EXPECT_FALSE(Uuid("zb526f78-0384-4888-8b6d-ac117d9247e3").isValidIheUuid());
    EXPECT_FALSE(Uuid("2E6E4D4F-68EE-43F4-A7A4-11879F21C62D").isValidIheUuid());

    EXPECT_FALSE(Uuid("urn:uuid:2E6E4D4F-68EE-43F4-A7A4-11879F21C62D").isValidIheUuid());

    // Our own generated UUIDs must of course follow our formatting requirements.
    for (unsigned int i = 0; i < SAMPLE_SIZE; ++i)
    {
        EXPECT_TRUE(Uuid().isValidIheUuid());
    }
}

TEST(UuidTest, uuidUrnFormat)//NOLINT(readability-function-cognitive-complexity)
{
    EXPECT_EQ(Uuid("e6f79c0b-8d08-40c9-88ee-6a157afac6c7").toUrn(), "urn:uuid:e6f79c0b-8d08-40c9-88ee-6a157afac6c7");
    EXPECT_EQ(Uuid("urn:uuid:e6f79c0b-8d08-40c9-88ee-6a157afac6c7").toUrn(), "urn:uuid:e6f79c0b-8d08-40c9-88ee-6a157afac6c7");
    EXPECT_EQ(Uuid("e6f79c0b-8d08-40c9-88ee-6a157afac6c7"), Uuid("urn:uuid:e6f79c0b-8d08-40c9-88ee-6a157afac6c7"));
    EXPECT_FALSE(Uuid("e6f79c0b-8d08-40c9-88ee-6a157afac6c7") < Uuid("urn:uuid:e6f79c0b-8d08-40c9-88ee-6a157afac6c7"));
    EXPECT_FALSE(Uuid("urn:uuid:e6f79c0b-8d08-40c9-88ee-6a157afac6c7") < Uuid("e6f79c0b-8d08-40c9-88ee-6a157afac6c7"));
    EXPECT_EQ(Uuid("huhu").toUrn(), "huhu");
}

TEST(UuidTest, toOid)//NOLINT(readability-function-cognitive-complexity)
{
    EXPECT_NO_THROW(Uuid().toOid());
    EXPECT_FALSE(Uuid().toOid().empty());
    EXPECT_EQ(Uuid("e6f79c0b-8d08-40c9-88ee-6a157afac6c7").toOid(), Uuid("urn:uuid:e6f79c0b-8d08-40c9-88ee-6a157afac6c7").toOid());
    EXPECT_EQ(Uuid("e6f79c0b-8d08-40c9-88ee-6a157afac6c7").toOid(),"2.25.307008101325644074250100480428591466183");
}
