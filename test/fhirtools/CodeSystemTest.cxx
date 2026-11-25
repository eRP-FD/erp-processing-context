#include "fhirtools/FPExpect.hxx"
#include "fhirtools/repository/FhirCodeSystem.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/groups/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/views/FhirResourceViewGroupSet.hxx"
#include "test/fhirtools/MockFhirResourceGroup.hxx"
#include "test/fhirtools/MockFhirResourceView.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>
#include <map>

class CodeSystemSupplementsTest : public testing::Test
{
public:
    static constexpr char supplemented1[] = "http://fhir-tools.test/supplemental_codesystems/Supplemented|1";
    static constexpr char supplemented2[] = "http://fhir-tools.test/supplemental_codesystems/Supplemented|2";

    static constexpr char supplementerA[] = "http://fhir-tools.test/supplemental_codesystems/Supplementer|A";
    static constexpr char supplementerB[] = "http://fhir-tools.test/supplemental_codesystems/Supplementer|B";

    void SetUp() override
    {
        ON_CALL(*resolver, allGroups).WillByDefault(::testing::Invoke([this] {
            return mGroups;
        }));
        ON_CALL(*resolver, findGroup)
            .WillByDefault(::testing::Invoke([this](const std::string& url, const fhirtools::FhirVersion& version,
                                                    const std::filesystem::path& path) {
                if (path.filename() == "minifhirtypes.xml")
                {
                    return baseResolver.findGroup(url, version, path);
                }
                return group(fhirtools::DefinitionKey{url, version});
            }));
        repo.load(
            {
                ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/minifhirtypes.xml"),
                ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/supplemental_codesystem.xml"),
            },
            *resolver);
    }
    void TearDown() override
    {
        resolver.reset();
        mGroups.clear();
    }

    std::shared_ptr<const fhirtools::FhirResourceGroup> group(const std::string_view& key)
    {
        return group(fhirtools::DefinitionKey{key});
    }
    std::shared_ptr<const fhirtools::FhirResourceGroup> group(const fhirtools::DefinitionKey& key)
    {
        auto& grp = mGroups[to_string(key)];
        if (! grp)
        {
            // create a new group for every key and set it up to resolve only that key.
            using ::testing::Eq;
            using ::testing::Invoke;
            using ::testing::Ne;
            using ::testing::Return;
            fhirtools::DefinitionKey unver{key.url, std::nullopt};
            auto mockGrp = std::make_shared<testing::NiceMock<MockResourceGroup>>();
            ON_CALL(*mockGrp, id).WillByDefault(Return(key.url + '|' + to_string(value(key.version))));

            ON_CALL(*mockGrp, findVersionLocal).WillByDefault(Return(std::nullopt));
            ON_CALL(*mockGrp, findVersionLocal(Eq(key))).WillByDefault(Return(value(key.version)));
            ON_CALL(*mockGrp, findVersionLocal(Eq(unver))).WillByDefault(Return(value(key.version)));

            auto foundResult = [ptr = std::weak_ptr{mockGrp}, version = key.version] {
                return std::make_pair(version, ptr.lock());
            };
            ON_CALL(*mockGrp, findVersion).WillByDefault(Return(std::make_pair(std::nullopt, nullptr)));
            ON_CALL(*mockGrp, findVersion(Eq(key))).WillByDefault(Invoke(foundResult));
            ON_CALL(*mockGrp, findVersion(Eq(unver))).WillByDefault(Invoke(foundResult));
            grp = std::move(mockGrp);
        }
        return grp;
    }
    fhirtools::FhirStructureRepositoryBackend repo;
    fhirtools::FhirResourceGroupConst baseResolver{"base-resolver"};
    std::unique_ptr<MockResourceGroupResolver> resolver =
        std::make_unique<testing::NiceMock<MockResourceGroupResolver>>();

    std::map<std::string, std::shared_ptr<const fhirtools::FhirResourceGroup>> mGroups;
};


