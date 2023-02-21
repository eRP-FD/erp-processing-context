#include "test/fhirtools/SampleValidation.hxx"

class ClosedSliceExtensionsNoReportTest : public fhirtools::test::SampleValidationTest<ClosedSliceExtensionsNoReportTest>
{
public:
    static std::list<std::filesystem::path> fileList()
    {
        auto result = SampleValidationTest::fileList();
        result.merge({
            // clang-format off
            "test/fhir-path/profiles/CodeExtension.xml",
            "test/fhir-path/profiles/IntegerExtension.xml",
            "test/fhir-path/profiles/StringExtension.xml",
            "test/fhir-path/closedSlicingExtensions/profiles/StringAndCodeExtensionResource.xml",
            // clang-format on
        });
        return result;
    }
};


TEST_P(ClosedSliceExtensionsNoReportTest, Samples)
{
    ASSERT_NO_FATAL_FAILURE(run());
}

class ClosedSliceExtensionsReportTest : public ClosedSliceExtensionsNoReportTest
{
protected:
    fhirtools::ValidatorOptions validatorOptions() override
    {
        return { .reportUnknownExtensions = fhirtools::ValidatorOptions::ReportUnknownExtensionsMode::enable };
    }
};

TEST_P(ClosedSliceExtensionsReportTest, Samples)
{
    ASSERT_NO_FATAL_FAILURE(run());
}


using Sample = fhirtools::test::Sample;
using Severity = fhirtools::Severity;
// clang-format off
INSTANTIATE_TEST_SUITE_P( samples, ClosedSliceExtensionsNoReportTest, ::testing::Values(
    Sample{"DomainResource", "test/fhir-path/closedSlicingExtensions/samples/valid_both.json"},
    Sample{"DomainResource", "test/fhir-path/closedSlicingExtensions/samples/valid_code.json"},
    Sample{"DomainResource", "test/fhir-path/closedSlicingExtensions/samples/valid_string.json"},
    Sample{"DomainResource", "test/fhir-path/closedSlicingExtensions/samples/report_unknownUrl1.json"},
    Sample{"DomainResource", "test/fhir-path/closedSlicingExtensions/samples/report_unknownUrl2.json"},
    Sample{"DomainResource", "test/fhir-path/closedSlicingExtensions/samples/report_unexpectedUrl1.json"},
    Sample{"DomainResource", "test/fhir-path/closedSlicingExtensions/samples/report_unexpectedUrl2.json"},
    Sample{"DomainResource", "test/fhir-path/closedSlicingExtensions/samples/report_unexpectedUrl3.json"}
));

INSTANTIATE_TEST_SUITE_P( samples, ClosedSliceExtensionsReportTest, ::testing::Values(
    Sample{"DomainResource", "test/fhir-path/closedSlicingExtensions/samples/valid_both.json"},
    Sample{"DomainResource", "test/fhir-path/closedSlicingExtensions/samples/valid_code.json"},
    Sample{"DomainResource", "test/fhir-path/closedSlicingExtensions/samples/valid_string.json"},
    Sample{"DomainResource", "test/fhir-path/closedSlicingExtensions/samples/report_unknownUrl1.json",
        {
            {std::make_tuple(Severity::unslicedWarning, "element doesn't belong to any slice."), "DomainResource.extension[0]"}
        }
    },
    Sample{"DomainResource", "test/fhir-path/closedSlicingExtensions/samples/report_unknownUrl2.json",
        {
            {std::make_tuple(Severity::unslicedWarning, "element doesn't belong to any slice."), "DomainResource.extension[1]"}
        }
    },
    Sample{"DomainResource", "test/fhir-path/closedSlicingExtensions/samples/report_unexpectedUrl1.json",
        {
            {std::make_tuple(Severity::unslicedWarning, "element doesn't belong to any slice."), "DomainResource.extension[0]"}
        }
    },
    Sample{"DomainResource", "test/fhir-path/closedSlicingExtensions/samples/report_unexpectedUrl2.json",
        {
            {std::make_tuple(Severity::unslicedWarning, "element doesn't belong to any slice."), "DomainResource.extension[1]"}
        }
    },
    Sample{"DomainResource", "test/fhir-path/closedSlicingExtensions/samples/report_unexpectedUrl3.json",
        {
            {std::make_tuple(Severity::unslicedWarning, "element doesn't belong to any slice."), "DomainResource.extension[2]"}
        }
    }
));
//clang-format on
