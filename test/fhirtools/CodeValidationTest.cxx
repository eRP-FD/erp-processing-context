
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/groups/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/views/FhirResourceViewGroupSet.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "test/util/ResourceManager.hxx"

#include <boost/algorithm/string.hpp>
#include <gtest/gtest.h>
#include <list>
#include <string_view>

namespace
{
static constexpr std::string_view stringCode = R"(
{
    "resourceType": "Resource",
    "id": "stringCode",
    "extension": [
        {
            "url": "http://fhir-tools.test/minifhirtypes/Extension/CodeValidationTest",
            "valueString": "###CODE###"
        }
    ]
})";

static constexpr std::string_view urlCode = R"(
{
    "resourceType": "Resource",
    "id": "urlCode",
    "extension": [
        {
            "url": "http://fhir-tools.test/minifhirtypes/Extension/CodeValidationTest",
            "valueUrl": "###CODE###"
        }
    ]
})";

static constexpr std::string_view codingCode = R"(
{
    "resourceType": "Resource",
    "id": "CodingCode",
    "extension": [
        {
            "url": "http://fhir-tools.test/minifhirtypes/Extension/CodeValidationTest",
            "valueCoding": {
                "system": "###SYSTEM_0###",
                "code": "###CODE_0###"
            }
        }
    ]
})";

static constexpr std::string_view quantityCode = R"(
{
    "resourceType": "Resource",
    "id": "CodingCode",
    "extension": [
        {
            "url": "http://fhir-tools.test/minifhirtypes/Extension/CodeValidationTest",
            "valueQuantity": {
                "value": 1.0,
                "unit": "banana",
                "system": "###SYSTEM_0###",
                "code": "###CODE_0###"
            }
        }
    ]
})";

static constexpr std::string_view codeableConceptCode = R"(
{
    "resourceType": "Resource",
    "id": "CodingCode",
    "extension": [
        {
            "url": "http://fhir-tools.test/minifhirtypes/Extension/CodeValidationTest",
            "valueCodeableConcept": [
                {
                    "coding":  {
                        "system": "###SYSTEM_0###",
                        "code": "###CODE_0###"
                    }
                }
            ]
        }
    ]
})";

static constexpr std::string_view codeableConceptMultiCode = R"(
{
    "resourceType": "Resource",
    "id": "CodingCode",
    "extension": [
        {
            "url": "http://fhir-tools.test/minifhirtypes/Extension/CodeValidationTest",
            "valueCodeableConcept": [
                {
                    "coding":  {
                        "system": "###SYSTEM_0###",
                        "code": "###CODE_0###"
                    }
                },
                {
                    "coding":  {
                        "system": "###SYSTEM_1###",
                        "code": "###CODE_1###"
                    }
                }
            ]
        }
    ]
})";

}

struct CodeValidationParam {
    std::string name;
    // base profile without restrictions:
    std::string profile = "http://fhir-tools.test/minifhirtypes/Resource";
    const std::string_view& resourceTemplate;
    struct Coding {
        std::string system;
        std::string code;
    };
    std::list<Coding> coding{};
    std::string code{};
};

void PrintTo(const CodeValidationParam& param, std::ostream* os)
{
    (*os) << R"({ "name": ")" << param.name << '"';
    (*os) << R"(, "profile": ")" << param.profile << '"';
    if (!param.coding.empty())
    {
        (*os) << R"(, "coding": [ ")";
        for (std::string_view sep; const auto& coding: param.coding)
        {
            (*os) << sep << coding.system << R"(": ")" << coding.code << '"';
            sep = R"(, ")";
        }
        (*os) << R"( ], )";
    }
    if (!param.code.empty())
    {
        (*os) << R"("code": ")" << param.code << '"';
    }
    (*os) << " }";
}

class CodeValidationTest : public ::testing::TestWithParam<std::tuple<fhirtools::Severity, CodeValidationParam>>
{
public:
    static constexpr char systemComplete[] = "http://fhir-tools.test/minifhirtypes/CodeSystem/CodeValidationTest/complete";
    static constexpr char systemEmpty[] = "http://fhir-tools.test/minifhirtypes/CodeSystem/CodeValidationTest/not-present";
    static constexpr char profileWithBinding[] = "http://fhir-tools.test/minifhirtypes/Resource/CodeValidationTest";
    void SetUp() override
    {

        backend.load(
            {
                ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/minifhirtypes.xml"),
                ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/mini_code_validation_profiles.xml"),
            },
            resolver);
        view = fhirtools::FhirResourceViewGroupSet::create("CodeValidationTestView", resolver.group(), &backend);
    }
    static const std::string& name(const ::testing::TestParamInfo<ParamType>& info)
    {
        return get<CodeValidationParam>(info.param).name;
    }
protected:
    fhirtools::FhirResourceGroupConst resolver{"CodeValidationTestGroups"};
    fhirtools::FhirStructureRepositoryBackend backend;
    std::shared_ptr<fhirtools::FhirStructureRepositoryView> view;
};

