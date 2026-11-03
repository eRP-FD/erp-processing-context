// (C) Copyright IBM Deutschland GmbH 2021, 2026
// (C) Copyright IBM Corp. 2021, 2026
// non-exclusively licensed to gematik GmbH

#include "exporter/model/trezept/ErpTPrescriptionCarbonCopy.hxx"
#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/views/FhirResourceViewList.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/model/Timestamp.hxx"
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

class DeGematikErpTPrescriptionTest : public testing::TestWithParam<std::tuple<fhirtools::FhirVersion, std::string>>
{
public:
    static void SetUpTestSuite()
    {
        (void) Fhir::instance();
    }

    static std::list<std::string> files(std::string_view examplesDir, const size_t expectCount,
                                        std::string_view suffix = ".json")
    {
        using namespace std::string_literals;
        std::list<std::string> result;
        auto sampleDir = ResourceManager::getAbsoluteFilename(examplesDir);
        Expect3(! sampleDir.empty(), "sample folder not found.", std::logic_error);
        for (const auto& file : std::filesystem::directory_iterator{sampleDir})
        {
            if (is_regular_file(file) && file.path().generic_string().ends_with(suffix))
            {
                result.emplace_back(file.path().native());
            }
        }
        Expect(result.size() == expectCount,
               fmt::format("Unexpected number of files in dir (expected: {} actual: {}): ", expectCount, result.size(),
                           examplesDir));
        return result;
    }

    static std::string name(testing::TestParamInfo<ParamType> p)
    {
        static const std::regex invalidChars{"[^a-zA-Z0-9_]"};
        std::string stem = std::filesystem::path{get<std::string>(p.param)}.stem().generic_string();
        return std::regex_replace(stem, invalidChars, "_");
    }
    static std::list<std::string> examples1_v1_2_0() {
        return {
            "test/fhir/transformer/TRezept/Beispiel 1 PZN Verordnung/example-case-01-digitaler-durchschlag.json",
            "test/fhir/transformer/TRezept/Beispiel 2 Wirkstoff Verordnung/example-case-02-digitaler-durchschlag.json",
            "test/fhir/transformer/TRezept/Beispiel 3 Freitext Verordnung/example-case-03-digitaler-durchschlag.json",
            "test/fhir/transformer/TRezept/Beispiel 4 PZN Verordnung mit absoluter Refernzierung/example-case-04-digitaler-durchschlag.json",
        };
    }
    static std::list<std::string> examples2_v1_2_0() {
        return files("test/fhir/examples/de.gematik.erp.t-prescription-1.2.0", 10);
    };
};

TEST_P(DeGematikErpTPrescriptionTest, success)
{
    const auto& fhirInstance = Fhir::instance();
    auto viewList = fhirInstance.allViews();
    auto view = viewList.match({std::string{model::resource::structure_definition::erp_tprescription_carbon_copy},
                                get<fhirtools::FhirVersion>(GetParam())});
    ASSERT_NE(view, nullptr);
    using ResourceFactory = model::ResourceFactory<model::UnspecifiedResource>;

    auto content = FileHelper::readFileAsString(ResourceManager::getAbsoluteFilename(get<std::string>(GetParam())));
    bool isJson = content.find_first_of("{") != std::string::npos;

    auto resourceFactory = isJson ? ResourceFactory::fromJson(content, *StaticData::getJsonValidator())
                                  : ResourceFactory::fromXml(content, *StaticData::getXmlValidator());

    auto results = resourceFactory.validateGeneric(*view, {}, {});
    if (results.highestSeverity() > fhirtools::Severity::warning)
    {
        results.dumpToLog();
        ADD_FAILURE();
    }
}

INSTANTIATE_TEST_SUITE_P(examples1_v_1_2_0, DeGematikErpTPrescriptionTest,
                         ::testing::Combine(::testing::Values(ResourceTemplates::Versions::GEM_ERP_TREZEPT_1_2_0),
                                            ::testing::ValuesIn(DeGematikErpTPrescriptionTest::examples1_v1_2_0())),
                         &DeGematikErpTPrescriptionTest::name);

INSTANTIATE_TEST_SUITE_P(examples2_v1_2_0, DeGematikErpTPrescriptionTest,
                         testing::Combine(::testing::Values(ResourceTemplates::Versions::GEM_ERP_TREZEPT_1_2_0),
                                          ::testing::ValuesIn(DeGematikErpTPrescriptionTest::examples2_v1_2_0())),
                         &DeGematikErpTPrescriptionTest::name);
