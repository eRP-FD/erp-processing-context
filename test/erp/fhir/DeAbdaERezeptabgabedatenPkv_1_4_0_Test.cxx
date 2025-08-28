/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/AbgabedatenPkvBundle.hxx"
#include "fhirtools/repository/views/FhirResourceViewList.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "shared/model/Timestamp.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>

class DeAbdaERezeptabgabedatenPkv_1_4_0_Test : public testing::TestWithParam<std::string>
{
public:
    static void SetUpTestSuite()
    {
        (void) Fhir::instance();
    }

    void validateKbv(std::string_view content)
    {
        const auto view =
            Fhir::instance().allViews().match({std::string{model::resource::structure_definition::dispenseItem},
                                               ResourceTemplates::Versions::DAV_PKV_1_4_0});
        ASSERT_NE(view, nullptr);
        using ResourceFactory = model::ResourceFactory<model::AbgabedatenPkvBundle>;
        auto resourceFactory = ResourceFactory::fromXml(content, *StaticData::getXmlValidator());
        const auto& options{Fhir::instance().defaultValidatorOptions(
            resourceFactory.profileType(), value(resourceFactory.getValidationReferenceTimestamp()))};
        auto results = resourceFactory.validateGeneric(*view, options, {});
        if (results.highestSeverity() > fhirtools::Severity::warning)
        {
            results.dumpToLog();
            ADD_FAILURE() << content;
        }
    }
};

TEST_F(DeAbdaERezeptabgabedatenPkv_1_4_0_Test, davDispenseItemXml)
{
    ASSERT_NO_FATAL_FAILURE(validateKbv(ResourceTemplates::davDispenseItemXml({
        .davPkvVersion = ResourceTemplates::Versions::DAV_PKV_1_4_0,
        .prescriptionId = model::PrescriptionId::fromDatabaseId(
            model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 4711'0815),
    })));
}
