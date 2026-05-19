/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/groups/FhirResourceGroupConfiguration.hxx"
#include "fhirtools/repository/groups/FhirResourceGroupConst.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/Resource.hxx"
#include "shared/util/Configuration.hxx"
#include "test/util/ResourceManager.hxx"

#include <ranges>
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

TEST_F(FhirStructureRepositoryTest, groupReferencing)
{
    // this test checks if all configured groups are actually referenced
    // as all groups are configured in a single file `001_fhir-resource-groups.config.json`
    // `01_production.config.json` and `01_production-medication-exporter.config.json`
    // are considered
    const auto& config = Configuration::instance();
    std::set<std::string> openGroups;
    const auto erpViews = config.fhirResourceViewConfiguration<Configuration::ERP>().allViews();
    const auto exporterViews = config.fhirResourceViewConfiguration<Configuration::MedicationExporter>().allViews();
    const auto allViews = {erpViews, exporterViews};
    auto intoOpenGrous = std::inserter(openGroups, openGroups.begin());
    for (const auto& view : std::views::join(allViews))
    {
        std::ranges::move(view->mGroups, intoOpenGrous);
    }
    std::map unreferencedGroups = config.fhirResourceGroupConfiguration().allGroups();
    while (!openGroups.empty())
    {
        auto groupIt = openGroups.begin();
        auto group = *groupIt;
        openGroups.erase(groupIt);
        auto g = unreferencedGroups.find(group);
        auto ref = g->second->referencedGroups();
        unreferencedGroups.erase(g);
        std::ranges::copy_if(std::move(ref), intoOpenGrous, [&](const std::string& gid){
            return unreferencedGroups.contains(gid);
        });
    }
    if (!unreferencedGroups.empty())
    {
        std::string groups;
        std::string_view sep;
        for (const auto& g: std::views::keys(unreferencedGroups))
        {
            groups.append(sep).append(g);
            sep = ", ";

        }
        TLOG(WARNING) << "unreferenced groups: " << groups;
        FAIL();
    }







}
