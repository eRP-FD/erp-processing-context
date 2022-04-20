/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include <gtest/gtest.h>

#include "erp/fhir/FhirStructureRepository.hxx"
#include "test/util/ResourceManager.hxx"

class FhirStructureRepositoryTest : public ::testing::Test
{
public:
    void checkTaskFields(const FhirStructureDefinition* task)
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
};


TEST_F(FhirStructureRepositoryTest, loadResources)
{
    static constexpr std::string_view erxTaskUrl{"https://gematik.de/fhir/StructureDefinition/ErxTask"};
    const auto& resourceManager = ResourceManager::instance();
    auto mkres= [&](const std::filesystem::path& p) { return resourceManager.getAbsoluteFilename(p); };

    auto fileList = {
        mkres("fhir/hl7.org/profiles-resources.xml"),
        mkres("fhir/hl7.org/profiles-types.xml")
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
    repo.load({mkres("test/fhir/GemerxTask.xml")});
    const auto* const erxTask = repo.findDefinitionByUrl(std::string{erxTaskUrl});
    ASSERT_NO_FATAL_FAILURE(checkTaskFields(erxTask));

    EXPECT_FALSE(repo.findTypeById("NonsenseResource"));
}

TEST_F(FhirStructureRepositoryTest, missingBase)
{
    const auto& resourceManager = ResourceManager::instance();
    auto mkres= [&](const std::filesystem::path& p) { return resourceManager.getAbsoluteFilename(p); };

    auto fileList = {
        mkres("fhir/hl7.org/profiles-resources.xml"),
    };

    FhirStructureRepository repo;

    EXPECT_ANY_THROW(repo.load(fileList));
}
