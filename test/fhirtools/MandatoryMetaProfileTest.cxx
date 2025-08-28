#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/groups/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/views/FhirResourceViewGroupSet.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "test/util/ResourceManager.hxx"


#include <gtest/gtest.h>
#include <list>
#include <string>


class MandatoryMetaProfileTest : public testing::TestWithParam<std::string>
{
public:
    void SetUp() override {
        repo.load(
            {
                ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/minifhirtypes.xml"),
                  ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/mandatory_meta_profile_variations.xml"),
            },
            resolver);
        view = fhirtools::FhirResourceViewGroupSet::create("testView", resolver.group(), &repo);
    }
protected:
    fhirtools::FhirStructureRepositoryBackend repo;
    fhirtools::FhirResourceGroupConst resolver{"test"};
    std::shared_ptr<fhirtools::FhirResourceViewGroupSet> view;
    fhirtools::ValidationResults validate(std::string_view sample, const fhirtools::ValidatorOptions& options)
    {
        auto doc = model::NumberAsStringParserDocument::fromJson(sample);
        auto element = std::make_shared<ErpElement>(view, std::weak_ptr<ErpElement>{}, "Resource", &doc);
        return fhirtools::FhirPathValidator::validateWithProfiles(
            element, "Resource", std::set{fhirtools::DefinitionKey{GetParam()}}, options);
    }
};

TEST_P(MandatoryMetaProfileTest, success)
{
    static constexpr char sampleStr[] =
    R"(
    {
        "resourceType": "Resource",
        "id": "MandatoryMetaProfileTestSample",
        "meta": {
            "source": "Source value to select slice of http://fhir-tools.test/minifhirtypes/Resource/mandatory_profile_via_slice_on_meta",
            "profile": [ "http://fhir-tools.test/minifhirtypes/Resource" ]
        }
    }
    )";
    auto results = validate(sampleStr, {.levels{. missingOrExtraMetaProfile = fhirtools::Severity::error}});
    EXPECT_LE(results.highestSeverity(), fhirtools::Severity::warning);
    results.dumpToLog();

}

TEST_P(MandatoryMetaProfileTest, missingProfileError)
{
    static constexpr char sampleStr[] =
    R"(
    {
        "resourceType": "Resource",
        "id": "MandatoryMetaProfileTestSample",
        "meta": {
            "source": "Source value to select slice of http://fhir-tools.test/minifhirtypes/Resource/mandatory_profile_via_slice_on_meta"
        }
    }
    )";
    auto results = validate(sampleStr, {.levels{. missingOrExtraMetaProfile = fhirtools::Severity::error}});
    EXPECT_TRUE(std::ranges::any_of(results.results(), [](const fhirtools::ValidationError& err) -> bool {
        return to_string(err) == "Resource.meta: error: missing profile in meta.profile";
    }));
}

TEST_P(MandatoryMetaProfileTest, missingProfileWarning)
{
    static constexpr char sampleStr[] =
    R"(
    {
        "resourceType": "Resource",
        "id": "MandatoryMetaProfileTestSample",
        "meta": {
            "source": "Source value to select slice of http://fhir-tools.test/minifhirtypes/Resource/mandatory_profile_via_slice_on_meta"
        }
    }
    )";
    auto results = validate(sampleStr, {.levels{. missingOrExtraMetaProfile = fhirtools::Severity::warning}});
    EXPECT_TRUE(std::ranges::any_of(results.results(), [](const fhirtools::ValidationError& err) -> bool {
        return to_string(err) == "Resource.meta: warning: missing profile in meta.profile";
    }));
}

TEST_P(MandatoryMetaProfileTest, extraProfileError)
{
    static constexpr char sampleStr[] =
    R"(
    {
        "resourceType": "Resource",
        "id": "MandatoryMetaProfileTestSample",
        "meta": {
            "source": "Source value to select slice of http://fhir-tools.test/minifhirtypes/Resource/mandatory_profile_via_slice_on_meta",
            "profile": [ "http://fhir-tools.test/minifhirtypes/Resource", "http://fhir-tools.test/minifhirtypes/Resource" ]
        }
    }
    )";
    auto results = validate(sampleStr, {.levels{. missingOrExtraMetaProfile = fhirtools::Severity::error}});
    EXPECT_TRUE(std::ranges::any_of(results.results(), [](const fhirtools::ValidationError& err) -> bool {
        return to_string(err) == "Resource.meta: error: extra profile(s) in meta.profile";
    }));
}


INSTANTIATE_TEST_SUITE_P(profiles, MandatoryMetaProfileTest,
                         testing::ValuesIn<std::list<std::string>>({
                             "http://fhir-tools.test/minifhirtypes/Resource/mandatory_profile_direct",
                             "http://fhir-tools.test/minifhirtypes/Resource/mandatory_profile_via_profile_on_meta",
                             "http://fhir-tools.test/minifhirtypes/Resource/mandatory_profile_via_slice_on_meta",
                         }));
