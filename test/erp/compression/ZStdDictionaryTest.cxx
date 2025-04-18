/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/compression/ZStdDictionary.hxx"

#include "test/erp/compression/ZStdTestHelper.hxx"

#include <gtest/gtest.h>
#include <string_view>

#include <optional>

using namespace std::string_view_literals;

TEST(ZStdDictionaryTest, dictionaryIds)//NOLINT(readability-function-cognitive-complexity)
{
    using DictionaryUse = Compression::DictionaryUse;
    for (auto dictUse : {DictionaryUse::Default_json, DictionaryUse::Default_xml})
    {
        for (uint8_t ver = 0; ver < 16; ++ver)
        {
            std::optional<ZStdDictionary> dict;
            ASSERT_NO_THROW(dict.emplace(ZStdDictionary::fromBin(sampleDictionary(dictUse, ver))));
            EXPECT_EQ(dict->id().use(), dictUse);
            EXPECT_EQ(dict->id().version(), ver);
        }
    }
}