TEST_F(CodeSystemSupplementsTest, unsupplemented1)
{
    const auto view = fhirtools::FhirResourceViewGroupSet::create("testView", group(supplemented1), &repo);
    ASSERT_NE(view, nullptr);
    auto cs = view->findCodeSystem(fhirtools::DefinitionKey{supplemented1});
    ASSERT_NE(cs, nullptr);
    const auto codes = cs->getCodes(*view);
    EXPECT_TRUE(codes.containsCode("Test1"));
    EXPECT_TRUE(codes.containsCode("Test2"));
    EXPECT_TRUE(codes.containsCode("NestedConcept"));
    EXPECT_FALSE(codes.containsCode("NewConcept"));

    auto test2 = std::ranges::find(codes, "Test2", &fhirtools::FhirCodeSystem::Code::code);
    ASSERT_NE(test2, codes.end());
    EXPECT_TRUE(test2->parent.empty());
    EXPECT_TRUE(test2->properties.empty());

    auto nestedConcept = std::ranges::find(codes, "NestedConcept", &fhirtools::FhirCodeSystem::Code::code);
    ASSERT_NE(nestedConcept, codes.end());
    EXPECT_EQ(nestedConcept->parent, "Test1");
    EXPECT_TRUE(nestedConcept->properties.empty());
}

TEST_F(CodeSystemSupplementsTest, unsupplemented2)
{
    const auto view = fhirtools::FhirResourceViewGroupSet::create("testView", group(supplemented2), &repo);
    ASSERT_NE(view, nullptr);
    auto cs = view->findCodeSystem(fhirtools::DefinitionKey{supplemented2});
    ASSERT_NE(cs, nullptr);
    const auto codes = cs->getCodes(*view);
    EXPECT_TRUE(codes.containsCode("Test1"));
    EXPECT_TRUE(codes.containsCode("Test2"));
    EXPECT_TRUE(codes.containsCode("NestedConcept"));
    EXPECT_FALSE(codes.containsCode("NewConcept"));

    auto test2 = std::ranges::find(codes, "Test2", &fhirtools::FhirCodeSystem::Code::code);
    ASSERT_NE(test2, codes.end());
    EXPECT_TRUE(test2->parent.empty());
    EXPECT_TRUE(test2->properties.empty());

    auto nestedConcept = std::ranges::find(codes, "NestedConcept", &fhirtools::FhirCodeSystem::Code::code);
    ASSERT_NE(nestedConcept, codes.end());
    EXPECT_EQ(nestedConcept->parent, "Test2");
    EXPECT_TRUE(nestedConcept->properties.empty());
}

TEST_F(CodeSystemSupplementsTest, supplemented1A)
{
    const auto view = fhirtools::FhirResourceViewGroupSet::create("testView",
                                                                  {
                                                                      group(supplemented1),
                                                                      group(supplementerA),
                                                                  },
                                                                  &repo);
    ASSERT_NE(view, nullptr);
    auto cs = view->findCodeSystem(fhirtools::DefinitionKey{supplemented1});
    ASSERT_NE(cs, nullptr);
    const auto codes = cs->getCodes(*view);
    EXPECT_TRUE(codes.containsCode("Test1"));
    EXPECT_TRUE(codes.containsCode("Test2"));
    EXPECT_TRUE(codes.containsCode("NestedConcept"));
    // added by supplementerA:
    EXPECT_TRUE(codes.containsCode("NewConcept"));

    auto test2 = std::ranges::find(codes, "Test2", &fhirtools::FhirCodeSystem::Code::code);
    ASSERT_NE(test2, codes.end());
    EXPECT_TRUE(test2->parent.empty());
    EXPECT_TRUE(test2->properties.empty());

    auto nestedConcept = std::ranges::find(codes, "NestedConcept", &fhirtools::FhirCodeSystem::Code::code);
    ASSERT_NE(nestedConcept, codes.end());
    EXPECT_EQ(nestedConcept->parent, "Test1");
    ASSERT_EQ(nestedConcept->properties.size(), 1);
    EXPECT_EQ(nestedConcept->properties[0].code, "deprecated");
    EXPECT_EQ(nestedConcept->properties[0].value, "false");
}

