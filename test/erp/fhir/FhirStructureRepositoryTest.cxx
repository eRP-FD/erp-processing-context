/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/fhir/Fhir.hxx"
#include "erp/util/Configuration.hxx"
#include "fhirtools/repository/FhirResourceGroupConfiguration.hxx"
#include "fhirtools/repository/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
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
    static constexpr std::string_view gemErpPrTaskUrl{
        "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Task"};
    static inline FhirVersion gemErpPrTaskVersion{Version{"1.3"}};
};


TEST_F(FhirStructureRepositoryTest, loadResources)//NOLINT(readability-function-cognitive-complexity)
{
    auto mkres = [&](const std::filesystem::path& p) {
        return ResourceManager::getAbsoluteFilename(p);
    };

    auto fileList = {mkres("fhir/hl7.org/profiles-resources.xml"), mkres("fhir/hl7.org/profiles-types.xml"),
                     mkres("fhir/hl7.org/valuesets.xml")};

    FhirStructureRepositoryBackend repo;

    EXPECT_NO_THROW(repo.load(fileList, fhirtools::FhirResourceGroupConst{"test"}));
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
}

TEST_F(FhirStructureRepositoryTest, missingBase)
{
    auto mkres = [&](const std::filesystem::path& p) {
        return ResourceManager::getAbsoluteFilename(p);
    };

    auto fileList = {
        mkres("fhir/hl7.org/profiles-resources.xml"),
    };

    FhirStructureRepositoryBackend repo;

    EXPECT_ANY_THROW(repo.load(fileList, FhirResourceGroupConst{"test"}));
}

TEST_F(FhirStructureRepositoryTest, loadConfig)
{
    ASSERT_NO_THROW(Fhir::instance());
}
