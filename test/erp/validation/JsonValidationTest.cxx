/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Binary.hxx"
#include "erp/model/Communication.hxx"
#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/model/Task.hxx"
#include "erp/util/Base64.hxx"
#include "erp/validation/JsonValidator.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/StaticData.hxx"

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
    const auto resource = ResourceManager::instance().getStringResource(
        "test/issues/ERP-8881/Malformed_URL_in_Communication_Extension.json");
    std::optional<model::Bundle> communication;
    EXPECT_NO_THROW(communication = model::Bundle::fromJsonNoValidation(resource));
    ASSERT_TRUE(communication.has_value());
    const auto validationResults =
        communication->genericValidate(model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01, {});
    validationResults.dumpToLog();
    EXPECT_EQ(validationResults.highestSeverity(), fhirtools::Severity::error);

    std::string expected = "Bundle.entry[0].resource{Communication}.payload[0].extension[0]: error: element doesn't "
                           "match any slice in closed slicing (from profile: "
                           "https://gematik.de/fhir/erp/StructureDefinition/GEM_ERP_PR_Communication_Reply|1.2); ";
    EXPECT_EQ(expected, validationResults.summary(fhirtools::Severity::error));
}