TEST_F(CodeSystemSupplementsTest, supplemented2A)
{
    const auto view = fhirtools::FhirResourceViewGroupSet::create("testView",
                                                                  {
                                                                      group(supplemented2),
                                                                      group(supplementerA),
                                                                  },
                                                                  &repo);
    ASSERT_NE(view, nullptr);
    auto cs = view->findCodeSystem(fhirtools::DefinitionKey{supplemented2});
    ASSERT_NE(cs, nullptr);
    const auto codes = cs->getCodes(*view);
    EXPECT_TRUE(codes.containsCode("Test1"));
    EXPECT_TRUE(codes.containsCode("Test2"));
    EXPECT_TRUE(codes.containsCode("NestedConcept"));
    // added by supplementerA:
    EXPECT_TRUE(codes.containsCode("NewConcept"));

    auto test2 = std::ranges::find(codes, "Test2", &fhirtools::FhirCodeSystem::Code::code);
    ASSERT_NE(test2, codes.end());
    EXPECT_TRUE(test2->parent.empty());
    EXPECT_TRUE(test2->properties.empty());

    auto nestedConcept = std::ranges::find(codes, "NestedConcept", &fhirtools::FhirCodeSystem::Code::code);
    ASSERT_NE(nestedConcept, codes.end());
    EXPECT_EQ(nestedConcept->parent, "Test2");
    ASSERT_EQ(nestedConcept->properties.size(), 1);
    EXPECT_EQ(nestedConcept->properties[0].code, "deprecated");
    EXPECT_EQ(nestedConcept->properties[0].value, "false");
}

TEST_F(CodeSystemSupplementsTest, supplemented1B)
{
    const auto view = fhirtools::FhirResourceViewGroupSet::create("testView",
                                                                  {
                                                                      group(supplemented1),
                                                                      group(supplementerB),
                                                                  },
                                                                  &repo);
    ASSERT_NE(view, nullptr);
    auto cs = view->findCodeSystem(fhirtools::DefinitionKey{supplemented1});
    ASSERT_NE(cs, nullptr);
    const auto codes = cs->getCodes(*view);
    EXPECT_TRUE(codes.containsCode("Test1"));
    EXPECT_TRUE(codes.containsCode("Test2"));
    EXPECT_TRUE(codes.containsCode("NestedConcept"));
    EXPECT_FALSE(codes.containsCode("NewConcept"));

    auto test2 = std::ranges::find(codes, "Test2", &fhirtools::FhirCodeSystem::Code::code);
    ASSERT_NE(test2, codes.end());
    EXPECT_TRUE(test2->parent.empty());
    ASSERT_EQ(test2->properties.size(), 1);
    EXPECT_EQ(test2->properties[0].code, "deprecated");
    EXPECT_EQ(test2->properties[0].value, "true");

    auto nestedConcept = std::ranges::find(codes, "NestedConcept", &fhirtools::FhirCodeSystem::Code::code);
    ASSERT_NE(nestedConcept, codes.end());
    EXPECT_EQ(nestedConcept->parent, "Test1");
    EXPECT_TRUE(nestedConcept->properties.empty());
}

TEST_F(CodeSystemSupplementsTest, supplemented2B)
{
    const auto view = fhirtools::FhirResourceViewGroupSet::create("testView",
                                                                  {
                                                                      group(supplemented2),
                                                                      group(supplementerB),
                                                                  },
                                                                  &repo);
    ASSERT_NE(view, nullptr);
    auto cs = view->findCodeSystem(fhirtools::DefinitionKey{supplemented2});
    ASSERT_NE(cs, nullptr);
    const auto codes = cs->getCodes(*view);
    EXPECT_TRUE(codes.containsCode("Test1"));
    EXPECT_TRUE(codes.containsCode("Test2"));
    EXPECT_TRUE(codes.containsCode("NestedConcept"));
    EXPECT_FALSE(codes.containsCode("NewConcept"));

    auto test2 = std::ranges::find(codes, "Test2", &fhirtools::FhirCodeSystem::Code::code);
    ASSERT_NE(test2, codes.end());
    EXPECT_TRUE(test2->parent.empty());
    ASSERT_EQ(test2->properties.size(), 1);
    EXPECT_EQ(test2->properties[0].code, "deprecated");
    EXPECT_EQ(test2->properties[0].value, "true");

    auto nestedConcept = std::ranges::find(codes, "NestedConcept", &fhirtools::FhirCodeSystem::Code::code);
    ASSERT_NE(nestedConcept, codes.end());
    EXPECT_EQ(nestedConcept->parent, "Test2");
    EXPECT_TRUE(nestedConcept->properties.empty());
}


