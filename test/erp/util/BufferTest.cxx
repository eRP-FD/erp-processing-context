/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/Buffer.hxx"

#include <gtest/gtest.h>

#include <string>
#include <cstdint>


TEST(BufferTest, byteUnitMultiplesCalculation)
{
    EXPECT_EQ(1_B, 1u);
    EXPECT_EQ(1_KB, 1024u);
    EXPECT_EQ(1_MB, 1048576u);
    EXPECT_EQ(1_GB, 1073741824u);

    EXPECT_EQ(1_KB, 1024_B);
    EXPECT_EQ(1_MB, 1024_KB);
    EXPECT_EQ(1_GB, 1024_MB);
}


TEST(BufferTest, concatenateBuffers)
{
    // Given
    std::vector<std::string> c1{"a", "b", "c"};
    std::vector<std::string> c2{"d", "e", "f"};
    std::vector<std::string> c3{"g", "h", "i"};
    std::vector<std::string> expected{"a", "b", "c", "d", "e", "f", "g", "h", "i"};

    // When
    auto c1c2c3 = util::concatenateBuffers(std::move(c1), std::move(c2), std::move(c3));

    // Then
    EXPECT_EQ(c1c2c3, expected);

    for (const auto& movedFromContainerItr : {c1, c2, c3}) // NOLINT(bugprone-use-after-move)
    {
        for (const auto& stringItr : movedFromContainerItr)
        {
            EXPECT_TRUE(stringItr.empty());
        }
    }
}


TEST(BufferTest, stringToBufferToString)
{
    // Given
    std::string string{"Thïs strïng cöntäins ÜTF-8 💣"};

    // When
    auto buffer = util::stringToBuffer(string);

    // Then
    EXPECT_EQ(buffer.size(), string.size());

    // When
    std::string stringAgain = util::bufferToString(buffer);

    // Then
    EXPECT_EQ(stringAgain, string);
}


TEST(BufferTest, rawToBufferToString)
{
    // Given
    std::vector<std::uint8_t> binary{0x01, 0x23, 0x45, 0x56, 0x78, 0x9a, 0xbc, 0xef};

    // When
    auto buffer = util::rawToBuffer(binary.data(), binary.size());

    // Then
    EXPECT_EQ(binary, buffer);

    // When
    auto begin = util::bufferToRaw(buffer);
    std::vector<std::uint8_t> binaryAgain(begin, begin + buffer.size());

    // Then
    EXPECT_EQ(binaryAgain, buffer);

    // When
    const auto cbegin = util::bufferToRaw((const util::Buffer&)buffer);
    std::vector<std::uint8_t> cbinaryAgain(cbegin, cbegin + buffer.size());

    // Then
    EXPECT_EQ(cbinaryAgain, buffer);
}
