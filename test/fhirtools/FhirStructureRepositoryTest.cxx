/*
 * (C) Copyright IBM Deutschland GmbH 2024
 * (C) Copyright IBM Corp. 2024
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "MockFhirResourceGroup.hxx"
#include "fhirtools/repository/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/FhirResourceViewGroupSet.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "test/util/ResourceManager.hxx"

#include <gmock/gmock.h>
#include <gtest/gtest.h>


class FhirStructureRepositoryTest : public ::testing::Test
{
public:
    struct Resolver : public MockResourceGroupResolver {
        std::shared_ptr<const fhirtools::FhirResourceGroup>
        findGroup(const std::string& url, const fhirtools::FhirVersion& version,
                  const std::filesystem::path& sourceFile) const override
        {
            using namespace fhirtools::version_literal;
            if (version == "0.1"_ver || version == "A"_ver || version == "alpha"_ver)
            {
                return groupResA.findGroup(url, version, sourceFile);
            }
            if (version == "0.2"_ver || version == "B"_ver || version == "beta"_ver)
            {
                return groupResB.findGroup(url, version, sourceFile);
            }
            return MockResourceGroupResolver::findGroup(url, version, sourceFile);
        }
        fhirtools::FhirResourceGroupConst groupResA{"groupA"};
        fhirtools::FhirResourceGroupConst groupResB{"groupB"};
    };
    void SetUp() override
    {
        std::map<std::string, std::shared_ptr<const fhirtools::FhirResourceGroup>> allGroups{
            {"groupA", groupA},
            {"groupB", groupB},
        };
        {
            using ::testing::Return;
            EXPECT_CALL(resolver, allGroups()).WillRepeatedly(Return(allGroups));
        }
        backend.load({ResourceManager::getAbsoluteFilename("test/fhir-path/samples/version_samples.xml")}, resolver);
    }
    Resolver resolver;
    fhirtools::FhirStructureRepositoryBackend backend;
    std::shared_ptr<const fhirtools::FhirResourceGroup> groupA = resolver.groupResA.findGroupById("groupA");
    std::shared_ptr<const fhirtools::FhirResourceGroup> groupB = resolver.groupResB.findGroupById("groupB");
};


TEST_F(FhirStructureRepositoryTest, run)
{
    using namespace fhirtools::version_literal;
    const auto* vsA = backend.findValueSet("http://erp-test.de/versiontest/ValueSet", "0.1"_ver);
    ASSERT_NE(vsA, nullptr);
    const auto* vsB = backend.findValueSet("http://erp-test.de/versiontest/ValueSet", "0.2"_ver);
    ASSERT_NE(vsB, nullptr);
    const auto* csA = backend.findCodeSystem("http://erp-test.de/versiontest/CodeSystem", "A"_ver);
    ASSERT_NE(csA, nullptr);
    const auto* csB = backend.findCodeSystem("http://erp-test.de/versiontest/CodeSystem", "B"_ver);
    ASSERT_NE(csB, nullptr);
    const auto* sdA = backend.findDefinition("http://erp-test.de/versiontest/StructureDefinition", "alpha"_ver);
    ASSERT_NE(sdA, nullptr);
    const auto* sdB = backend.findDefinition("http://erp-test.de/versiontest/StructureDefinition", "beta"_ver);
    ASSERT_NE(sdB, nullptr);

    EXPECT_EQ(vsA->resourceGroup(), groupA);
    EXPECT_EQ(vsB->resourceGroup(), groupB);
    EXPECT_EQ(csA->resourceGroup(), groupA);
    EXPECT_EQ(csB->resourceGroup(), groupB);
    EXPECT_EQ(sdA->resourceGroup(), groupA);
    EXPECT_EQ(sdB->resourceGroup(), groupB);

    EXPECT_EQ(vsA->codesToString(), "[http://erp-test.de/versiontest/CodeSystem]ObsoleteTest, "
                                    "[http://erp-test.de/versiontest/CodeSystem]Test1, "
                                    "[http://erp-test.de/versiontest/CodeSystem]Test2");
    EXPECT_EQ(vsB->codesToString(), "[http://erp-test.de/versiontest/CodeSystem]Test1, "
                                    "[http://erp-test.de/versiontest/CodeSystem]Test2, "
                                    "[http://erp-test.de/versiontest/CodeSystem]Test3");
}
