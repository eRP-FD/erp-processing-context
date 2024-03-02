/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/compression/ZStd.hxx"

#include "erp/util/Base64.hxx"
#include "erp/util/Configuration.hxx"
#include "test_config.h"

#include <boost/algorithm/string.hpp>
#include <gtest/gtest.h>

using namespace std::string_literals;

TEST(ZStdTest, load)
{
    auto dir = Configuration::instance().getStringValue(ConfigurationKey::ZSTD_DICTIONARY_DIR);
    EXPECT_NO_THROW(ZStd{dir});
}

TEST(ZStdTest, missingDictionary)
{
    try
    {
        auto dir = std::filesystem::path{TEST_DATA_DIR} / "zstd" / "missingDictionary";
        (void)ZStd{dir};
        FAIL() << "should throw an exception";
    }
    catch (const std::runtime_error& e)
    {
        EXPECT_TRUE(boost::starts_with(e.what(), "Not all necessary dictionaries found in"s)) << "Actual: " << e.what();
    }
}

TEST(ZStdTest, extraDictionary)
{
    try
    {
        auto dir = std::filesystem::path{TEST_DATA_DIR} / "zstd" / "extraDictionary";
        (void)ZStd{dir};
        FAIL() << "should throw an exception";
    }
    catch (const std::logic_error& e)
    {
        EXPECT_TRUE(boost::ends_with(e.what(), R"(Got unexpected dictionary: { "use": "Default_xml", "version": 15})"s)) << "Actual: " << e.what();
    }
}

TEST(ZStdTest, modifiedDictionary)
{
    try
    {
        auto dir = std::filesystem::path{TEST_DATA_DIR} / "zstd" / "modifiedDictionary";
        (void)ZStd{dir};
        FAIL() << "should throw an exception";
    }
    catch (const std::logic_error& e)
    {
        EXPECT_TRUE(boost::ends_with(e.what(), R"(Sha256 mismatch for dictionary: { "use": "Default_xml", "version": 0})"s)) << "Actual: " << e.what();
    }
}

TEST(ZStdTest, duplicateDictionary)
{
    try
    {
        auto dir = std::filesystem::path{TEST_DATA_DIR} / "zstd" / "duplicateDictionary";
        (void)ZStd{dir};
        FAIL() << "should throw an exception";
    }
    catch (const std::logic_error& e)
    {
        EXPECT_TRUE(boost::ends_with(e.what(), R"(Got unexpected dictionary: { "use": "Default_xml", "version": 0})"s)) << "Actual: " << e.what();
    }
}

namespace {
class TestSample final
{
public:
    Compression::DictionaryUse dictUse;
    std::string_view plain;
    std::string_view compressedBase64;
};

std::ostream& operator << (std::ostream& out, const TestSample& sample)
{
    out << R"({ "dictUse": ")" << sample.dictUse << R"(", "plain": ")" << sample.plain << R"(" })";
    return out;
}

}

class ZStdSampleTest : public ::testing::TestWithParam<TestSample> {};

TEST_P(ZStdSampleTest, roundTrip)//NOLINT(readability-function-cognitive-complexity)
{
    auto dir = Configuration::instance().getStringValue(ConfigurationKey::ZSTD_DICTIONARY_DIR);
    ZStd compressor{dir};
    ZStd decompressor{dir};
    const auto& p = GetParam();
    std::string compressed;
    ASSERT_NO_THROW(compressed = compressor.compress(p.plain, p.dictUse));
    auto id = ZStdDictionary::IdType::fromFrame(compressed);
    EXPECT_EQ(id.use(), p.dictUse);
    std::string plain;
    ASSERT_NO_THROW(plain = decompressor.decompress(compressed));
    EXPECT_EQ(p.plain, plain);
}

TEST_P(ZStdSampleTest, decompress)
{
   auto dir = Configuration::instance().getStringValue(ConfigurationKey::ZSTD_DICTIONARY_DIR);
    ZStd decompressor{dir};
    const auto& p = GetParam();
    std::string plain;
    auto compressed = Base64::decodeToString(std::string{p.compressedBase64});
    auto id = ZStdDictionary::IdType::fromFrame(compressed);
    EXPECT_EQ(id.use(), p.dictUse);
    ASSERT_NO_THROW(plain = decompressor.decompress(compressed));
    EXPECT_EQ(p.plain, plain);
}

using namespace std::string_view_literals;
INSTANTIATE_TEST_SUITE_P(dataSet, ZStdSampleTest , ::testing::Values(
    TestSample{Compression::DictionaryUse::Default_json, ""sv                     , "KLUv/SUCAAEAAJnp2FE="sv                        },
    TestSample{Compression::DictionaryUse::Default_xml , ""sv                     , "KLUv/SUBAAEAAJnp2FE="sv                        },
    TestSample{Compression::DictionaryUse::Default_json, "Hello World!"sv         , "KLUv/SUCDGEAAEhlbGxvIFdvcmxkIZFNfz4="sv        },
    TestSample{Compression::DictionaryUse::Default_xml , "Hello World!"sv         , "KLUv/SUBDGEAAEhlbGxvIFdvcmxkIZFNfz4="sv        },
    TestSample{Compression::DictionaryUse::Default_json, "With Null\0 and One\1"sv, "KLUv/SUCE5kAAFdpdGggTnVsbAAgYW5kIE9uZQHO8oKv"sv},
    TestSample{Compression::DictionaryUse::Default_xml , "With Null\0 and One\1"sv, "KLUv/SUBE5kAAFdpdGggTnVsbAAgYW5kIE9uZQHO8oKv"sv}
));
