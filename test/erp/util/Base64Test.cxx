#include <gtest/gtest.h>

#include "erp/util/Base64.hxx"
#include "erp/util/Random.hxx"

#include <stdexcept>
#include <string>


using namespace std::literals::string_literals;


class Base64Test : public testing::Test
{
public:
};


TEST_F(Base64Test, decodeString)
{
    // Given
    std::string input{"bXkgdmVyeSBzcGVjaWFsIHRlc3Q="};
    std::string expected{"my very special test"};

    // When
    std::string result = util::bufferToString(Base64::decode(input));

    // Then
    EXPECT_EQ(expected, result);
}


TEST_F(Base64Test, decodeUrlVariant)
{
    // Given
    std::string input{"4oiC4oKsW-KInuKCrMK1wqniiILGksK_eA"};
    std::string expected{"∂€[∞€µ©∂ƒ¿x"};
    std::string expectedOutput{"4oiC4oKsW+KInuKCrMK1wqniiILGksK/eA=="};

    // When
    std::string decodedResult = Base64::decodeToString(input);
    std::string encodedResult = Base64::encode(decodedResult);

    // Then
    EXPECT_EQ(decodedResult, expected);
    EXPECT_EQ(encodedResult, expectedOutput);
}


TEST_F(Base64Test, encodeString)
{
    // Given
    std::string input{"my very special test"};
    std::string expected{"bXkgdmVyeSBzcGVjaWFsIHRlc3Q="};

    // When
    std::string result = Base64::encode(input);

    // Then
    EXPECT_EQ(expected, result);
}


TEST_F(Base64Test, encodePadding)
{
    EXPECT_EQ("", Base64::encode(""));
    EXPECT_EQ("Zg==", Base64::encode("f"));
    EXPECT_EQ("Zm8=", Base64::encode("fo"));
    EXPECT_EQ("Zm9v", Base64::encode("foo"));
    EXPECT_EQ("Zm9vYg==", Base64::encode("foob"));
    EXPECT_EQ("Zm9vYmE=", Base64::encode("fooba"));
    EXPECT_EQ("Zm9vYmFy", Base64::encode("foobar"));
}


TEST_F(Base64Test, encodeDecodeRoundTrip)
{
    util::Buffer binaryData = Random::randomBinaryData(2_MB);

    const auto encoded = Base64::encode(binaryData);

    EXPECT_EQ(binaryData, Base64::decode(encoded));
}


TEST_F(Base64Test, decodeErrors)
{
    // Invalid characters.
    EXPECT_THROW(Base64::decode("ABC`"), std::invalid_argument);
    EXPECT_THROW(Base64::decode(std::string(4, '\0')), std::invalid_argument);

    // Invalid padding.
    EXPECT_THROW(Base64::decode("====`"), std::invalid_argument);
    EXPECT_THROW(Base64::decode("Zm9v====`"), std::invalid_argument);
    EXPECT_THROW(Base64::decode("=m9v"), std::invalid_argument);
}


TEST_F(Base64Test, variableLengthRoundTrip)
{
    // The trickiest part about Base64 is that it requires padding with == for some input lengths.
    // To make sure all lengths survive the round trip to and from Base64, run a little loop:
    for (std::size_t length = 0; length < 1024; ++length)
    {
        util::Buffer input = Random::randomBinaryData(length);
        std::string base64 = Base64::encode(input);
        util::Buffer output = Base64::decode(base64);
        EXPECT_EQ(input, output);
    }
}


TEST_F(Base64Test, adjustSizeBeforeEncoding)
{
    for (const size_t size : {0, 1, 2, 3, 4, 5, 1023, 1024, 1025, 1026, 1024*1024-1, 1024*1024, 1024*1024+1, 1024*1024+2})
    {
        const std::string s (size, 'X');
        const std::string encoded = Base64::encode(s);

        EXPECT_EQ(Base64::adjustSizeForEncoding(size), encoded.size())
                        << "for size " << size;
    }
}


TEST_F(Base64Test, adjustSizeBeforeDecoding)
{
    for (const size_t size : {0, 1, 2, 3, 4, 5, 1023, 1024, 1025, 1026, 1024*1024-1, 1024*1024, 1024*1024+1, 1024*1024+2})
    {
        const std::string s (size, 'X');
        const std::string encoded = Base64::encode(s);

        const size_t paddingSize = encoded.size() - encoded.find_last_not_of('=') - 1;
        const size_t dataSize = encoded.size() - paddingSize;

        ASSERT_EQ(Base64::adjustSizeForDecoding(dataSize, paddingSize), size)
                            << "for size " << size;
    }
}


TEST_F(Base64Test, cleanupForDecoding)
{
    const std::string data = Base64::cleanupForDecoding("1 2\t3\v4\f5\r6\n7\08"s);
    ASSERT_EQ(data, "12345678");
}


TEST_F(Base64Test, toBase64Url)
{
    ASSERT_EQ(Base64::toBase64Url("bXkgdmVy+/BzcGVjaWFsIHRlc3Q="), "bXkgdmVy-_BzcGVjaWFsIHRlc3Q");
}
