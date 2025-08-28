/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/VersionMapper.hxx"
#include "fhirtools/repository/groups/FhirResourceGroupConfiguration.hxx"

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

    auto groupGetter(fhirtools::FhirResourceGroupConfiguration& groupCfg) {
        return [&](std::string url, fhirtools::FhirVersion version, std::filesystem::path path) {
            return fhirtools::FhirStructureDefinition::Builder{}
            .url(std::move(url))
            .version(std::move(version))
            .sourceFile(std::move(path))
            .typeId("dummy")
            .initGroup(groupCfg)
            .repositoryBackend(&repo)
            .getAndReset()
            ->resourceGroup();
        };
    }
    fhirtools::FhirStructureRepositoryBackend repo;
};

TEST_F(FhirResourceGroupConfigurationTest, allGroups)
{
    fhirtools::FhirResourceGroupConfiguration groupCfg{doc().get(), nullptr};
    auto allGroups = groupCfg.allGroups();
    EXPECT_EQ(allGroups.size(), 3);
    EXPECT_TRUE(allGroups.contains("groupA"));
    EXPECT_TRUE(allGroups.contains("groupB"));
    EXPECT_TRUE(allGroups.contains("fileMatchGroup"));
}

TEST_F(FhirResourceGroupConfigurationTest, findGroupById)
{
    fhirtools::FhirResourceGroupConfiguration groupCfg{doc().get(), nullptr};
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
    fhirtools::FhirResourceGroupConfiguration groupCfg{doc().get(), nullptr};
    auto groupFor = groupGetter(groupCfg);

    auto groupA = groupFor("https://erp-test.de/resource", "A"_ver, "some_other.xml");
    ASSERT_NE(groupA, nullptr);
    EXPECT_EQ(std::string{groupA->id()}, "groupA");

    auto groupB = groupFor("https://erp-test.de/resource","B"_ver,"some_other.xml");
    ASSERT_NE(groupB, nullptr);
    EXPECT_EQ(std::string{groupB->id()}, "groupB");

    auto fileMatchGroup = groupFor("https://erp-test.de/resource", "C"_ver, "some_file.xml");
    ASSERT_NE(fileMatchGroup, nullptr);
    EXPECT_EQ(std::string{fileMatchGroup->id()}, "fileMatchGroup");

    // Ambiguous versions (A, C) in group (fileMatchGroup):
    EXPECT_THROW(groupFor("https://erp-test.de/resource", "A"_ver, "some_file.xml"), std::logic_error);

    // no grpup matches
    EXPECT_THROW(groupFor("https://erp-test.de/resource", "D"_ver, "some_other.xml"), std::logic_error);
}

