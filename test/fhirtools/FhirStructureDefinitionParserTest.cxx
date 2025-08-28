#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/groups/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/views/FhirResourceViewVerifier.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>

using namespace fhirtools;


TEST(FhirStructureDefinitionParserTest, ignoredDefinitions)
{
    using namespace fhirtools::version_literal;
    FhirResourceGroupConst resolver{"test"};
    FhirStructureRepositoryBackend repo;
    ASSERT_NO_THROW(repo.load(
        {
            ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/minifhirtypes.xml"),
            ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/logical_model.xml"),
            ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/retired_code_system.xml"),
            ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/retired_resource.xml"),
            ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/retired_value_set.xml"),
            ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/test_code_system.xml"),
            ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/test_resource.xml"),
            ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/test_value_set.xml"),
        },
        resolver));

    /// definitions expected to be found
    const auto* testResource = repo.findDefinition("http://fhir-tools.test/minifhirtypes/TestResource", "0.1"_ver);
    EXPECT_NE(testResource, nullptr);

    const auto* testCodeSystem = repo.findCodeSystem("http://erp.test/TestCodeSystem", "0.1"_ver);
    EXPECT_NE(testCodeSystem, nullptr);

    const auto* testValueSet = repo.findValueSet("http://erp.test/TestValueSet", "0.1"_ver);
    EXPECT_NE(testValueSet, nullptr);


    /// Retired definitions expected to be ignored
    const auto* retiredResource = repo.findDefinition("http://fhir-tools.test/minifhirtypes/TestResource", "0.0.9"_ver);
    EXPECT_EQ(retiredResource, nullptr);

    const auto* retiredCodeSystem = repo.findCodeSystem("http://erp.test/TestCodeSystem", "0.0.9"_ver);
    EXPECT_EQ(retiredCodeSystem, nullptr);

    const auto* retiredValueSet = repo.findValueSet("http://erp.test/TestValueSet", "0.0.9"_ver);
    EXPECT_EQ(retiredValueSet, nullptr);

    /// Logical model expected to be ignored
    const auto* logicalModel = repo.findDefinition("http://fhir-tools.test/logical-models-are-ignored", "0.8.15"_ver);
    EXPECT_EQ(logicalModel, nullptr);
}


