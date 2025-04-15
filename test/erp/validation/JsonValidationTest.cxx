/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Communication.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "erp/model/Task.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/Binary.hxx"
#include "shared/util/Base64.hxx"
#include "shared/validation/JsonValidator.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <gtest/gtest.h>


class JsonValidationTest : public testing::Test
{
public:
    static std::shared_ptr<JsonValidator> getJsonValidator()
    {
        return StaticData::getJsonValidator();
    }

    static model::Task validTask()
    {
        model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel,
                         "a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1");
        task.setPrescriptionId(
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1));
        return task;
    }
};


TEST_F(JsonValidationTest, ValidateDefaultTask)
{
    rapidjson::Document jsonDocument =
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(validTask().jsonDocument());
    ASSERT_NO_THROW(getJsonValidator()->validate(jsonDocument, SchemaType::fhir));
}

TEST_F(JsonValidationTest, EmptyDocument)
{
    rapidjson::Document jsonDocument;
    jsonDocument.Parse(R"({})");
    ASSERT_ANY_THROW(getJsonValidator()->validate(jsonDocument, SchemaType::fhir));
}

TEST_F(JsonValidationTest, TaskUnallowedAdditionalProperty)
{
    model::Task task = validTask();
    rapidjson::Document jsonDocument =
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(task.jsonDocument());
    rapidjson::Pointer pointer("/unallowed_property");
    pointer.Set(jsonDocument, "unallowed_property");

    ASSERT_ANY_THROW(getJsonValidator()->validate(jsonDocument, SchemaType::fhir));
}

TEST_F(JsonValidationTest, Erp8881CommunicationExtensionUrl)
{
    testutils::ShiftFhirResourceViewsGuard unshift{testutils::ShiftFhirResourceViewsGuard::asConfigured};
    const auto resource = ResourceManager::instance().getStringResource(
        "test/issues/ERP-8881/Malformed_URL_in_Communication_Extension.json");
    const auto& fhirInstance = Fhir::instance();
    const auto& backend = fhirInstance.backend();
    const auto viewList = fhirInstance.structureRepository(model::Timestamp::fromGermanDate("2024-10-01"));
    const auto& view =
        viewList.match(&backend, std::string{model::resource::structure_definition::communicationReply},
                       ResourceTemplates::Versions::GEM_ERP_1_2);
    ASSERT_NE(view, nullptr);
    std::optional<model::Bundle> communication;
    EXPECT_NO_THROW(communication = model::Bundle::fromJsonNoValidation(resource));
    ASSERT_TRUE(communication.has_value());
    const auto validationResults = communication->genericValidate(model::ProfileType::fhir, {}, view);
    validationResults.dumpToLog();
    EXPECT_EQ(validationResults.highestSeverity(), fhirtools::Severity::error);

    std::string expected = "Bundle.entry[0].resource{Communication}.payload[0].extension[0]: error: element doesn't "
                           "match any slice in closed slicing (from profile: "
                           "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_Reply|1.2); ";
    EXPECT_EQ(expected, validationResults.summary(fhirtools::Severity::error));
}
