/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/dosagetext/Generator.hxx"
#include "erp/service/task/ActivateTaskHandler.hxx"
#include "shared/model/DosageDgMP.hxx"
#include "shared/model/KbvBundle.hxx"
#include "shared/model/KbvMedicationRequest.hxx"
#include "shared/util/dosagetext/Language.hxx"
#include "shared/util/dosagetext/RenderedDosageInstruction.hxx"
#include "shared/util/dosagetext/ScriptVersion.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <boost/algorithm/string/replace.hpp>
#include <boost/beast/core/file_stdio.hpp>
#include <gtest/gtest.h>

using namespace dosagetext;

class GeneratorTest : public testing::Test
{
public:
    testutils::ShiftFhirResourceViewsGuard kbv1_4{"KBV_1_4", date::floor<date::days>(std::chrono::system_clock::now())};
};

TEST_F(GeneratorTest, Test)
{
    ResourceTemplates::KbvBundleOptions options;
    options.kbvVersion = ResourceTemplates::Versions::KBV_ERP_1_4_2;
    const auto kbvBundle =
        model::KbvBundle::fromXml(ResourceTemplates::kbvBundleXml(options), *StaticData::getXmlValidator());
    const auto dosage = kbvBundle.getUniqueResourceByType<model::KbvMedicationRequest>().dosageInstruction();
    const Generator gen{VERSION_1_0_1, Language_DE};
    const auto renderedDosageInstruction = gen.render(dosage);
    EXPECT_EQ(renderedDosageInstruction.language, Language_DE);
    EXPECT_EQ(renderedDosageInstruction.scriptVersion, VERSION_1_0_1);
    EXPECT_EQ(renderedDosageInstruction.renderedDosageInstruction, "0-0-1-0 Stück");
}

class GematikTestData : public GeneratorTest, public testing::WithParamInterface<std::string>
{
public:
    static std::string paramToString(const testing::TestParamInfo<std::string>& info)
    {
        return std::regex_replace(info.param, std::regex("[^A-Za-z0-9_]"), "_");
    }
};

TEST_P(GematikTestData, sample)
{
    const auto filename = "test/dosage/" + GetParam();
    const auto resourceJson = ResourceManager::instance().getStringResource(filename);
    const auto resource = model::UnspecifiedResource::fromJsonNoValidation(resourceJson);
    const auto dosages = resource.getResourceType() == "MedicationStatement"
                             ? model::getDosageFromResource(resource)
                             : model::getDosageInstructionFromResource(resource);
    const Generator gen{VERSION_1_0_1, Language_DE};
    const auto renderedDosage = gen.render(dosages);
    const auto renderedDosageExtension =
        resource.getExtension("http://hl7.org/fhir/5.0/StructureDefinition/extension-" +
                              std::string{resource.getResourceType()} + ".renderedDosageInstruction");
    ASSERT_NE(renderedDosageExtension, std::nullopt);
    ASSERT_NE(renderedDosageExtension->valueMarkdown(), std::nullopt);
    EXPECT_EQ(renderedDosage.renderedDosageInstruction, std::string(*renderedDosageExtension->valueMarkdown()));
}

static std::vector<std::string> makeTestParams(const std::string& dir)
{
    const std::filesystem::directory_iterator it{ResourceManager::instance().getAbsoluteFilename("test/dosage/" + dir)};
    std::vector<std::string> filenames;
    for (const auto& entry : it)
    {
        filenames.emplace_back((dir / entry.path().filename()).string());
    }
    std::ranges::sort(filenames);
    return filenames;
}

INSTANTIATE_TEST_SUITE_P(valid, GematikTestData, testing::ValuesIn(makeTestParams("valid")),
                         &GematikTestData::paramToString);
