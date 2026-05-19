/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "fhirtools/repository/views/FhirResourceViewList.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/KbvBundle.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "shared/model/Timestamp.hxx"
#include "shared/util/FileHelper.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>


class KbvItaErp_1_3_2_Test : public testing::TestWithParam<std::string>
{
public:
    static void SetUpTestSuite()
    {
        (void) Fhir::instance();
    }

    void validateKbv(std::string_view content)
    {
        const auto view =
            Fhir::instance().allViews().match({std::string{model::resource::structure_definition::prescriptionItem},
                                               ResourceTemplates::Versions::KBV_ERP_1_3_3});
        ASSERT_NE(view, nullptr);
        using ResourceFactory = model::ResourceFactory<model::KbvBundle>;
        auto resourceFactory = ResourceFactory::fromXml(content, *StaticData::getXmlValidator());
        const auto& options{Fhir::instance().defaultValidatorOptions(
            model::ProfileType::KBV_PR_ERP_Bundle, value(resourceFactory.getValidationReferenceTimestamp()))};
        auto results = resourceFactory.validateGeneric(*view, options, {});
        if (results.highestSeverity() > fhirtools::Severity::warning)
        {
            results.dumpToLog();
            ADD_FAILURE() << content;
        }
    }
};

TEST_F(KbvItaErp_1_3_2_Test, kbvBundleXml)
{
    ASSERT_NO_FATAL_FAILURE(
        validateKbv(ResourceTemplates::kbvBundleXml({.kbvVersion = ResourceTemplates::Versions::KBV_ERP_1_3_3})));
}

TEST_F(KbvItaErp_1_3_2_Test, kbvBundleMvoXml)
{
    ASSERT_NO_FATAL_FAILURE(
        validateKbv(ResourceTemplates::kbvBundleMvoXml({.kbvVersion = ResourceTemplates::Versions::KBV_ERP_1_3_3})));
}

TEST_F(KbvItaErp_1_3_2_Test, kbvBundlePkvXml_gkvKvnr)
{
    ASSERT_NO_FATAL_FAILURE(validateKbv(ResourceTemplates::kbvBundlePkvXml({
        .kvnr = model::Kvnr{"X123456789"},
        .kbvVersion{ResourceTemplates::Versions::KBV_ERP_1_3_3},
    })));
}
