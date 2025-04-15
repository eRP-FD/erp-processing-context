/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/Hash.hxx"
#include "shared/util/ByteHelper.hxx"
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

TEST(HashTest, sha384)
{
    // Example taken from https://en.wikipedia.org/wiki/SHA-2#Test_vectors
    EXPECT_EQ(ByteHelper::toHex(Hash::sha384(gsl::span<const char>{nullptr, 0L})),
              "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b");
    // Expected values taken from macOS' sha512sum utility.
    EXPECT_EQ(ByteHelper::toHex(Hash::sha384(dog)),
              "ca737f1014a48f4c0b6dd43cb177b0afd9e5169367544c494011e3317dbf9a509cb1e5dc1e85a941bbee3d7f2afbc9b1");
    EXPECT_EQ(ByteHelper::toHex(Hash::sha384(cog)),
              "098cea620b0978caa5f0befba6ddcf22764bea977e1c70b3483edfdf1de25f4b40d6cea3cadf00f809d422feb1f0161b");
}


TEST(HashTest, sha512)
{
    // Example taken from https://en.wikipedia.org/wiki/SHA-2#Test_vectors
    EXPECT_EQ(ByteHelper::toHex(Hash::sha512(gsl::span<const char>{nullptr, 0L})),
              "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd"
              "47417a81a538327af927da3e");
    // Expected values taken from macOS' sha512sum utility.
    EXPECT_EQ(ByteHelper::toHex(Hash::sha512(dog)), "07e547d9586f6a73f73fbac0435ed76951218fb7d0c8d788a309d785436bbb642e"
                                                    "93a252a954f23912547d1e8a3b5ed6e1bfd7097821233fa0538f3db854fee6");
    EXPECT_EQ(ByteHelper::toHex(Hash::sha512(cog)), "3eeee1d0e11733ef152a6c29503b3ae20c4f1f3cda4cb26f1bc1a41f91c7fe4ab3"
                                                    "bd86494049e201c4bd5155f31ecb7a3c8606843c4cc8dfcab7da11c8ae5045");
}
