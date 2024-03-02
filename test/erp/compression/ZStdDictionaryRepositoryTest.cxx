/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/compression/ZStdDictionaryRepository.hxx"

#include "test/erp/compression/ZStdTestHelper.hxx"

#include <gtest/gtest.h>

using DictVersion = uint8_t;
using DictUseAndVersion = std::tuple<Compression::DictionaryUse, DictVersion>;

TEST(ZStdDictionaryRepositoryTest, VersionRange)//NOLINT(readability-function-cognitive-complexity)
{
    std::set<DictUseAndVersion> samples = {
        {Compression::DictionaryUse::Default_json, static_cast<DictVersion>( 0) },
        {Compression::DictionaryUse::Default_json, static_cast<DictVersion>( 1) },
        {Compression::DictionaryUse::Default_json, static_cast<DictVersion>(14) },
        {Compression::DictionaryUse::Default_json, static_cast<DictVersion>(15) },
        {Compression::DictionaryUse::Default_xml , static_cast<DictVersion>( 0) },
        {Compression::DictionaryUse::Default_xml , static_cast<DictVersion>( 1) },
        {Compression::DictionaryUse::Default_xml , static_cast<DictVersion>(14) },
        {Compression::DictionaryUse::Default_xml , static_cast<DictVersion>(15) }
    };
    std::vector dictUses = {Compression::DictionaryUse::Default_json, Compression::DictionaryUse::Default_xml};
    ZStdDictionaryRepository repo;
    for (const auto& [dictUse, dictVersion]: samples)
    {
        repo.addDictionary(std::make_unique<ZStdDictionary>(ZStdDictionary::fromBin(sampleDictionary(dictUse, dictVersion))));
    }
    for (const auto& dictUse: dictUses)
    {
        const ZStdDictionary* dictByUse = nullptr;
        EXPECT_NO_THROW(dictByUse = &repo.getDictionaryForUse(dictUse));
        ASSERT_NE(dictByUse, nullptr);
        EXPECT_EQ(dictByUse->id().use(), dictUse);
        for (uint8_t ver = 0; ver < 16; ++ver)
        {
            if (samples.count({dictUse, ver}))
            {
                const ZStdDictionary* dict = nullptr;
                EXPECT_NO_THROW(dict = &repo.getDictionary({dictUse, ver}));
                ASSERT_NE(dict, nullptr);
                EXPECT_EQ(dict->id().use(), dictUse);
                EXPECT_EQ(dict->id().version(), ver);
            }
            else
            {
                EXPECT_THROW(repo.getDictionary({dictUse, ver}), std::logic_error);
            }
        }
    }
}


TEST(ZStdDictionaryRepositoryTest, latestVersion)//NOLINT(readability-function-cognitive-complexity)
{
    std::set<DictUseAndVersion> samples = {
        {Compression::DictionaryUse::Default_json, static_cast<DictVersion>( 0) },
        {Compression::DictionaryUse::Default_json, static_cast<DictVersion>( 1) },
        {Compression::DictionaryUse::Default_json, static_cast<DictVersion>(12) },
        {Compression::DictionaryUse::Default_xml , static_cast<DictVersion>( 0) },
        {Compression::DictionaryUse::Default_xml , static_cast<DictVersion>( 1) },
        {Compression::DictionaryUse::Default_xml , static_cast<DictVersion>(14) },
    };
    std::vector dictUses = {Compression::DictionaryUse::Default_json, Compression::DictionaryUse::Default_xml};
    ZStdDictionaryRepository repo;
    for (const auto& [dictUse, dictVersion]: samples)
    {
        repo.addDictionary(std::make_unique<ZStdDictionary>(ZStdDictionary::fromBin(sampleDictionary(dictUse, dictVersion))));
    }

    { // get latest dictionary for Default_json
        const ZStdDictionary* dict = nullptr;
        ASSERT_NO_THROW(dict = &repo.getDictionaryForUse(Compression::DictionaryUse::Default_json));
        ASSERT_NE(dict, nullptr);
        EXPECT_EQ(dict->id().use(), Compression::DictionaryUse::Default_json);
        EXPECT_EQ(dict->id().version(), 12);
    }
    { // get latest dictionary for Default_xml
        const ZStdDictionary* dict = nullptr;
        ASSERT_NO_THROW(dict = &repo.getDictionaryForUse(Compression::DictionaryUse::Default_xml));
        ASSERT_NE(dict, nullptr);
        EXPECT_EQ(dict->id().use(), Compression::DictionaryUse::Default_xml);
        EXPECT_EQ(dict->id().version(), 14);
    }
}

TEST(ZStdDictionaryRepositoryTest, undefined)
{
    std::set<DictUseAndVersion> samples = {
        {Compression::DictionaryUse::Default_json, static_cast<DictVersion>( 0) },
        {Compression::DictionaryUse::Default_json, static_cast<DictVersion>( 1) },
        {Compression::DictionaryUse::Default_json, static_cast<DictVersion>(12) },
        {Compression::DictionaryUse::Default_xml , static_cast<DictVersion>( 0) },
        {Compression::DictionaryUse::Default_xml , static_cast<DictVersion>( 1) },
        {Compression::DictionaryUse::Default_xml , static_cast<DictVersion>(14) },
    };
    std::vector dictUses = {Compression::DictionaryUse::Default_json, Compression::DictionaryUse::Default_xml};
    ZStdDictionaryRepository repo;
    for (const auto& [dictUse, dictVersion]: samples)
    {
        repo.addDictionary(std::make_unique<ZStdDictionary>(ZStdDictionary::fromBin(sampleDictionary(dictUse, dictVersion))));
    }
    EXPECT_ANY_THROW(repo.getDictionaryForUse(Compression::DictionaryUse::Undefined));
}
