/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "erp/util/Configuration.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>

using namespace fhirtools;

class FhirStructureRepositoryTest : public ::testing::Test
{
public:
    void checkTaskFields(const FhirStructureDefinition* task)//NOLINT(readability-function-cognitive-complexity)
    {
        ASSERT_TRUE(task);
        EXPECT_TRUE(task->findElement("Task.id"));
        EXPECT_TRUE(task->findElement("Task.status"));
        EXPECT_TRUE(task->findElement("Task.input"));
        EXPECT_TRUE(task->findElement("Task.output"));
        EXPECT_TRUE(task->findElement("Task.identifier"));
        EXPECT_TRUE(task->findElement("Task.extension"));
        EXPECT_TRUE(task->findElement("Task.performerType"));
    }
    static constexpr std::string_view erxTaskUrl{"https://gematik.de/fhir/StructureDefinition/ErxTask"};
    static constexpr std::string_view gemErpPrTaskUrl{"https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Task"};
};


TEST_F(FhirStructureRepositoryTest, loadResources)//NOLINT(readability-function-cognitive-complexity)
{
    auto mkres = [&](const std::filesystem::path& p) {
        return ResourceManager::getAbsoluteFilename(p);
    };

    auto fileList = {
        mkres("fhir/hl7.org/profiles-resources.xml"),
        mkres("fhir/hl7.org/profiles-types.xml"),
        mkres("fhir/hl7.org/valuesets.xml")
    };

    FhirStructureRepository repo;

    EXPECT_NO_THROW(repo.load(fileList));
    const auto* const hl7task = repo.findTypeById("Task");
    ASSERT_NO_FATAL_FAILURE(checkTaskFields(hl7task));

    EXPECT_TRUE(repo.findTypeById("Resource"));
    EXPECT_TRUE(repo.findTypeById("Patient"));
    EXPECT_TRUE(repo.findTypeById("Communication"));
    EXPECT_TRUE(repo.findTypeById("Medication"));
    EXPECT_TRUE(repo.findTypeById("AuditEvent"));
    EXPECT_TRUE(repo.findTypeById("MedicationDispense"));
    EXPECT_TRUE(repo.findTypeById("Bundle"));
    EXPECT_TRUE(repo.findTypeById("Parameters"));
    EXPECT_TRUE(repo.findTypeById("Organization"));
    EXPECT_TRUE(repo.findTypeById("Device"));
    EXPECT_TRUE(repo.findTypeById("Meta"));

    EXPECT_FALSE(repo.findDefinitionByUrl(std::string{erxTaskUrl}));
}

TEST_F(FhirStructureRepositoryTest, missingBase)
{
    auto mkres = [&](const std::filesystem::path& p) {
        return ResourceManager::getAbsoluteFilename(p);
    };

    auto fileList = {
        mkres("fhir/hl7.org/profiles-resources.xml"),
    };

    FhirStructureRepository repo;

    EXPECT_ANY_THROW(repo.load(fileList));
}

TEST_F(FhirStructureRepositoryTest, loadFromConfigurationOldProfiles)
{
    auto strings = Configuration::instance().getArray(ConfigurationKey::FHIR_STRUCTURE_DEFINITIONS_OLD);
    std::list<std::filesystem::path> fileList;
    for (const auto& item : strings)
    {
        fileList.emplace_back(item);
    }

    FhirStructureRepository repo;
    EXPECT_NO_THROW(repo.load(fileList));
    const auto* const erxTask = repo.findDefinitionByUrl(std::string{erxTaskUrl});
    ASSERT_NO_FATAL_FAILURE(checkTaskFields(erxTask));
}

TEST_F(FhirStructureRepositoryTest, loadFromConfigurationNewProfiles)
{
    auto strings = Configuration::instance().getArray(ConfigurationKey::FHIR_STRUCTURE_DEFINITIONS);
    std::list<std::filesystem::path> fileList;
    for (const auto& item : strings)
    {
        fileList.emplace_back(item);
    }

    FhirStructureRepository repo;
    EXPECT_NO_THROW(repo.load(fileList));
    const auto* const erxTask = repo.findDefinitionByUrl(std::string{gemErpPrTaskUrl});
    ASSERT_NO_FATAL_FAILURE(checkTaskFields(erxTask));
}
