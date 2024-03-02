/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/DatabaseCodec.hxx"

#include "erp/compression/ZStd.hxx"
#include "erp/database/DatabaseModel.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/ByteHelper.hxx"

#include <gtest/gtest.h>

namespace
{
class TestSample
{
public:
    std::string_view const name;
    std::string_view const keyHex;
    std::string_view const ivHex;
    std::string_view const authenticationTagHex;
    std::string const plainText;
    std::string_view const ciperTextHex;
};
std::ostream& operator << (std::ostream& out, const TestSample& testSample)
{
    return out << testSample.name;
}


void append(db_model::EncryptedBlob& blob, const std::string& toAppend)
{
    blob.insert(blob.end(),
        reinterpret_cast<const std::byte*>(toAppend.data()),
        reinterpret_cast<const std::byte*>(toAppend.data()) + toAppend.size()
    );
}

}

class DatabaseCodecSampleTest : public ::testing::TestWithParam<TestSample>
{
public:
    class NoopCompression final : public Compression
    {
    public:
        std::string compress(std::string_view plain, Compression::DictionaryUse dict) const override
        {
            (void)dict;
            return std::string{plain};
        }

        std::string decompress(std::string_view compressed) const override
        {
            return std::string{compressed};
        }
    };

    static const std::shared_ptr<Compression>& getCompression()
    {
        static auto instance = createCompression();
        return instance;
    }
private:
    static std::shared_ptr<Compression> createCompression()
    {
        auto dir = Configuration::instance().getStringValue(ConfigurationKey::ZSTD_DICTIONARY_DIR);
        return std::make_shared<ZStd>(dir);
    }

};



TEST_P(DatabaseCodecSampleTest, roundTrip) // NOLINT
{
    using EncryptionT = DataBaseCodec::EncryptionT;
    const auto& param = GetParam();

    DataBaseCodec codec(std::make_shared<NoopCompression>(),[&](size_t size){
        [&]{ ASSERT_EQ(param.ivHex.size() / 2, size); }();
        return SafeString{ ByteHelper::fromHex(param.ivHex) };
    });
    SafeString key{ByteHelper::fromHex(param.keyHex)};
    db_model::EncryptedBlob encoded;
    ASSERT_NO_FATAL_FAILURE(encoded = codec.encode(param.plainText, key, Compression::DictionaryUse::Default_xml));
    ASSERT_LE(EncryptionT::IvLength + EncryptionT::AuthenticationTagLength, encoded.size());
    SafeString decoded;
    ASSERT_NO_FATAL_FAILURE(decoded = codec.decode(encoded, key));
    ASSERT_EQ(std::string{param.plainText}, std::string(std::string_view{decoded}));
}


TEST_P(DatabaseCodecSampleTest, compressedRoundTrip) // NOLINT
{
    using EncryptionT = DataBaseCodec::EncryptionT;
    const auto& param = GetParam();
    auto dir = Configuration::instance().getStringValue(ConfigurationKey::ZSTD_DICTIONARY_DIR);

    DataBaseCodec codec(getCompression(),[&](size_t size){
        [&]{ ASSERT_EQ(param.ivHex.size() / 2, size); }();
        return SafeString{ ByteHelper::fromHex(param.ivHex) };
    });
    SafeString key{ByteHelper::fromHex(param.keyHex)};
    db_model::EncryptedBlob encoded;
    ASSERT_NO_FATAL_FAILURE(encoded = codec.encode(param.plainText, key, Compression::DictionaryUse::Default_xml));
    ASSERT_LE(EncryptionT::IvLength + EncryptionT::AuthenticationTagLength, encoded.size());
    SafeString decoded;
    ASSERT_NO_FATAL_FAILURE(decoded = codec.decode(encoded, key));
    ASSERT_EQ(std::string{param.plainText}, std::string(std::string_view{decoded}));
}

TEST_P(DatabaseCodecSampleTest, encode) // NOLINT
{
    const auto& param = GetParam();

    std::string expected;
    expected.push_back(1); // Version
    expected.append(ByteHelper::fromHex(param.ivHex))
            .append(ByteHelper::fromHex(param.authenticationTagHex))
            .append(ByteHelper::fromHex(param.ciperTextHex));

    DataBaseCodec codec(std::make_shared<NoopCompression>(),[&](size_t size){
        [&]{ ASSERT_EQ(param.ivHex.size() / 2, size); }();
        return SafeString{ ByteHelper::fromHex(param.ivHex) };
    });
    SafeString key{ByteHelper::fromHex(param.keyHex)};
    auto encoded = codec.encode(param.plainText, key, Compression::DictionaryUse::Default_xml);
    EXPECT_EQ(ByteHelper::toHex(expected),
              ByteHelper::toHex(gsl::span<const char>(reinterpret_cast<const char*>(encoded.data()), encoded.size())));
}


TEST_P(DatabaseCodecSampleTest, decode) // NOLINT
{
    const auto& param = GetParam();
    db_model::EncryptedBlob blob;
    blob.push_back(std::byte(1)); // Version
    append(blob, ByteHelper::fromHex(param.ivHex));
    append(blob, ByteHelper::fromHex(param.authenticationTagHex));
    append(blob, ByteHelper::fromHex(param.ciperTextHex));

    DataBaseCodec codec(std::make_shared<NoopCompression>(), nullptr);
    SafeString key{ByteHelper::fromHex(param.keyHex)};
    auto decoded = codec.decode(blob, key);
    EXPECT_EQ(ByteHelper::toHex(param.plainText), ByteHelper::toHex(decoded));
}

