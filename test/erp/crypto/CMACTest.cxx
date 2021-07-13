#include <gtest/gtest.h>

#include "erp/crypto/CMAC.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/SafeString.hxx"

#include <optional>

class CmacTest : public ::testing::Test
{
public:
};

TEST_F(CmacTest, CmacSignature_fromBin) // NOLINT
{
    std::string_view testData{"1234567890123456"};
    const auto& sig = CmacSignature::fromBin(testData);
    EXPECT_EQ((std::string_view{reinterpret_cast<const char*>(sig.data()), sig.size()}), testData);
}

TEST_F(CmacTest, CmacSignature_fromHex) // NOLINT
{
    std::string_view testHex{"11223344556677889900112233445566"};
    char testData[] = "\x11\x22\x33\x44\x55\x66\x77\x88\x99\x00\x11\x22\x33\x44\x55\x66";
    const auto& sig = CmacSignature::fromHex(std::string{testHex});
    EXPECT_EQ((std::string_view{reinterpret_cast<const char*>(sig.data()), sig.size()}), (std::string_view{testData, std::size(testData) - 1}));
}

TEST_F(CmacTest, CmacSignature_hex) // NOLINT
{
    std::string_view testHex{"11223344556677889900112233445566"};
    const auto& sig = CmacSignature::fromHex(std::string{testHex});
    EXPECT_EQ(sig.hex(), testHex);
}

TEST_F(CmacTest, CmacSignature_equal_operator) // NOLINT
{
    std::string_view testHex1{"11223344556677889900112233445566"};
    std::string_view testHex2{"00112233445566778899001122334455"};
    const auto& sig1a = CmacSignature::fromHex(std::string{testHex1});
    const auto& sig1b = CmacSignature::fromHex(std::string{testHex1});
    const auto& sig2 = CmacSignature::fromHex(std::string{testHex2});
    EXPECT_TRUE(sig1a == sig1a);
    EXPECT_TRUE(sig1a == sig1b);
    EXPECT_FALSE(sig1a == sig2);
}

TEST_F(CmacTest, CmacKey_random) // NOLINT
{
    using namespace std::string_view_literals;
    class MockRandomGenerator : public RandomSource
    {
    public:
        std::string_view testData = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"sv;
        SafeString randomBytes(size_t bytes) override
        {
            [&]{ASSERT_EQ(bytes, testData.size());}();
            return SafeString(std::string{testData});
        }
    };
    std::optional<CmacKey> key;
    MockRandomGenerator mockRandomGenerator;
    ASSERT_NO_FATAL_FAILURE(key = CmacKey::randomKey(mockRandomGenerator));
    EXPECT_EQ((std::string_view{reinterpret_cast<const char*>(key->data()), key->size()}), mockRandomGenerator.testData);
}

TEST_F(CmacTest, CmacKey_fromBin) // NOLINT
{
    std::string_view testData{"1234567890123456"};
    const auto& key = CmacKey::fromBin(testData);
    EXPECT_EQ((std::string_view{reinterpret_cast<const char*>(key.data()), key.size()}), testData);
}

TEST_F(CmacTest, sign) // NOLINT
{
    // reference generated online:
    // https://artjomb.github.io/cryptojs-extension/
    std::string_view message{"11223344556677889900112233445566"};
    std::string_view keyHex{"00112233445566778899001122334455"};
    std::string keyBin{ByteHelper::fromHex(std::string{keyHex})};
    auto expected = CmacSignature::fromHex("8cabbaa9845cd07b876002257cbe92f4");
    auto key = CmacKey::fromBin(keyBin);
    auto actual = key.sign(message);
    EXPECT_EQ(actual, expected);
}