TEST_F(CodeSystemSupplementsTest, supplemented1AB)
{
    const auto view = fhirtools::FhirResourceViewGroupSet::create("testView",
                                                                  {
                                                                      group(supplemented1),
                                                                      group(supplementerA),
                                                                      group(supplementerB),
                                                                  },
                                                                  &repo);
    ASSERT_NE(view, nullptr);
    auto cs = view->findCodeSystem(fhirtools::DefinitionKey{supplemented1});
    ASSERT_NE(cs, nullptr);
    const auto codes = cs->getCodes(*view);
    EXPECT_TRUE(codes.containsCode("Test1"));
    EXPECT_TRUE(codes.containsCode("Test2"));
    EXPECT_TRUE(codes.containsCode("NestedConcept"));
    EXPECT_TRUE(codes.containsCode("NewConcept"));

    auto test2 = std::ranges::find(codes, "Test2", &fhirtools::FhirCodeSystem::Code::code);
    ASSERT_NE(test2, codes.end());
    EXPECT_TRUE(test2->parent.empty());
    ASSERT_EQ(test2->properties.size(), 1);
    EXPECT_EQ(test2->properties[0].code, "deprecated");
    EXPECT_EQ(test2->properties[0].value, "true");

    auto nestedConcept = std::ranges::find(codes, "NestedConcept", &fhirtools::FhirCodeSystem::Code::code);
    ASSERT_NE(nestedConcept, codes.end());
    EXPECT_EQ(nestedConcept->parent, "Test1");
    ASSERT_EQ(nestedConcept->properties.size(), 1);
    EXPECT_EQ(nestedConcept->properties[0].code, "deprecated");
    EXPECT_EQ(nestedConcept->properties[0].value, "false");
}

TEST_F(CodeSystemSupplementsTest, supplemented2AB)
{
    const auto view = fhirtools::FhirResourceViewGroupSet::create("testView",
                                                                  {
                                                                      group(supplemented2),
                                                                      group(supplementerA),
                                                                      group(supplementerB),
                                                                  },
                                                                  &repo);
    ASSERT_NE(view, nullptr);
    auto cs = view->findCodeSystem(fhirtools::DefinitionKey{supplemented2});
    ASSERT_NE(cs, nullptr);
    const auto codes = cs->getCodes(*view);
    EXPECT_TRUE(codes.containsCode("Test1"));
    EXPECT_TRUE(codes.containsCode("Test2"));
    EXPECT_TRUE(codes.containsCode("NestedConcept"));
    EXPECT_TRUE(codes.containsCode("NewConcept"));

    auto test2 = std::ranges::find(codes, "Test2", &fhirtools::FhirCodeSystem::Code::code);
    ASSERT_NE(test2, codes.end());
    EXPECT_TRUE(test2->parent.empty());
    ASSERT_EQ(test2->properties.size(), 1);
    EXPECT_EQ(test2->properties[0].code, "deprecated");
    EXPECT_EQ(test2->properties[0].value, "true");

    auto nestedConcept = std::ranges::find(codes, "NestedConcept", &fhirtools::FhirCodeSystem::Code::code);
    ASSERT_NE(nestedConcept, codes.end());
    EXPECT_EQ(nestedConcept->parent, "Test2");
    ASSERT_EQ(nestedConcept->properties.size(), 1);
    EXPECT_EQ(nestedConcept->properties[0].code, "deprecated");
    EXPECT_EQ(nestedConcept->properties[0].value, "false");
}
