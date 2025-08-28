/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/groups/FhirResourceGroupConst.hxx"

#include <gtest/gtest.h>

class FhirResourceGroupConstTest : public ::testing::Test
{
public:
    void SetUp() override
    {
        using namespace fhirtools::version_literal;
        resolver.findGroup("http://erp-test.de/some/url1", "A"_ver, "somewhereondevice.xml");
        resolver.findGroup("http://erp-test.de/some/url2", "B"_ver, "somewhereondevice.xml");
        resolver.findGroup("http://erp-test.de/some/url3", "C"_ver, "somewhereondevice.xml");
        resolver.findGroup("http://erp-test.de/some/url4", "D"_ver, "somewhereondevice.xml");
        resolver.findGroup("http://erp-test.de/some/url_noVer", fhirtools::FhirVersion::notVersioned,
                           "somewhereondevice.xml");
    }
    fhirtools::FhirResourceGroupConst resolver{"test"};
};

TEST_F(FhirResourceGroupConstTest, allGroups)
{
    auto groups = resolver.allGroups();
    ASSERT_EQ(groups.size(), 1);
    const auto& group = *groups.begin();
    ASSERT_EQ(group.first, "test");
    ASSERT_NE(group.second, nullptr);
    EXPECT_EQ(std::string{group.second->id()}, "test");
}

TEST_F(FhirResourceGroupConstTest, findGroup)
{
    using namespace fhirtools::version_literal;
    auto group = resolver.findGroup("http://erp-test.de/some/url", "some"_ver, "somewhereondevice.xml");
    ASSERT_NE(group, nullptr);
    EXPECT_EQ(std::string{group->id()}, "test");
}

TEST_F(FhirResourceGroupConstTest, findGroupAmbiguousVer)
{
    using namespace fhirtools::version_literal;
    EXPECT_THROW(resolver.findGroup("http://erp-test.de/some/url1", "X"_ver, "somewhereondevice.xml"),
                 std::logic_error);
}


TEST_F(FhirResourceGroupConstTest, findGroupById)
{
    using namespace fhirtools::version_literal;
    fhirtools::FhirResourceGroupConst resolver{"test"};
    auto group = resolver.findGroupById("test");
    ASSERT_NE(group, nullptr);
    EXPECT_EQ(std::string{group->id()}, "test");
    auto noGroup = resolver.findGroupById("someOtherGroupId");
    EXPECT_EQ(noGroup, nullptr);
}

TEST_F(FhirResourceGroupConstTest, Group_findVersionLocal)
{
    using namespace fhirtools::version_literal;
    auto group = resolver.findGroupById("test");
    ASSERT_NE(group, nullptr);
    {
        // no version provided
        auto version = group->findVersionLocal({"http://erp-test.de/some/url1", std::nullopt});
        ASSERT_TRUE(version.has_value());
        EXPECT_EQ(to_string(*version), "A");
    }
    {
        // matching Version requested
        auto version = group->findVersionLocal({"http://erp-test.de/some/url1", "A"_ver});
        ASSERT_TRUE(version.has_value());
        EXPECT_EQ(to_string(*version), "A");
    }
    {
        // requested Version doesn't match'
        auto version = group->findVersionLocal({"http://erp-test.de/some/url1", "C"_ver});
        ASSERT_FALSE(version.has_value());
    }
    {
        // requested resource only exists with version
        auto version = group->findVersionLocal({"http://erp-test.de/some/url1", fhirtools::FhirVersion::notVersioned});
        ASSERT_FALSE(version.has_value());
    }
    {
        // find an unversioned Profile
        auto version = group->findVersionLocal({"http://erp-test.de/some/url_noVer", std::nullopt});
        ASSERT_TRUE(version.has_value());
        EXPECT_FALSE(version->version().has_value());
    }
    {
        // find an unversioned Profile
        auto version = group->findVersionLocal({"http://erp-test.de/some/url_noVer", fhirtools::FhirVersion::notVersioned});
        ASSERT_TRUE(version.has_value());
        EXPECT_FALSE(version->version().has_value());
    }
    {
        // '|B' is interpreted as part of the URL not as Version
        auto version = group->findVersionLocal({"http://erp-test.de/some/url2|B", std::nullopt});
        ASSERT_FALSE(version.has_value());
    }
    {
        // unknown url no version
        auto notFound = group->findVersionLocal({"http://erp-test.de/some/url/unknown", std::nullopt});
        EXPECT_FALSE(notFound.has_value());
    }
    {
        // unknown url no version
        auto notFound = group->findVersionLocal({"http://erp-test.de/some/url/unknown", "A"_ver});
        EXPECT_FALSE(notFound.has_value());
    }
}

