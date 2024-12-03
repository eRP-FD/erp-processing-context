#include <gtest/gtest.h>

#include "erp/model/GemErpPrMedication.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/MedicationDispenseId.hxx"
#include "erp/model/MedicationDispenseOperationParameters.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"

namespace model
{
inline namespace
{
void PrintTo(const model::ProfileType& profileType, std::ostream* out)
{
    (*out) << magic_enum::enum_name(profileType);
}
void PrintTo(const model::MedicationDispenseId& medicationDispenseId, std::ostream* out)
{
    (*out) << medicationDispenseId.toString();
}
}
}

class MedicationDispenseOperationParametersProfileTypeTest : public testing::TestWithParam<model::ProfileType>
{
    void SetUp() override
    {
        const auto& fhirInstance = Fhir::instance();
        const auto view = fhirInstance.structureRepository(model::Timestamp::now());
        gsl::not_null backend = std::addressof(fhirInstance.backend());
        auto vers = view.supportedVersions(backend, {std::string{model::resource::structure_definition::closeOperationInput}});
        if (vers.empty())
        {
            GTEST_SKIP_("MedicationDispenseOperationParameters not supported.");
        }
    }
};

TEST_P(MedicationDispenseOperationParametersProfileTypeTest, profileType)
{
    auto closeParametersXml = ResourceTemplates::medicationDispenseOperationParametersXml({.profileType = GetParam()});

    std::optional<model::MedicationDispenseOperationParameters> parameters;
    ASSERT_NO_THROW(parameters.emplace(
        model::MedicationDispenseOperationParameters::fromXml(closeParametersXml, *StaticData::getXmlValidator())))
        << closeParametersXml;
    EXPECT_EQ(parameters.value().getProfile(), GetParam());
}

INSTANTIATE_TEST_SUITE_P(types, MedicationDispenseOperationParametersProfileTypeTest,
                         ::testing::Values(model::ProfileType::GEM_ERP_PR_PAR_CloseOperation_Input,
                                           model::ProfileType::GEM_ERP_PR_PAR_DispenseOperation_Input));



TEST(MedicationDispenseOperationParametersTest, medicationDispenses)
{
    using namespace std::string_literals;
    auto id = model::PrescriptionId::fromString("160.555.555.555.555.40");
    auto parameters = model::MedicationDispenseOperationParameters::fromXmlNoValidation(
        ResourceTemplates::medicationDispenseOperationParametersXml({.medicationDispenses = {
            {.id = model::MedicationDispenseId{id, 0}, .medication = ResourceTemplates::MedicationOptions{.id ="A"}},
            {.id = model::MedicationDispenseId{id, 1}, .medication = ResourceTemplates::MedicationOptions{.id ="B"}},
            {.id = model::MedicationDispenseId{id, 2}, .medication = ResourceTemplates::MedicationOptions{.id ="C"}},
            {.id = model::MedicationDispenseId{id, 3}, .medication = ResourceTemplates::MedicationOptions{.id ="D"}},
        }}));
    auto dispenses = parameters.medicationDispenses();
    ASSERT_EQ(dispenses.size(), 4);
    auto d = dispenses.begin();
    EXPECT_EQ(get<model::MedicationDispense>(*d).id(), (model::MedicationDispenseId{id, 0}));
    EXPECT_EQ(get<model::GemErpPrMedication>(*d).getId(), "A");
    ++d;
    EXPECT_EQ(get<model::MedicationDispense>(*d).id(), (model::MedicationDispenseId{id, 1})) << parameters.serializeToJsonString();
    EXPECT_EQ(get<model::GemErpPrMedication>(*d).getId(), "B");
    std::ranges::advance(d, 1, dispenses.end());
    EXPECT_EQ(get<model::MedicationDispense>(*d).id(), (model::MedicationDispenseId{id, 2}));
    EXPECT_EQ(get<model::GemErpPrMedication>(*d).getId(), "C");
    std::ranges::advance(d, 1, dispenses.end());
    EXPECT_EQ(get<model::MedicationDispense>(*d).id(), (model::MedicationDispenseId{id, 3}));
    EXPECT_EQ(get<model::GemErpPrMedication>(*d).getId(), "D");
}
