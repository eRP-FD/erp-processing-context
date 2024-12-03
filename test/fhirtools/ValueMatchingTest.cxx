#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/repository/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/FhirResourceViewGroupSet.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "test/fhirtools/DefaultFhirStructureRepository.hxx"
#include "test/util/ResourceManager.hxx"

#include <boost/algorithm/string/replace.hpp>
#include <gtest/gtest.h>


struct ValueMatchingTestParam {
    std::string_view extension;
    std::string_view type;
    std::string_view value;
    bool expectSuccess;
};

class ValueMatchingTest : public testing::TestWithParam<ValueMatchingTestParam>
{
protected:
    std::string resource =
        R"--(
{
    "resourceType": "Resource",
    "id":"rootResource",
    "meta": {
        "profile": [ "http://fhir-tools.test/minifhirtypes/PatternAndFixed" ]
    },
    "extension": [
        {
            "url": "###EXTENSION URL###",
            "value###TYPE###": "###VALUE###"
        }
    ]
})--";

    void SetUp() override
    {
        static constexpr char miniFhirAndPatternAndFixed[] = "minifhir+pattern_and_fixed";
        fhirtools::FhirResourceGroupConst resolver{miniFhirAndPatternAndFixed};
        backend.load(
            {
                ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/minifhirtypes.xml"),
                ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/mini_pattern_and_fixed.xml"),
            },
            resolver);
        view = std::make_shared<fhirtools::FhirResourceViewGroupSet>(
            miniFhirAndPatternAndFixed, resolver.findGroupById(miniFhirAndPatternAndFixed), &backend);
    }
    fhirtools::FhirStructureRepositoryBackend backend;
    std::shared_ptr<const fhirtools::FhirStructureRepository> view;
};

TEST_P(ValueMatchingTest, run)
{
    const auto& param = GetParam();
    boost::replace_all(resource, "###EXTENSION URL###", param.extension);
    boost::replace_all(resource, "###TYPE###", param.type);
    boost::replace_all(resource, "###VALUE###", param.value);
    const auto resourceDoc = model::NumberAsStringParserDocument::fromJson(resource);
    const auto element =
        std::make_shared<const ErpElement>(view, std::weak_ptr<const fhirtools::Element>{}, "Resource", &resourceDoc);
    fhirtools::ValidationResults results;
    EXPECT_NO_THROW(results = fhirtools::FhirPathValidator::validate(element, "Resource", {}));
    results.dumpToLog();
    if (param.expectSuccess)
    {
        EXPECT_LE(results.highestSeverity(), fhirtools::Severity::debug);
    }
    else
    {
        EXPECT_GE(results.highestSeverity(), fhirtools::Severity::error);
    }
}

INSTANTIATE_TEST_SUITE_P(valid, ValueMatchingTest,
                         testing::ValuesIn(std::initializer_list<ValueMatchingTestParam>{
                             {
                                 .extension = "fixedString",
                                 .type = "String",
                                 .value = "A string in a fixed.",
                                 .expectSuccess = true,
                             },
                             {
                                 .extension = "patternString",
                                 .type = "String",
                                 .value = "A string in a pattern.",
                                 .expectSuccess = true,
                             },
                             {
                                 .extension = "fixedDecimal",
                                 .type = "Decimal",
                                 .value = "1.1",
                                 .expectSuccess = true,
                             },
                             {
                                 .extension = "patternDecimal",
                                 .type = "Decimal",
                                 .value = "1.2",
                                 .expectSuccess = true,
                             },
                             {
                                 .extension = "fixedDecimalLeadingZero",
                                 .type = "Decimal",
                                 .value = "01.1",
                                 .expectSuccess = true,
                             },
                             {
                                 .extension = "patternDecimalLeadingZero",
                                 .type = "Decimal",
                                 .value = "01.2",
                                 .expectSuccess = true,
                             },
                             {
                                 .extension = "fixedDecimalTrailingZero",
                                 .type = "Decimal",
                                 .value = "1.10",
                                 .expectSuccess = true,
                             },
                             {
                                 .extension = "patternDecimalTrailingZero",
                                 .type = "Decimal",
                                 .value = "1.20",
                                 .expectSuccess = true,
                             },
                         }));

INSTANTIATE_TEST_SUITE_P(invalid, ValueMatchingTest,
                         testing::ValuesIn(std::initializer_list<ValueMatchingTestParam>{
                             {
                                 .extension = "fixedString",
                                 .type = "String",
                                 .value = "A string in a pattern.",// should be "A string in a fixed."
                                 .expectSuccess = false,
                             },
                             {
                                 .extension = "patternString",
                                 .type = "String",
                                 .value = "A string in a fixed.",// should be "A string in a pattern."
                                 .expectSuccess = false,
                             },
                             {
                                 .extension = "fixedDecimal",
                                 .type = "Decimal",
                                 .value = "1.10",// should be "1.1"
                                 .expectSuccess = false,
                             },
                             {
                                 .extension = "patternDecimal",
                                 .type = "Decimal",
                                 .value = "01.2",// should be "1.2"
                                 .expectSuccess = false,
                             },
                             {
                                 .extension = "fixedDecimalLeadingZero",
                                 .type = "Decimal",
                                 .value = "1.1",// should be "01.1"
                                 .expectSuccess = false,
                             },
                             {
                                 .extension = "patternDecimalLeadingZero",
                                 .type = "Decimal",
                                 .value = "1.2",// shaould be "01.2"
                                 .expectSuccess = false,
                             },
                             {
                                 .extension = "fixedDecimalTrailingZero",
                                 .type = "Decimal",
                                 .value = "1.1",// should be ""1.10""
                                 .expectSuccess = false,
                             },
                             {
                                 .extension = "patternDecimalTrailingZero",
                                 .type = "Decimal",
                                 .value = "1.2",// should be "1.20"
                                 .expectSuccess = false,
                             },
                         }));