class DatabaseCodecTest : public ::testing::Test
{
public:
    static constexpr std::string_view keyHex{"0000000000000000000000000000000000000000000000000000000000000000"};
    static constexpr std::string_view ivHex{"000000000000000000000000"};
    static constexpr std::string_view authenticationTagHex{"08a14a28d16058ffa752a1ffb11fff36"};
    static constexpr std::string_view cipherTextHex{"4bedb6a20f96f4fd8144a698"};

    db_model::EncryptedBlob makeBlob(uint8_t version)
    {
        db_model::EncryptedBlob blob;
        blob.push_back(std::byte(version));
        append(blob, ByteHelper::fromHex(ivHex));
        append(blob, ByteHelper::fromHex(authenticationTagHex));
        append(blob, ByteHelper::fromHex(cipherTextHex));
        return blob;
    }
    SafeString key{ByteHelper::fromHex(keyHex)};
};

TEST_F(DatabaseCodecTest, versionMismatch) // NOLINT
{
    DataBaseCodec codec(std::make_shared<DatabaseCodecSampleTest::NoopCompression>(), nullptr);
    EXPECT_THROW((void)codec.decode(makeBlob(2), key), std::logic_error);
}

TEST_F(DatabaseCodecTest, shortBlob) // NOLINT
{
    DataBaseCodec codec(std::make_shared<DatabaseCodecSampleTest::NoopCompression>(),
                        [](size_t){ return SafeString{ByteHelper::fromHex(ivHex)};});
    auto shortBlob = codec.encode("", key, Compression::DictionaryUse::Default_xml);
    // cipherText of encrypted text is empty with AES in GCM mode:
    ASSERT_EQ(DataBaseCodec::minEncodedLength, shortBlob.size());
    ASSERT_NO_THROW((void)codec.decode(shortBlob, key));
    shortBlob.resize(DataBaseCodec::minEncodedLength - 1);
    EXPECT_THROW((void)codec.decode(shortBlob, key), std::logic_error);
}



using namespace std::string_literals;
// test cases generated using: https://gchq.github.io/CyberChef/
INSTANTIATE_TEST_SUITE_P(sample, DatabaseCodecSampleTest,
    ::testing::Values(
        TestSample
        {
            "Empty String",
            "0000000000000000000000000000000000000000000000000000000000000000", // Key
            "000000000000000000000000", // IV
            "530f8afbc74536b9a963b4f1c4cb738b", // authenticationTag
            "", // plain
            "" // cipherText
        },
        TestSample
        {
            "Null Key and IV",
            "0000000000000000000000000000000000000000000000000000000000000000", // Key
            "000000000000000000000000", // IV
            "a0dd17908abb5af374bb1ebee6eea341", // authenticationTag
            "Hello World!"s, // plain
            "86c22c5122403c017522a1f2" // cipherText
        },
        TestSample
        {
            "Hello World!",
            "f59882fa246e5913d103e4a10ff79193f59882fa246e5913d103e4a10ff79193", // Key
            "5489730942d724748bb7428e", // IV
            "98b9105bbd788d14068a57cb023ba2d2", // authenticationTag
            "Hello World!"s, // plain
            "97b4c34e68cf7cdb24309610" // cipherText
        },
        TestSample
        {
            "null in plain text",
            "f59882fa246e5913d103e4a10ff79193f59882fa246e5913d103e4a10ff79193", // Key
            "5489730942d724748bb7428e", // IV
            "c59230c7e14a3ad57c652796168689af", // authenticationTag
            "With Null\0 and One\1"s, // plain, hex: 57697468204e756c6c0020616e64204f6e6501
            "88b8db4a27a15ed83a5cd2505f5e11d0a7c82c" // cipherText
        },
        TestSample
        {
            "encoded data contains a null byte",
            "f59882fa246e5913d103e4a10ff79193f59882fa246e5913d103e4a10ff79193", // Key
            "5489730942d724748bb7428e", // IV
            "76b3b331b86af7dc6e02b32a30ebf03a", // authenticationTag
            "zsndlensldneidlssh"s, // plain
            "a5a2c1466b8a45c73a389c54585e5decbac5" // cipherText
        }
    ));

/// the folowing test samples originate from
/// Version: openssl-3.0.0-alpha13-244-g8f81e3a184
/// File   : test/recipes/30-test_evp_data/evpciph_aes_common.txt
/// Git    : https://github.com/openssl/openssl
///
/// Note: All tests with additional authentication data or IV with lengths
///       other than 96 Bit where skipped.

INSTANTIATE_TEST_SUITE_P(opensslTest, DatabaseCodecSampleTest,
    ::testing::Values(
        TestSample
        {
            "openssl-1",
            "0000000000000000000000000000000000000000000000000000000000000000",
            "000000000000000000000000",
            "530f8afbc74536b9a963b4f1c4cb738b",
            "",
            "",
        },
        TestSample
        {
            "openssl-2",
            "0000000000000000000000000000000000000000000000000000000000000000",
            "000000000000000000000000",
            "d0d1c8a799996bf0265b98b5d48ab919",
            ByteHelper::fromHex("00000000000000000000000000000000"),
            "cea7403d4d606b6e074ec5d3baf39d18"
        },
        TestSample{
            "openssl-3",
            "feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308",
            "cafebabefacedbaddecaf888",
            "b094dac5d93471bdec1a502270e3cc6c",
            ByteHelper::fromHex("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255"),
            "522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa8cb08e48590dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f662898015ad"
        }
));