TEST_F(FhirResourceGroupConfigurationTest, findGroup_pattern)
{
    using namespace fhirtools::version_literal;
    fhirtools::FhirResourceGroupConfiguration groupCfg{doc().get(), nullptr};
    auto groupFor = groupGetter(groupCfg);

    auto groupA = groupFor("https://erp-test.de/resource/A_is_also_there", "pattern"_ver, {});
    ASSERT_NE(groupA, nullptr);
    EXPECT_EQ(std::string{groupA->id()}, "groupA");

    auto groupB = groupFor("https://erp-test.de/resource/B_also_likes_to_play", "pattern"_ver, {});
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
                                                           .get(), nullptr};

    auto groupFor = groupGetter(groupCfg);
    auto groupA = groupFor("https://erp-test.de/resource/forA", "A"_ver, {});
    ASSERT_NE(groupA, nullptr);
    auto groupB = groupFor("https://erp-test.de/resource/forB", "B"_ver, {});
    ASSERT_NE(groupB, nullptr);
    {
        auto foundAInA = groupA->findVersion({"https://erp-test.de/resource/forA", std::nullopt});
        ASSERT_NE(foundAInA.second, nullptr);
        EXPECT_EQ(std::string{foundAInA.second->id()}, "groupA");
        ASSERT_TRUE(foundAInA.first.has_value());
        EXPECT_EQ(to_string(*foundAInA.first), "A");
    }

    {
        auto notFoundBInA = groupA->findVersion({"https://erp-test.de/resource/forB", std::nullopt});
        ASSERT_EQ(notFoundBInA.second, nullptr);
        ASSERT_FALSE(notFoundBInA.first.has_value());
    }

    {
        // group A includes B
        auto foundAViaB = groupB->findVersion({"https://erp-test.de/resource/forA", std::nullopt});
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
                                                           .get(), nullptr};
    auto groupFor = groupGetter(groupCfg);
    auto groupA = groupFor("https://erp-test.de/resource/forA", "A"_ver, {});
    ASSERT_NE(groupA, nullptr);
    auto groupB = groupFor("https://erp-test.de/resource/forB", "B"_ver, {});
    ASSERT_NE(groupB, nullptr);

    {
        // group A includes B (to double check config is correct)
        auto foundAViaB = groupB->findVersion({"https://erp-test.de/resource/forA", std::nullopt});
        ASSERT_NE(foundAViaB.second, nullptr);
        EXPECT_EQ(std::string{foundAViaB.second->id()}, "groupA");
        ASSERT_TRUE(foundAViaB.first.has_value());
        EXPECT_EQ(to_string(*foundAViaB.first), "A");
    }
    {
        auto foundAInA = groupA->findVersionLocal({"https://erp-test.de/resource/forA", std::nullopt});
        EXPECT_TRUE(foundAInA.has_value());
        EXPECT_EQ(to_string(*foundAInA), "A");
    }
    {
        auto foundBinB = groupB->findVersionLocal({"https://erp-test.de/resource/forB", std::nullopt});
        EXPECT_TRUE(foundBinB.has_value());
        EXPECT_EQ(to_string(*foundBinB), "B");
    }
    {
        auto notFoundAInB = groupB->findVersionLocal({"https://erp-test.de/resource/forA", std::nullopt});
        EXPECT_FALSE(notFoundAInB.has_value());
    }
    {
        auto notFoundBInA = groupA->findVersionLocal({"https://erp-test.de/resource/forB", std::nullopt});
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
                                                           .get(), nullptr};

    auto groupFor = groupGetter(groupCfg);
    auto groupA = groupFor("https://erp-test.de/resource", "A"_ver, {});
    ASSERT_NE(groupA, nullptr);
    auto groupB = groupFor("https://erp-test.de/resource", "B"_ver, {});
    ASSERT_NE(groupB, nullptr);
    ASSERT_THROW(groupB->findVersion({"https://erp-test.de/resource", std::nullopt}), std::logic_error);
}


TEST_F(FhirResourceGroupConfigurationTest, extend)
{
    using namespace fhirtools::version_literal;
    fhirtools::FhirResourceGroupConfiguration groupCfg{doc(R"([{
        "id": "groupA",
        "match": [
          { "url": "https://erp-test.de/overriddenResource", "version": "A" },
          { "url": "https://erp-test.de/resourceOnlyA", "version": "A" }
        ]
  },
      {
        "id": "groupB",
        "extend": ["groupA"],
        "match": [
          { "url": "https://erp-test.de/overriddenResource", "version": "B" },
          { "url": "https://erp-test.de/resourceOnlyB", "version": "B" }
        ]
      }
  ]
  )")
                                                           .get(), nullptr};
    auto groupFor = groupGetter(groupCfg);
    std::shared_ptr groupA = groupCfg.findGroupById("groupA");
    ASSERT_NE(groupA, nullptr);
    std::shared_ptr groupB = groupCfg.findGroupById("groupB");
    ASSERT_NE(groupB, nullptr);

    // register resources
    ASSERT_EQ(groupFor("https://erp-test.de/overriddenResource", "A"_ver, {}), groupA);
    ASSERT_EQ(groupFor("https://erp-test.de/resourceOnlyA", "A"_ver, {}), groupA);
    ASSERT_EQ(groupFor("https://erp-test.de/overriddenResource", "B"_ver, {}), groupB);
    ASSERT_EQ(groupFor("https://erp-test.de/resourceOnlyB", "B"_ver, {}), groupB);


    {// query without version provides overridden version from groupB
        auto resource = groupB->findVersion({"https://erp-test.de/overriddenResource", std::nullopt});
        EXPECT_EQ(resource.second, groupB);
        EXPECT_EQ(resource.first, "B"_ver);
    }
    {// query with explicit version finds that version even though there is an overridden one
        auto resource = groupB->findVersion({"https://erp-test.de/overriddenResource", "A"_ver});
        EXPECT_EQ(resource.second, groupA);
        EXPECT_EQ(resource.first, "A"_ver);
    }
    {// query with explicit version finds that version no matter if it overrides anything
        auto resource = groupB->findVersion({"https://erp-test.de/overriddenResource", "B"_ver});
        EXPECT_EQ(resource.second, groupB);
        EXPECT_EQ(resource.first, "B"_ver);
    }
    {// query resource with explicit version in B only still works
        auto resource = groupB->findVersion({"https://erp-test.de/resourceOnlyB", "B"_ver});
        EXPECT_EQ(resource.second, groupB);
        EXPECT_EQ(resource.first, "B"_ver);
    }
    {// query resource in B only still works
        auto resource = groupB->findVersion({"https://erp-test.de/resourceOnlyB", "B"_ver});
        EXPECT_EQ(resource.second, groupB);
        EXPECT_EQ(resource.first, "B"_ver);
    }
    {// group A is not affected by group B
        auto resource = groupA->findVersion({"https://erp-test.de/overriddenResource", std::nullopt});
        EXPECT_EQ(resource.second, groupA);
        EXPECT_EQ(resource.first, "A"_ver);
    }
}

