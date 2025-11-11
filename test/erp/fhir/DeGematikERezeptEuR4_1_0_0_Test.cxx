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
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>
#include <list>
#include <string>
#include <string_view>



class DeGematikERezeptEuR4_1_0_0_Test : public testing::TestWithParam<std::string> {
public:
    static constexpr std::string_view examplesDir = "test/fhir/examples/de.gematik.erezept.eu-1.0.0";

    static std::list<std::string> files()
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
        EXPECT_EQ(result.size(), 18) <<  "Unexpected number of files in dir: " << examplesDir;
        return result;
    }
};

TEST_P(DeGematikERezeptEuR4_1_0_0_Test, success)
{
    testutils::ShiftFhirResourceViewsGuard unshift{testutils::ShiftFhirResourceViewsGuard::asConfigured};
    const auto& fhirInstance = Fhir::instance();
    auto viewList = fhirInstance.structureRepository(model::Timestamp::fromGermanDate("2025-10-01"));
    auto view = viewList.match(
        {std::string{model::resource::structure_definition::gem_erpeu_pr_par_get_prescription_input}, std::nullopt});
    ASSERT_NE(view, nullptr);
    using ResourceFactory = model::ResourceFactory<model::UnspecifiedResource>;

    auto content = FileHelper::readFileAsString(GetParam());

    auto resourceFactory = ResourceFactory::fromJson(content, *StaticData::getJsonValidator());
    auto results = resourceFactory.validateGeneric(*view, {}, {});
    if (results.highestSeverity() > fhirtools::Severity::warning)
    {
        results.dumpToLog();
        ADD_FAILURE();
    }
}

INSTANTIATE_TEST_SUITE_P(examples, DeGematikERezeptEuR4_1_0_0_Test,
                         ::testing::ValuesIn(DeGematikERezeptEuR4_1_0_0_Test::files()));