TEST_F(FhirResourceGroupConstTest, Group_findVersion)
{
    using namespace fhirtools::version_literal;
    auto group = resolver.findGroupById("test");
    ASSERT_NE(group, nullptr);
    {
        // no version provided
        auto versionAndGroup = group->findVersion({"http://erp-test.de/some/url1", std::nullopt});
        ASSERT_NE(versionAndGroup.second, nullptr);
        EXPECT_EQ(std::string{versionAndGroup.second->id()}, "test");
        ASSERT_TRUE(versionAndGroup.first.has_value());
        EXPECT_EQ(to_string(*versionAndGroup.first), "A");
    }
    {
        // matching Version requested
        auto versionAndGroup = group->findVersion({"http://erp-test.de/some/url1", "A"_ver});
        ASSERT_NE(versionAndGroup.second, nullptr);
        EXPECT_EQ(std::string{versionAndGroup.second->id()}, "test");
        ASSERT_TRUE(versionAndGroup.first.has_value());
        EXPECT_EQ(to_string(*versionAndGroup.first), "A");
    }
    {
        // requested Version doesn't match'
        auto versionAndGroup = group->findVersion({"http://erp-test.de/some/url1", "C"_ver});
        ASSERT_EQ(versionAndGroup.second, nullptr);
        ASSERT_FALSE(versionAndGroup.first.has_value());
    }
    {
        // requested resource only exists with version
        auto notFound = group->findVersion({"http://erp-test.de/some/url1", fhirtools::FhirVersion::notVersioned});
        ASSERT_EQ(notFound.second, nullptr);
        ASSERT_FALSE(notFound.first.has_value());
    }
    {
        // not versioned provided
        auto versionAndGroup = group->findVersion({"http://erp-test.de/some/url_noVer", std::nullopt});
        ASSERT_NE(versionAndGroup.second, nullptr);
        EXPECT_EQ(std::string{versionAndGroup.second->id()}, "test");
        ASSERT_TRUE(versionAndGroup.first.has_value());
        EXPECT_FALSE(versionAndGroup.first->version().has_value());
    }
    {
        // find an unversioned Profile
        auto versionAndGroup = group->findVersion({"http://erp-test.de/some/url_noVer", fhirtools::FhirVersion::notVersioned});
        ASSERT_NE(versionAndGroup.second, nullptr);
        EXPECT_EQ(std::string{versionAndGroup.second->id()}, "test");
        ASSERT_TRUE(versionAndGroup.first.has_value());
        EXPECT_FALSE(versionAndGroup.first->version().has_value());
    }
    {
        // '|B' is interpreted as part of the URL not as Version
        auto notFound = group->findVersion({"http://erp-test.de/some/url2|B", std::nullopt});
        ASSERT_EQ(notFound.second, nullptr);
        ASSERT_FALSE(notFound.first.has_value());
    }
    {
        // unknown url no version
        auto notFound = group->findVersion({"http://erp-test.de/some/url/unknown", std::nullopt});
        EXPECT_EQ(notFound.second, nullptr);
        EXPECT_FALSE(notFound.first.has_value());
    }
    {
        // unknown url no version
        auto notFound = group->findVersion({"http://erp-test.de/some/url/unknown", "A"_ver});
        EXPECT_EQ(notFound.second, nullptr);
        EXPECT_FALSE(notFound.first.has_value());
    }
}

TEST_F(FhirResourceGroupConstTest, Group_find)
{
    using namespace fhirtools::version_literal;
    auto group = resolver.findGroupById("test");
    ASSERT_NE(group, nullptr);
    {
        // no version provided
        auto key = group->find({"http://erp-test.de/some/url1", std::nullopt});
        ASSERT_NE(key.second, nullptr);
        EXPECT_EQ(std::string{key.second->id()}, "test");
        ASSERT_TRUE(key.first.version.has_value());
        EXPECT_EQ(to_string(*key.first.version), "A");
    }
    {
        // matching version provided
        auto key = group->find({"http://erp-test.de/some/url2", "B"_ver});
        ASSERT_NE(key.second, nullptr);
        EXPECT_EQ(std::string{key.second->id()}, "test");
        ASSERT_TRUE(key.first.version.has_value());
        EXPECT_EQ(to_string(*key.first.version), "B");
    }
    {
        // not versioned provided
        auto key = group->find({"http://erp-test.de/some/url_noVer", std::nullopt});
        ASSERT_NE(key.second, nullptr);
        EXPECT_EQ(std::string{key.second->id()}, "test");
        ASSERT_TRUE(key.first.version.has_value());
        EXPECT_FALSE(key.first.version->version().has_value());
    }
    {
        // query notVersioned - unversioned resource exists
        auto key = group->find({"http://erp-test.de/some/url_noVer", fhirtools::FhirVersion::notVersioned});
        ASSERT_NE(key.second, nullptr);
        EXPECT_EQ(std::string{key.second->id()}, "test");
        ASSERT_TRUE(key.first.version.has_value());
        EXPECT_FALSE(key.first.version->version().has_value());
    }
    {
        // query notVersioned - only versioned resource exists
        auto notFound = group->find({"http://erp-test.de/some/url1", fhirtools::FhirVersion::notVersioned});
        EXPECT_EQ(notFound.second, nullptr);
        EXPECT_FALSE(notFound.first.version.has_value());
    }
    {
        // none-matching version provided
        auto notFound = group->find({"http://erp-test.de/some/url3", "B"_ver});
        EXPECT_EQ(notFound.second, nullptr);
        EXPECT_FALSE(notFound.first.version.has_value());
    }
    {
        // unknown url no version
        auto notFound = group->find({"http://erp-test.de/some/url/unknown", std::nullopt});
        EXPECT_EQ(notFound.second, nullptr);
        EXPECT_FALSE(notFound.first.version.has_value());
    }
    {
        // unknown url with version
        auto notFound = group->find({"http://erp-test.de/some/url/unknown", "A"_ver});
        EXPECT_EQ(notFound.second, nullptr);
        EXPECT_FALSE(notFound.first.version.has_value());
    }
}
