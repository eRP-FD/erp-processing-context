#include "shared/model/GemErpPrMedication.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>


class GemErpPrMedicationTest : public testing::TestWithParam<std::string>
{
protected:
    class TestMedication : public model::GemErpPrMedication
    {
    public:
        TestMedication(GemErpPrMedication&& super) : GemErpPrMedication{std::move(super)}{}
        std::optional<model::Timestamp> getValidationReferenceTimestamp() const override
        {
            return model::Timestamp::now();
        }
    };


};

TEST_P(GemErpPrMedicationTest, validateTemplate)
{
    auto medicationXml = ResourceTemplates::medicationXml({
        .version = ResourceTemplates::Versions::GEM_ERP_current(),
        .templatePrefix = GetParam(),
    });
    using TestMedicationFactory = model::ResourceFactory<TestMedication>;
    std::optional<TestMedicationFactory> testMedicationFactory;
    ASSERT_NO_THROW(
        testMedicationFactory.emplace(TestMedicationFactory::fromXml(medicationXml, *StaticData::getXmlValidator())))
        << medicationXml;
    ASSERT_NO_THROW(
        (void) std::move(testMedicationFactory).value().getValidated(model::ProfileType::GEM_ERP_PR_Medication))
        << medicationXml;
}

INSTANTIATE_TEST_SUITE_P(medicationTypes, GemErpPrMedicationTest,
                         ::testing::Values(ResourceTemplates::MedicationOptions::PZN,
                                           ResourceTemplates::MedicationOptions::COMPOUNDING,
                                           ResourceTemplates::MedicationOptions::INGREDIENT,
                                           ResourceTemplates::MedicationOptions::FREETEXT));
