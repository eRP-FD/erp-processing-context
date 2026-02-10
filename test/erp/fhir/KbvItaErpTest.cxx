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
#include "test/util/StaticData.hxx"

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

    static std::list<std::string> files(std::string_view examplesDir, const size_t expectCount)
    {
        using namespace std::string_literals;
        std::list<std::string> result;
        auto sampleDir = ResourceManager::getAbsoluteFilename(examplesDir);
        Expect3(! sampleDir.empty(), "sample folder not found.", std::logic_error);
        for (const auto& file : std::filesystem::directory_iterator{sampleDir})
        {
            if (is_regular_file(file) && file.path().extension() == ".xml")
            {
                result.emplace_back(file.path().native());
            }
        }
        Expect(result.size() == expectCount, "Unexpected number of files in dir: "s.append(examplesDir));
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
    1_4_0, KbvItaErpTest,
    testing::Combine(::testing::Values(fhirtools::FhirVersion{"1.4.0"}),
                     ::testing::ValuesIn(KbvItaErpTest::files("test/fhir/examples/kbv.ita.erp-1.4.0", 71))),
    &KbvItaErpTest::name);
