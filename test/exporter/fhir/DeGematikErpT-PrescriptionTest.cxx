// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

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
    static std::string name(testing::TestParamInfo<ParamType> p)
    {
        static const std::regex invalidChars{"[^a-zA-Z0-9_]"};
        std::string stem = std::filesystem::path{get<std::string>(p.param)}.stem().generic_string();
        return std::regex_replace(stem, invalidChars, "_");
    }
    static constexpr std::string_view examples1_1 = "test/fhir/transformer/TRezept/Beispiel 1 PZN Verordnung/";
};

TEST_P(DeGematikErpTPrescriptionTest, success)
{
    testutils::ShiftFhirResourceViewsGuard shift{"erp.t-prescription-1.1.0-ballot3",
                                                 date::floor<date::days>(model::Timestamp::now().toChronoTimePoint())};
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
    auto results = resourceFactory.validateGeneric(
        *view, {},
        {fhirtools::DefinitionKey{std::string{model::resource::structure_definition::erp_tprescription_carbon_copy},
                                  get<fhirtools::FhirVersion>(GetParam())}});
    if (results.highestSeverity() > fhirtools::Severity::warning)
    {
        results.dumpToLog();
        ADD_FAILURE();
    }
}

INSTANTIATE_TEST_SUITE_P(examples1_1, DeGematikErpTPrescriptionTest,
                         ::testing::Combine(::testing::Values(ResourceTemplates::Versions::GEM_ERP_TREZEPT_1_1),
                                            ::testing::Values(std::string{DeGematikErpTPrescriptionTest::examples1_1} +
                                                              "example-case-01-digitaler-durchschlag.json")),
                         &DeGematikErpTPrescriptionTest::name);