TEST_P(CodeValidationTest, run)
{
    const auto& [expectedSeverity, param] = GetParam();
    std::string resourceStr{param.resourceTemplate};
    if (!param.code.empty())
    {
        boost::replace_all(resourceStr, "###CODE###", param.code);
    }
    for (size_t idx = 0; const auto& coding: param.coding)
    {
        boost::replace_all(resourceStr, "###SYSTEM_" + std::to_string(idx) + "###" , coding.system);
        boost::replace_all(resourceStr, "###CODE_" + std::to_string(idx) + "###" , coding.code);
        ++idx;
    }
    auto resourceJson = model::NumberAsStringParserDocument::fromJson(resourceStr);
    auto resourceElement = std::make_shared<ErpElement>(&backend, std::weak_ptr<const ErpElement>{}, "Resource", &resourceJson);
    auto validationResult = fhirtools::FhirPathValidator::validateWithProfiles(view, resourceElement, "Resource",
                                                       {fhirtools::DefinitionKey{param.profile}});

    if (validationResult.highestSeverity() != expectedSeverity)
    {
        validationResult.dumpToLog();
        ADD_FAILURE() << resourceStr;
    }
}

INSTANTIATE_TEST_SUITE_P(noBindingSuccess, CodeValidationTest,
                         ::testing::Combine(::testing::Values(fhirtools::Severity::debug),
                                            ::testing::ValuesIn(
                                                std::list<CodeValidationParam>{
                                                    {
                                                        .name = "string",
                                                        .resourceTemplate = stringCode,
                                                        .code = "not-checked-anyways",
                                                    },
                                                    {
                                                        .name = "url",
                                                        .resourceTemplate = urlCode,
                                                        .code = "not-checked-anyways",
                                                    },
                                                    {
                                                        .name = "Coding",
                                                        .resourceTemplate = codingCode,
                                                        .coding = {{CodeValidationTest::systemComplete, "Test_0"}},
                                                    },
                                                    {
                                                        .name = "Quantity",
                                                        .resourceTemplate = quantityCode,
                                                        .coding = {{CodeValidationTest::systemComplete, "Test_0"}},
                                                    },
                                                    {
                                                        .name = "CodeableConcept",
                                                        .resourceTemplate = codeableConceptCode,
                                                        .coding = {{CodeValidationTest::systemComplete, "Test_0"}},
                                                    },
                                                    {
                                                        .name = "CodeableConceptMulti",
                                                        .resourceTemplate = codeableConceptMultiCode,
                                                        .coding =
                                                            {
                                                                {CodeValidationTest::systemComplete, "Test_0"},
                                                                {CodeValidationTest::systemComplete, "Test_1"},

                                                            },
                                                    },
                                                })),
                         &CodeValidationTest::name);


INSTANTIATE_TEST_SUITE_P(noBindingEmptyCodeSystemSuccess, CodeValidationTest,
                         ::testing::Combine(::testing::Values(fhirtools::Severity::warning),
                                            ::testing::ValuesIn(
                                                std::list<CodeValidationParam>{
                                                    {
                                                        .name = "Coding",
                                                        .resourceTemplate = codingCode,
                                                        .coding = {{CodeValidationTest::systemEmpty, "not-checked-anyways"}},
                                                    },
                                                    {
                                                        .name = "Quantity",
                                                        .resourceTemplate = quantityCode,
                                                        .coding = {{CodeValidationTest::systemEmpty, "not-checked-anyways"}},
                                                    },
                                                    {
                                                        .name = "CodeableConcept",
                                                        .resourceTemplate = codeableConceptCode,
                                                        .coding = {{CodeValidationTest::systemEmpty, "not-checked-anyways"}},
                                                    },
                                                    {
                                                        .name = "CodeableConceptMulti",
                                                        .resourceTemplate = codeableConceptMultiCode,
                                                        .coding =
                                                        {
                                                            {CodeValidationTest::systemEmpty, "not-checked-anyways"},
                                                            {CodeValidationTest::systemEmpty, "not-checked-anyways"},

                                                        },
                                                    },
                                                })),
                         &CodeValidationTest::name);


