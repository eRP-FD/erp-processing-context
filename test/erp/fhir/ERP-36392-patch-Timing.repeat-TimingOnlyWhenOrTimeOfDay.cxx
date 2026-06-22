/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/converter/FhirConverter.hxx"
#include "fhirtools/repository/views/FhirResourceViewList.hxx"
#include "fhirtools/util/SaxHandler.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/model/Resource.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <regex>
#include <gtest/gtest.h>


class ERP_36392_patch_Timing_repeat_TimingOnlyWhenOrTimeOfDayTest : public testing::TestWithParam<std::string>
{
public:
    static void SetUpTestSuite()
    {
        (void) Fhir::instance();
    }

    static std::string name(const testing::TestParamInfo<ParamType>& info)
    {
        std::regex notAllowed{R"([^A-Za-z0-9_])"};
        return regex_replace(info.param, notAllowed, "_");
    }

    testutils::ShiftFhirResourceViewsGuard shift{"KBV_1_4", fhirtools::Date{"2026-05-29"}};
};

TEST_P(ERP_36392_patch_Timing_repeat_TimingOnlyWhenOrTimeOfDayTest, negative)
{
    const std::filesystem::path basePath("test/fhir/examples/ERP-36392-negative-examples-v1");
    const auto& fhirInstance = Fhir::instance();
    const auto now = model::Timestamp::now();
    // load resource
    const auto path = ResourceManager::getAbsoluteFilename(basePath / GetParam());
    const auto sample = FileHelper::readFileAsString(path);
    gsl::not_null resource =
        testutils::createResourceNoValidation(Fhir::instance().converter().xmlStringToJson(sample));
    const auto refTime = resource->getValidationReferenceTimestamp().value_or(now);
    const auto profileType = resource->getProfile();
    // determin view
    std::shared_ptr<const fhirtools::FhirStructureRepositoryView> view;
    const auto viewsList = fhirInstance.structureRepository(refTime);
    view = viewsList.match(fhirtools::DefinitionKey{value(resource->getProfileName())});
    ASSERT_NE(view, nullptr);
    // validate
    const auto options = Fhir::instance().defaultValidatorOptions(profileType, refTime);
    const auto result = resource->genericValidate(resource->getProfile(), options, view);
    // assert result
    ASSERT_EQ(result.highestSeverity(), fhirtools::Severity::error);
    ASSERT_TRUE(std::ranges::any_of(result.results(), [](const fhirtools::ValidationError& err) -> bool {
        const auto* constraint = std::get_if<fhirtools::FhirConstraint>(&err.reason);
        return err.severity() == fhirtools::Severity::error && constraint &&
               constraint->getKey() == "TimingOnlyWhenOrTimeOfDay";
    }));
}

INSTANTIATE_TEST_SUITE_P(negative, ERP_36392_patch_Timing_repeat_TimingOnlyWhenOrTimeOfDayTest,
                         testing::ValuesIn<std::list<std::string>>({
                             "B_FD-1676-Verordnung-1.xml",
                             "B_FD-1676-Verordnung-2.xml",
                             "B_FD-1676-MedicationDispense-1.xml",
                             "B_FD-1676-MedicationDispense-2.xml",
                         }),
                         &ERP_36392_patch_Timing_repeat_TimingOnlyWhenOrTimeOfDayTest::name);
