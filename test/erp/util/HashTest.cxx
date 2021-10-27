/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/Hash.hxx"
#include "erp/util/ByteHelper.hxx"
#include <gtest/gtest.h>

namespace
{
    const std::string dog = "The quick brown fox jumps over the lazy dog";
    const std::string cog = "The quick brown fox jumps over the lazy cog";
}


TEST(HashTest, sha1)
{
    // Examples taken from https://en.wikipedia.org/wiki/SHA-1#Examples_and_pseudocode
    EXPECT_EQ(ByteHelper::toHex(Hash::sha1({nullptr, 0L})),
              "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    EXPECT_EQ(ByteHelper::toHex(Hash::sha1(dog)), "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");
    EXPECT_EQ(ByteHelper::toHex(Hash::sha1(cog)), "de9f2c7fd25e1b3afad3e85a0bd17d9b100db4b3");
}

TEST(HashTest, sha256)
{
    // Example taken from https://en.wikipedia.org/wiki/SHA-2#Test_vectors
    EXPECT_EQ(ByteHelper::toHex(Hash::sha256(gsl::span<const char>{nullptr, 0L})),
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    // Expected values taken from macOS' sha256sum utility.
    EXPECT_EQ(ByteHelper::toHex(Hash::sha256(dog)),
              "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
    EXPECT_EQ(ByteHelper::toHex(Hash::sha256(cog)),
              "e4c4d8f3bf76b692de791a173e05321150f7a345b46484fe427f6acc7ecc81be");
}
