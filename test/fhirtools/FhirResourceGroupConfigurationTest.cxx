/*
 * (C) Copyright IBM Deutschland GmbH 2024
 * (C) Copyright IBM Corp. 2024
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "fhirtools/repository/FhirResourceGroupConfiguration.hxx"

#include <gtest/gtest.h>
#include <rapidjson/document.h>


class FhirResourceGroupConfigurationTest : public ::testing::Test
{
public:
    static constexpr std::string_view configJson = R"([{
        "id": "groupA",
        "match": [
          { "url": "https://erp-test.de/resource", "version": "A" },
          { "url": "https://erp-test.de/resource/A.*", "version": "pattern" }
        ]
      },
      {
        "id": "groupB",
        "match": [
          { "url": "https://erp-test.de/resource", "version": "B" },
          { "url": "https://erp-test.de/resource/B.*", "version": "pattern" }
        ]
      },
      {
        "id": "fileMatchGroup",
        "match": [
          { "file": "some_file\\.xml"}
        ]
      }
    ]
    )";
    std::shared_ptr<rapidjson::Document> doc(std::string_view str = configJson)
    {
        auto result = std::make_shared<rapidjson::Document>();
        [&]() {
            ASSERT_FALSE(result->Parse(str.data(), str.size()).HasParseError());
        }();
        return result;
    }
};

TEST_F(FhirResourceGroupConfigurationTest, allGroups)
{
    fhirtools::FhirResourceGroupConfiguration groupCfg{doc().get()};
    auto allGroups = groupCfg.allGroups();
    EXPECT_EQ(allGroups.size(), 3);
    EXPECT_TRUE(allGroups.contains("groupA"));
    EXPECT_TRUE(allGroups.contains("groupB"));
    EXPECT_TRUE(allGroups.contains("fileMatchGroup"));
}

TEST_F(FhirResourceGroupConfigurationTest, findGroupById)
{
    fhirtools::FhirResourceGroupConfiguration groupCfg{doc().get()};
    const auto testGoup = [&](const std::string& groupName) {
        const std::shared_ptr group = groupCfg.findGroupById(groupName);
        ASSERT_NE(group, nullptr);
        EXPECT_EQ(std::string{group->id()}, groupName);
    };
    testGoup("groupA");
    testGoup("groupB");
    testGoup("fileMatchGroup");
    const std::shared_ptr noSuchGroup = groupCfg.findGroupById("noSuchGroup");
    ASSERT_EQ(noSuchGroup, nullptr);
}

TEST_F(FhirResourceGroupConfigurationTest, findGroup)
{
    using namespace fhirtools::version_literal;
    fhirtools::FhirResourceGroupConfiguration groupCfg{doc().get()};

    auto groupA = groupCfg.findGroup("https://erp-test.de/resource", "A"_ver, "some_other.xml");
    ASSERT_NE(groupA, nullptr);
    EXPECT_EQ(std::string{groupA->id()}, "groupA");

    auto groupB = groupCfg.findGroup("https://erp-test.de/resource", "B"_ver, "some_other.xml");
    ASSERT_NE(groupB, nullptr);
    EXPECT_EQ(std::string{groupB->id()}, "groupB");

    auto fileMatchGroup = groupCfg.findGroup("https://erp-test.de/resource", "C"_ver, "some_file.xml");
    ASSERT_NE(fileMatchGroup, nullptr);
    EXPECT_EQ(std::string{fileMatchGroup->id()}, "fileMatchGroup");

    // Ambiguous versions (A, C) in group (fileMatchGroup):
    EXPECT_THROW(groupCfg.findGroup("https://erp-test.de/resource", "A"_ver, "some_file.xml"), std::logic_error);

    auto noGroupMatches = groupCfg.findGroup("https://erp-test.de/resource", "D"_ver, "some_other.xml");
    EXPECT_EQ(noGroupMatches, nullptr) << noGroupMatches->id();
}

TEST_F(FhirResourceGroupConfigurationTest, findGroup_pattern)
{
    using namespace fhirtools::version_literal;
    fhirtools::FhirResourceGroupConfiguration groupCfg{doc().get()};

    auto groupA = groupCfg.findGroup("https://erp-test.de/resource/A_is_also_there", "pattern"_ver, {});
    ASSERT_NE(groupA, nullptr);
    EXPECT_EQ(std::string{groupA->id()}, "groupA");

    auto groupB = groupCfg.findGroup("https://erp-test.de/resource/B_also_likes_to_play", "pattern"_ver, {});
    ASSERT_NE(groupB, nullptr);
    EXPECT_EQ(std::string{groupB->id()}, "groupB");
}

TEST_F(FhirResourceGroupConfigurationTest, Group_findVersion)
{
    using namespace fhirtools::version_literal;
    fhirtools::FhirResourceGroupConfiguration groupCfg{doc(R"([{
        "id": "groupA",
        "match": [
          { "url": "https://erp-test.de/resource/forA", "version": "A" }
        ]
    },
      {
        "id": "groupB",
        "include": ["groupA"],
        "match": [
          { "url": "https://erp-test.de/resource/forB", "version": "B" }
        ]
      }
    ]
    )")
                                                           .get()};

    auto groupA = groupCfg.findGroup("https://erp-test.de/resource/forA", "A"_ver, {});
    ASSERT_NE(groupA, nullptr);
    auto groupB = groupCfg.findGroup("https://erp-test.de/resource/forB", "B"_ver, {});
    ASSERT_NE(groupB, nullptr);
    {
        auto foundAInA = groupA->findVersion("https://erp-test.de/resource/forA");
        ASSERT_NE(foundAInA.second, nullptr);
        EXPECT_EQ(std::string{foundAInA.second->id()}, "groupA");
        ASSERT_TRUE(foundAInA.first.has_value());
        EXPECT_EQ(to_string(*foundAInA.first), "A");
    }

    {
        auto notFoundBInA = groupA->findVersion("https://erp-test.de/resource/forB");
        ASSERT_EQ(notFoundBInA.second, nullptr);
        ASSERT_FALSE(notFoundBInA.first.has_value());
    }

    {
        // group A includes B
        auto foundAViaB = groupB->findVersion("https://erp-test.de/resource/forA");
        ASSERT_NE(foundAViaB.second, nullptr);
        EXPECT_EQ(std::string{foundAViaB.second->id()}, "groupA");
        ASSERT_TRUE(foundAViaB.first.has_value());
        EXPECT_EQ(to_string(*foundAViaB.first), "A");
    }
}

TEST_F(FhirResourceGroupConfigurationTest, Group_findVersionLocal)
{
    using namespace fhirtools::version_literal;
    fhirtools::FhirResourceGroupConfiguration groupCfg{doc(R"([{
        "id": "groupA",
        "match": [
          { "url": "https://erp-test.de/resource/forA", "version": "A" }
        ]
    },
      {
        "id": "groupB",
        "include": ["groupA"],
        "match": [
          { "url": "https://erp-test.de/resource/forB", "version": "B" }
        ]
      }
    ]
    )")
                                                           .get()};

    auto groupA = groupCfg.findGroup("https://erp-test.de/resource/forA", "A"_ver, {});
    ASSERT_NE(groupA, nullptr);
    auto groupB = groupCfg.findGroup("https://erp-test.de/resource/forB", "B"_ver, {});
    ASSERT_NE(groupB, nullptr);

    {
        // group A includes B (to double check config is correct)
        auto foundAViaB = groupB->findVersion("https://erp-test.de/resource/forA");
        ASSERT_NE(foundAViaB.second, nullptr);
        EXPECT_EQ(std::string{foundAViaB.second->id()}, "groupA");
        ASSERT_TRUE(foundAViaB.first.has_value());
        EXPECT_EQ(to_string(*foundAViaB.first), "A");
    }
    {
        auto foundAInA = groupA->findVersionLocal("https://erp-test.de/resource/forA");
        EXPECT_TRUE(foundAInA.has_value());
        EXPECT_EQ(to_string(*foundAInA), "A");
    }
    {
        auto foundBinB = groupB->findVersionLocal("https://erp-test.de/resource/forB");
        EXPECT_TRUE(foundBinB.has_value());
        EXPECT_EQ(to_string(*foundBinB), "B");
    }
    {
        auto notFoundAInB = groupB->findVersionLocal("https://erp-test.de/resource/forA");
        EXPECT_FALSE(notFoundAInB.has_value());
    }
    {
        auto notFoundBInA = groupA->findVersionLocal("https://erp-test.de/resource/forB");
        EXPECT_FALSE(notFoundBInA.has_value());
    }
}


TEST_F(FhirResourceGroupConfigurationTest, AmbiguousVersion)
{
    using namespace fhirtools::version_literal;
    fhirtools::FhirResourceGroupConfiguration groupCfg{doc(R"([{
        "id": "groupA",
        "match": [
          { "url": "https://erp-test.de/resource", "version": "A" }
        ]
      },
      {
        "id": "groupB",
        "include": ["groupA"],
        "match": [
          { "url": "https://erp-test.de/resource", "version": "B" }
        ]
      }
    ]
    )")
                                                           .get()};

    auto groupA = groupCfg.findGroup("https://erp-test.de/resource", "A"_ver, {});
    ASSERT_NE(groupA, nullptr);
    auto groupB = groupCfg.findGroup("https://erp-test.de/resource", "B"_ver, {});
    ASSERT_NE(groupB, nullptr);
    ASSERT_THROW(groupB->findVersion("https://erp-test.de/resource"), std::logic_error);
}