INSTANTIATE_TEST_SUITE_P(noBindingUnknownCode, CodeValidationTest,
                         ::testing::Combine(::testing::Values(fhirtools::Severity::error),
                                            ::testing::ValuesIn(
                                                std::list<CodeValidationParam>{
                                                    {
                                                        .name = "Coding",
                                                        .resourceTemplate = codingCode,
                                                        .coding = {{CodeValidationTest::systemComplete, "unknown"}},
                                                    },
                                                    {
                                                        .name = "Quantity",
                                                        .resourceTemplate = quantityCode,
                                                        .coding = {{CodeValidationTest::systemComplete, "unknown"}},
                                                    },
                                                    {
                                                        .name = "CodeableConcept",
                                                        .resourceTemplate = codeableConceptCode,
                                                        .coding = {{CodeValidationTest::systemComplete, "unknown"}},
                                                    },
                                                    {
                                                        .name = "CodeableConceptMultiFirst",
                                                        .resourceTemplate = codeableConceptMultiCode,
                                                        .coding =
                                                        {
                                                            {CodeValidationTest::systemComplete, "unknown"},
                                                            {CodeValidationTest::systemComplete, "Test_1"},

                                                        },
                                                    },
                                                    {
                                                        .name = "CodeableConceptMultiSecond",
                                                        .resourceTemplate = codeableConceptMultiCode,
                                                        .coding =
                                                        {
                                                            {CodeValidationTest::systemComplete, "Test_0"},
                                                            {CodeValidationTest::systemComplete, "unknown"},

                                                        },
                                                    },
                                                })),
                         &CodeValidationTest::name);

INSTANTIATE_TEST_SUITE_P(noBindingUnknownSystem, CodeValidationTest,
                         ::testing::Combine(::testing::Values(fhirtools::Severity::warning),
                                            ::testing::ValuesIn(
                                                std::list<CodeValidationParam>{
                                                    {
                                                        .name = "Coding",
                                                        .resourceTemplate = codingCode,
                                                        .coding = {{"unknown-system", "Test_0"}},
                                                    },
                                                    {
                                                        .name = "Quantity",
                                                        .resourceTemplate = quantityCode,
                                                        .coding = {{"unknown-system", "Test_0"}},
                                                    },
                                                    {
                                                        .name = "CodeableConcept",
                                                        .resourceTemplate = codeableConceptCode,
                                                        .coding = {{"unknown-system", "Test_0"}},
                                                    },
                                                    {
                                                        .name = "CodeableConceptMultiFirst",
                                                        .resourceTemplate = codeableConceptMultiCode,
                                                        .coding =
                                                        {
                                                            {"unknown-system", "Test_0"},
                                                            {CodeValidationTest::systemComplete, "Test_1"},

                                                        },
                                                    },
                                                    {
                                                        .name = "CodeableConceptMultiSecond",
                                                        .resourceTemplate = codeableConceptMultiCode,
                                                        .coding =
                                                        {
                                                            {CodeValidationTest::systemComplete, "Test_0"},
                                                            {"unknown-system", "Test_1"},

                                                        },
                                                    },
                                                })),
                         &CodeValidationTest::name);


INSTANTIATE_TEST_SUITE_P(withBindingSuccess, CodeValidationTest,
                         ::testing::Combine(::testing::Values(fhirtools::Severity::debug),
                                            ::testing::ValuesIn(
                                                std::list<CodeValidationParam>{
                                                    {
                                                        .name = "string",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = stringCode,
                                                        .code = "Test_0",
                                                    },
                                                    {
                                                        .name = "url",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = urlCode,
                                                        .code = "Test_0",
                                                    },
                                                    {
                                                        .name = "Coding",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = codingCode,
                                                        .coding = {{CodeValidationTest::systemComplete, "Test_0"}},
                                                    },
                                                    {
                                                        .name = "Quantity",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = quantityCode,
                                                        .coding = {{CodeValidationTest::systemComplete, "Test_0"}},
                                                    },
                                                    {
                                                        .name = "CodeableConcept",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = codeableConceptCode,
                                                        .coding = {{CodeValidationTest::systemComplete, "Test_0"}},
                                                    },
                                                    {
                                                        .name = "CodeableConceptMulti",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = codeableConceptMultiCode,
                                                        .coding =
                                                        {
                                                            {CodeValidationTest::systemComplete, "Test_0"},
                                                            {CodeValidationTest::systemComplete, "Test_1"},

                                                        },
                                                    },
                                                })),
                         &CodeValidationTest::name);


