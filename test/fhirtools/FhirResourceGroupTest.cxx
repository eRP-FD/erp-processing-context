#include "MockFhirResourceGroup.hxx"
#include "fhirtools/repository/DefinitionKey.hxx"

#include <gtest/gtest.h>


TEST(FhirResourceGroupTest, find)
{
    using namespace fhirtools::version_literal;
    using ::testing::Eq;
    using ::testing::Return;
    auto group = std::make_shared<MockResourceGroup>();
    EXPECT_CALL(*group, id).WillRepeatedly(Return("test"));

    ASSERT_NE(group, nullptr);
    {
        // no version provided
        EXPECT_CALL(*group, findVersion(Eq("http://erp-test.de/some/url1")))
            .WillRepeatedly(::testing::Return(std::make_pair("A"_ver, group)));
        auto key = group->find({"http://erp-test.de/some/url1", std::nullopt});
        ASSERT_EQ(key.second, group);
        EXPECT_EQ(key.first.url, "http://erp-test.de/some/url1");
        ASSERT_TRUE(key.first.version.has_value());
        EXPECT_EQ(to_string(*key.first.version), "A");
    }
    {
        // matching version provided
        EXPECT_CALL(*group, findVersion(Eq("http://erp-test.de/some/url2")))
            .WillRepeatedly(::testing::Return(std::make_pair("B"_ver, group)));
        auto key = group->find({"http://erp-test.de/some/url2", "B"_ver});
        ASSERT_EQ(key.second, group);
        EXPECT_EQ(key.first.url, "http://erp-test.de/some/url2");
        ASSERT_TRUE(key.first.version.has_value());
        EXPECT_EQ(to_string(*key.first.version), "B");
    }
    {
        // not versioned - no version Provided
        EXPECT_CALL(*group, findVersion(Eq("http://erp-test.de/some/url_noVer")))
            .WillRepeatedly(::testing::Return(std::make_pair(fhirtools::FhirVersion::notVersioned, group)));
        auto key = group->find({"http://erp-test.de/some/url_noVer", std::nullopt});
        ASSERT_EQ(key.second, group);
        EXPECT_EQ(key.first.url, "http://erp-test.de/some/url_noVer");
        ASSERT_TRUE(key.first.version.has_value());
        EXPECT_FALSE(key.first.version->version().has_value());
    }
    {
        // query notVersioned - unversioned resource exists
        EXPECT_CALL(*group, findVersion(Eq("http://erp-test.de/some/url_noVer")))
            .WillRepeatedly(::testing::Return(std::make_pair(fhirtools::FhirVersion::notVersioned, group)));
        auto key = group->find({"http://erp-test.de/some/url_noVer", fhirtools::FhirVersion::notVersioned});
        ASSERT_EQ(key.second, group);
        EXPECT_EQ(key.first.url, "http://erp-test.de/some/url_noVer");
        ASSERT_TRUE(key.first.version.has_value());
        EXPECT_FALSE(key.first.version->version().has_value());
    }
    {
        // query notVersioned - only versioned resource exists
        EXPECT_CALL(*group, findVersion(Eq("http://erp-test.de/some/url1")))
            .WillRepeatedly(::testing::Return(std::make_pair("A"_ver, group)));
        auto notFound = group->find({"http://erp-test.de/some/url1", fhirtools::FhirVersion::notVersioned});
        EXPECT_EQ(notFound.second, nullptr);
        EXPECT_FALSE(notFound.first.version.has_value());
    }
    {
        // none-matching version provided
        EXPECT_CALL(*group, findVersion(Eq("http://erp-test.de/some/url3")))
            .WillRepeatedly(::testing::Return(std::make_pair("C"_ver, group)));
        auto notFound = group->find({"http://erp-test.de/some/url3", "B"_ver});
        EXPECT_EQ(notFound.second, nullptr);
        EXPECT_FALSE(notFound.first.version.has_value());
    }
    {
        // unknown url no version
        EXPECT_CALL(*group, findVersion(Eq("http://erp-test.de/some/url/unknown")))
            .WillRepeatedly(::testing::Return(std::make_pair(std::nullopt, nullptr)));
        auto notFound = group->find({"http://erp-test.de/some/url/unknown", std::nullopt});
        EXPECT_EQ(notFound.second, nullptr);
        EXPECT_FALSE(notFound.first.version.has_value());
    }
    {
        // unknown url with version
        EXPECT_CALL(*group, findVersion(Eq("http://erp-test.de/some/url/unknown")))
            .WillRepeatedly(::testing::Return(std::make_pair(std::nullopt, nullptr)));
        auto notFound = group->find({"http://erp-test.de/some/url/unknown", "A"_ver});
        EXPECT_EQ(notFound.second, nullptr);
        EXPECT_FALSE(notFound.first.version.has_value());
    }
    {
        // version falsely included in url part of DefinitionKey
        EXPECT_CALL(*group, findVersion(Eq("http://erp-test.de/some/url1|A")))
            .WillRepeatedly(::testing::Return(std::make_pair(std::nullopt, nullptr)));
        auto notFound = group->find({"http://erp-test.de/some/url1|A", std::nullopt});
        EXPECT_EQ(notFound.second, nullptr);
        EXPECT_FALSE(notFound.first.version.has_value());
    }
    ::testing::Mock::VerifyAndClear(group.get());
}
