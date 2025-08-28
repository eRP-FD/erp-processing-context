/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "test/fhirtools/SampleValidation.hxx"

class UriValidationTest : public fhirtools::test::SampleValidationTest<UriValidationTest>
{
public:
    using Sample = fhirtools::test::Sample;
    static std::list<std::filesystem::path> fileList()
    {
        auto result = SampleValidationTest::fileList();
        return {
            "test/fhir-path/profiles/minifhirtypes.xml",
            "test/fhir-path/profiles/UriValidationProfiles.xml",
        };
    }

    fhirtools::ValidatorOptions validatorOptions() override
    {
        auto result = SampleValidationTest::validatorOptions();
        result.levels.invalidUrnUuidInUri.emplace(fhirtools::Severity::error);
        return result;
    }


    static std::list<Sample> samples()
    {
        using enum fhirtools::Severity;
        using enum fhirtools::ExtendedValidation;
        return {
            Sample{"UriValidation", "test/fhir-path/samples/valid_UriValidation_direct.json"},
            Sample{"UriValidation", "test/fhir-path/samples/valid_UriValidation_direct_noUrnUuid.json"},
            Sample{"UriValidation", "test/fhir-path/samples/valid_UriValidation_derived.json"},
            Sample{"UriValidation", "test/fhir-path/samples/valid_UriValidation_derived_noUrnUuid.json"},
            Sample{"UriValidation", "test/fhir-path/samples/valid_UriValidation_direct_data_absent.json"},
            Sample{
                "UriValidation",
                "test/fhir-path/samples/invalid_UriValidation_direct_uppercase.json",
                {
                    {std::make_tuple(error, invalidUrnUuidInUri), "UriValidation.uri"},
                },
            },
            Sample{
                "UriValidation",
                "test/fhir-path/samples/invalid_UriValidation_derived_uppercase.json",
                {
                    {std::make_tuple(error, invalidUrnUuidInUri), "UriValidation.url"},
                },
            },
        };
    }
};

TEST_P(UriValidationTest, run)
{
    ASSERT_NO_FATAL_FAILURE(run());
}

INSTANTIATE_TEST_SUITE_P(samples, UriValidationTest, testing::ValuesIn(UriValidationTest::samples()),
                         &fhirtools::test::Sample::name);