INSTANTIATE_TEST_SUITE_P(withBindingUnknownCode, CodeValidationTest,
                         ::testing::Combine(::testing::Values(fhirtools::Severity::error),
                                            ::testing::ValuesIn(
                                                std::list<CodeValidationParam>{
                                                    {
                                                        .name = "string",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = stringCode,
                                                        .code = "unknown",
                                                    },
                                                    {
                                                        .name = "url",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = urlCode,
                                                        .code = "unknown",
                                                    },
                                                    {
                                                        .name = "Coding",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = codingCode,
                                                        .coding = {{CodeValidationTest::systemComplete, "unknown"}},
                                                    },
                                                    {
                                                        .name = "Quantity",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = quantityCode,
                                                        .coding = {{CodeValidationTest::systemComplete, "unknown"}},
                                                    },
                                                    {
                                                        .name = "CodeableConcept",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = codeableConceptCode,
                                                        .coding = {{CodeValidationTest::systemComplete, "unknown"}},
                                                    },
                                                    {
                                                        .name = "CodeableConceptMultiFirst",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = codeableConceptMultiCode,
                                                        .coding =
                                                        {
                                                            {CodeValidationTest::systemComplete, "unknown"},
                                                            {CodeValidationTest::systemComplete, "Test_1"},

                                                        },
                                                    },
                                                    {
                                                        .name = "CodeableConceptMultiSecond",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = codeableConceptMultiCode,
                                                        .coding =
                                                        {
                                                            {CodeValidationTest::systemComplete, "Test_0"},
                                                            {CodeValidationTest::systemComplete, "unknown"},

                                                        },
                                                    },
                                                })),
                         &CodeValidationTest::name);

INSTANTIATE_TEST_SUITE_P(withBindingKnownWrongSystem, CodeValidationTest,
                         ::testing::Combine(::testing::Values(fhirtools::Severity::error),
                                            ::testing::ValuesIn(
                                                std::list<CodeValidationParam>{
                                                    {
                                                        .name = "Coding",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = codingCode,
                                                        .coding = {{CodeValidationTest::systemEmpty, "Test_0"}},
                                                    },
                                                    {
                                                        .name = "Quantity",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = quantityCode,
                                                        .coding = {{CodeValidationTest::systemEmpty, "Test_0"}},
                                                    },
                                                    {
                                                        .name = "CodeableConcept",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = codeableConceptCode,
                                                        .coding = {{CodeValidationTest::systemEmpty, "Test_0"}},
                                                    },
                                                    {
                                                        .name = "CodeableConceptMultiFirst",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = codeableConceptMultiCode,
                                                        .coding =
                                                        {
                                                            {CodeValidationTest::systemEmpty, "Test_0"},
                                                            {CodeValidationTest::systemComplete, "Test_1"},

                                                        },
                                                    },
                                                    {
                                                        .name = "CodeableConceptMultiSecond",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = codeableConceptMultiCode,
                                                        .coding =
                                                        {
                                                            {CodeValidationTest::systemComplete, "Test_0"},
                                                            {CodeValidationTest::systemEmpty, "Test_1"},

                                                        },
                                                    },
                                                })),
                         &CodeValidationTest::name);

INSTANTIATE_TEST_SUITE_P(withBindingUnknownWrongSystem, CodeValidationTest,
                         ::testing::Combine(::testing::Values(fhirtools::Severity::error),
                                            ::testing::ValuesIn(
                                                std::list<CodeValidationParam>{
                                                    {
                                                        .name = "Coding",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = codingCode,
                                                        .coding = {{"unknown-system", "Test_0"}},
                                                    },
                                                    {
                                                        .name = "Quantity",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = quantityCode,
                                                        .coding = {{"unknown-system", "Test_0"}},
                                                    },
                                                    {
                                                        .name = "CodeableConcept",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = codeableConceptCode,
                                                        .coding = {{"unknown-system", "Test_0"}},
                                                    },
                                                    {
                                                        .name = "CodeableConceptMultiFirst",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = codeableConceptMultiCode,
                                                        .coding =
                                                        {
                                                            {"unknown-system", "Test_0"},
                                                            {CodeValidationTest::systemComplete, "Test_1"},

                                                        },
                                                    },
                                                    {
                                                        .name = "CodeableConceptMultiSecond",
                                                        .profile = CodeValidationTest::profileWithBinding,
                                                        .resourceTemplate = codeableConceptMultiCode,
                                                        .coding =
                                                        {
                                                            {CodeValidationTest::systemComplete, "Test_0"},
                                                            {"unknown-system", "Test_1"},

                                                        },
                                                    },
                                                })),
                         &CodeValidationTest::name);
