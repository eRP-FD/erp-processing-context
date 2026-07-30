/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/views/FhirResourceViewList.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/KbvBundle.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "shared/util/FileHelper.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"

#include <fmt/format.h>
#include <gtest/gtest.h>
#include <list>
#include <string>
#include <string_view>


class KbvItaErpTest : public testing::TestWithParam<std::tuple<fhirtools::FhirVersion, std::string>>
{
public:
    static void SetUpTestSuite()
    {
        (void) Fhir::instance();
    }

    static std::list<std::string> files(std::string_view examplesDir, const size_t expectCount, std::string_view suffix = ".xml")
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
    static std::string name(const testing::TestParamInfo<ParamType>& info)
    {
        std::regex notAllowed{R"([^A-Za-z0-9_])"};
        std::filesystem::path path{get<std::string>(info.param)};
        return regex_replace(path.stem().generic_string(), notAllowed, "_");
    }
};

TEST_P(KbvItaErpTest, success)
{
    using ResourceFactory = model::ResourceFactory<model::KbvBundle>;
    const auto& fhirInstance = Fhir::instance();
    auto viewList = fhirInstance.allViews();
    auto view = viewList.match({std::string{model::resource::structure_definition::prescriptionItem},
                                get<fhirtools::FhirVersion>(GetParam())});
    ASSERT_NE(view, nullptr);
    auto content = FileHelper::readFileAsString(get<std::string>(GetParam()));
    auto resourceFactory = ResourceFactory::fromXml(content, *StaticData::getXmlValidator());
    ASSERT_NO_THROW((void) std::move(resourceFactory).getValidated(model::KbvBundle::profileType, view));
}

INSTANTIATE_TEST_SUITE_P(
    1_4, KbvItaErpTest,
    testing::Combine(::testing::Values(ResourceTemplates::Versions::KBV_ERP_1_4_4),
                     ::testing::ValuesIn(KbvItaErpTest::files("test/fhir/examples/kbv.ita.erp-1.4.0", 71))),
    &KbvItaErpTest::name);

INSTANTIATE_TEST_SUITE_P(ERP_37233_Dosierkennzeichen_Testfaelle_Positive, KbvItaErpTest,
                         testing::Combine(::testing::Values(ResourceTemplates::Versions::KBV_ERP_1_4_4),
                                          ::testing::ValuesIn(KbvItaErpTest::files(
                                              "test/fhir/examples/ERP-37233-Dosierkennzeichen_Testfaelle", 9,
                                              "Positiv.xml"))),
                         &KbvItaErpTest::name);


class KbvItaErpNegativeTest : public KbvItaErpTest
{


};

TEST_P(KbvItaErpNegativeTest, fail)
{
    std::set<std::string_view> expectedConstraints{
        "-erp-angabeDosierkennzeichenBtMT-RezeptPflicht", "-erp-angabeDosierkennzeichenPflicht",
        "-erp-angabeDosierungKennzeichenFalse",           "-erp-angabeDosierungKennzeichenTrue",
        "-erp-angabeKennzeichenDosierungRezepturTrue",
    };
    using ResourceFactory = model::ResourceFactory<model::KbvBundle>;
    const auto& fhirInstance = Fhir::instance();
    auto viewList = fhirInstance.allViews();
    auto view = viewList.match({std::string{model::resource::structure_definition::prescriptionItem},
                                get<fhirtools::FhirVersion>(GetParam())});
    ASSERT_NE(view, nullptr);
    auto content = FileHelper::readFileAsString(get<std::string>(GetParam()));
    auto resourceFactory = ResourceFactory::fromXml(content, *StaticData::getXmlValidator());
    try
    {
        (void) std::move(resourceFactory).getValidated(model::KbvBundle::profileType, view);
    }
    catch (const ErpException& ex)
    {
        EXPECT_EQ(ex.status(), HttpStatus::BadRequest);
        EXPECT_STREQ(ex.what(), "FHIR-Validation error");
        const auto& diagnostics = ex.diagnostics();
        ASSERT_TRUE(diagnostics.has_value());
        size_t erpPos = diagnostics->find("-erp-");
        ASSERT_NE(erpPos, std::string::npos);
        size_t colonPos = diagnostics->find_first_of(":", erpPos);
        ASSERT_NE(colonPos, std::string::npos);
        std::string_view constraint{diagnostics->begin() + static_cast<ptrdiff_t>(erpPos),
                                    diagnostics->begin() + static_cast<ptrdiff_t>(colonPos)};
        EXPECT_TRUE(expectedConstraints.contains(constraint)) << constraint;
    }
}


INSTANTIATE_TEST_SUITE_P(ERP_37233_Dosierkennzeichen_Testfaelle_Positive, KbvItaErpNegativeTest,
                         testing::Combine(::testing::Values(ResourceTemplates::Versions::KBV_ERP_1_4_4),
                                          ::testing::ValuesIn(KbvItaErpTest::files(
                                              "test/fhir/examples/ERP-37233-Dosierkennzeichen_Testfaelle", 10,
                                              "Negativ.xml"))),
                         &KbvItaErpTest::name);