TEST_F(FhirResourceGroupConfigurationTest, ambiguousExtend)
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
        "match": [
          { "url": "https://erp-test.de/resource", "version": "B" }
        ]
      },
      {
        "id": "groupAB",
        "extend": ["groupA", "groupB"],
        "match": []
      }
  ]
  )")
                                                           .get(), nullptr};
    auto groupFor = groupGetter(groupCfg);
    std::shared_ptr groupA = groupCfg.findGroupById("groupA");
    ASSERT_NE(groupA, nullptr);
    std::shared_ptr groupB = groupCfg.findGroupById("groupB");
    ASSERT_NE(groupB, nullptr);
    std::shared_ptr groupAB = groupCfg.findGroupById("groupAB");
    ASSERT_NE(groupAB, nullptr);

    // register resources
    ASSERT_EQ(groupFor("https://erp-test.de/resource", "A"_ver, {}), groupA);
    ASSERT_EQ(groupFor("https://erp-test.de/resource", "B"_ver, {}), groupB);

    ASSERT_ANY_THROW(groupAB->findVersion({"https://erp-test.de/resource", std::nullopt}));
}


TEST_F(FhirResourceGroupConfigurationTest, versionMap)
{
    using namespace fhirtools::version_literal;
    const auto groupCfgJson{doc(R"([{
          "id": "groupA",
          "map-versions": true,
          "match": [
            { "url": "https://erp-test.de/resource", "version": "A.1" }
          ]
        },
        {
          "id": "groupB",
          "match": [
            { "url": "https://erp-test.de/resource", "version": "B.1" }
          ]
        }
      ])")};
    fhirtools::VersionMapper::Config versionMapperCfg{
        .map =
            {
                {
                    .urlRegex = "https://erp-test\\.de/.*",
                    .versionRegex = "A",
                    .realVersion = "A.1"_ver,
                    .renderVersion = "0"_ver,
                },
                {
                    .urlRegex = "https://erp-test\\.de/.*",
                    .versionRegex = "B",
                    .realVersion = "B.1"_ver,
                    .renderVersion = "1"_ver,
                },
            },
    };
    auto mapper = std::make_shared<const fhirtools::VersionMapper>(std::move(versionMapperCfg));
    fhirtools::FhirResourceGroupConfiguration groupCfg{groupCfgJson.get(), mapper};
    auto groupFor = groupGetter(groupCfg);
    std::shared_ptr groupA = groupCfg.findGroupById("groupA");
    ASSERT_NE(groupA, nullptr);
    std::shared_ptr groupB = groupCfg.findGroupById("groupB");
    ASSERT_NE(groupB, nullptr);

    // register resources
    ASSERT_EQ(groupFor("https://erp-test.de/resource", "A.1"_ver, {}), groupA);
    ASSERT_EQ(groupFor("https://erp-test.de/resource", "B.1"_ver, {}), groupB);

    { // find unmapped A.1:
        auto A1 = groupA->findVersion({"https://erp-test.de/resource", "A.1"_ver});
        EXPECT_EQ(A1.first, "A.1"_ver);
    }
    { // find unmapped B.1:
        auto B1 = groupB->findVersion({"https://erp-test.de/resource", "B.1"_ver});
        EXPECT_EQ(B1.first, "B.1"_ver);
    }
    { // find A.1 without version:
        auto A1 = groupA->findVersion({"https://erp-test.de/resource", std::nullopt});
        EXPECT_EQ(A1.first, "A.1"_ver);
    }
    { // find B.1 without version:
        auto B1 = groupB->findVersion({"https://erp-test.de/resource", std::nullopt});
        EXPECT_EQ(B1.first, "B.1"_ver);
    }
    { // find mapped A.1:
        auto A1 = groupA->findVersion({"https://erp-test.de/resource", "A"_ver});
        EXPECT_EQ(A1.first, "A.1"_ver);
    }
    { // B.1 is not found, because group B doesn't have "map-versions": true
        auto B1 = groupB->findVersion({"https://erp-test.de/resource", "B"_ver});
        EXPECT_FALSE(B1.first.has_value());
    }
}
