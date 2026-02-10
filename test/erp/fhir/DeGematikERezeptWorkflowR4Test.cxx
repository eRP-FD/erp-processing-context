/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/views/FhirResourceViewList.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/FileHelper.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>
#include <list>
#include <string>
#include <string_view>



class DeGematikERezeptWorkflowR4Test : public testing::TestWithParam<std::tuple<fhirtools::FhirVersion, std::string>> {
public:
    static constexpr std::string_view examples1_5 = "test/fhir/examples/de.gematik.erezept-workflow.r4-1.5.0";
    static constexpr std::string_view examples1_6 = "test/fhir/examples/de.gematik.erezept-workflow.r4-1.6.0";

    static std::list<std::string> files(const std::string_view examplesDir, size_t sampleCount)
    {
        using namespace std::string_literals;
        std::list<std::string> result;
        auto sampleDir = ResourceManager::getAbsoluteFilename(examplesDir);
        Expect3(! sampleDir.empty(),"sample folder not found.", std::logic_error);
        for (const auto& file: std::filesystem::directory_iterator{sampleDir} )
        {
            if (is_regular_file(file) && file.path().extension() == ".json")
            {
                result.emplace_back(file.path().native());
            }
        }
        EXPECT_EQ(result.size(), sampleCount) <<  "Unexpected number of files in dir: " << examplesDir;
        return result;
    }
    static std::string name(testing::TestParamInfo<ParamType> p)
    {
        static const std::regex invalidChars{"[^a-zA-Z0-9_]"};
        std::string stem = std::filesystem::path{get<std::string>(p.param)}.stem().generic_string();
        return std::regex_replace(stem, invalidChars, "_");
    }
};

TEST_P(DeGematikERezeptWorkflowR4Test, success)
{
    using namespace fhirtools::version_literal;
    testutils::ShiftFhirResourceViewsGuard unshift{testutils::ShiftFhirResourceViewsGuard::asConfigured};
    const auto& fhirInstance = Fhir::instance();
    auto viewList = fhirInstance.allViews();
    auto view = viewList.match(
        {std::string{model::resource::structure_definition::task}, get<fhirtools::FhirVersion>(GetParam())});
    ASSERT_NE(view, nullptr);
    using ResourceFactory = model::ResourceFactory<model::UnspecifiedResource>;

    auto content = FileHelper::readFileAsString(get<std::string>(GetParam()));

    auto resourceFactory = ResourceFactory::fromJson(content, *StaticData::getJsonValidator());
    auto results = resourceFactory.validateGeneric(*view, {}, {});
    if (results.highestSeverity() > fhirtools::Severity::warning)
    {
        results.dumpToLog();
        ADD_FAILURE();
    }
}

INSTANTIATE_TEST_SUITE_P(examples1_5, DeGematikERezeptWorkflowR4Test,
                         ::testing::Combine(::testing::Values(ResourceTemplates::Versions::GEM_ERP_1_5_2),
                                            ::testing::ValuesIn(DeGematikERezeptWorkflowR4Test::files(
                                                DeGematikERezeptWorkflowR4Test::examples1_5, 49))),
                         &DeGematikERezeptWorkflowR4Test::name);
INSTANTIATE_TEST_SUITE_P(examples1_6, DeGematikERezeptWorkflowR4Test,
                         ::testing::Combine(::testing::Values(ResourceTemplates::Versions::GEM_ERP_1_6_0),
                                            ::testing::ValuesIn(DeGematikERezeptWorkflowR4Test::files(
                                                DeGematikERezeptWorkflowR4Test::examples1_6, 46))),
                         &DeGematikERezeptWorkflowR4Test::name);
