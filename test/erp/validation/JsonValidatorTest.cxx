/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/validation/JsonValidator.hxx"
#include "test/util/StaticData.hxx"

#include <gtest/gtest.h>

#include "erp/util/Base64.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/model/Task.hxx"


class JsonValidatorTest : public testing::Test
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


TEST_F(JsonValidatorTest, ValidateDefaultTask)
{
    rapidjson::Document jsonDocument =
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(validTask().jsonDocument());
    ASSERT_NO_THROW(getJsonValidator()->validate(jsonDocument, SchemaType::Gem_erxTask));
}

TEST_F(JsonValidatorTest, EmptyDocument)
{
    rapidjson::Document jsonDocument;
    jsonDocument.Parse(R"({})");
    ASSERT_ANY_THROW(getJsonValidator()->validate(jsonDocument, SchemaType::Gem_erxTask));
}

TEST_F(JsonValidatorTest, TaskUnallowedAdditionalProperty)
{
    model::Task task = validTask();
    rapidjson::Document jsonDocument =
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(task.jsonDocument());
    rapidjson::Pointer pointer("/unallowed_property");
    pointer.Set(jsonDocument, "unallowed_property");

    ASSERT_ANY_THROW(getJsonValidator()->validate(jsonDocument, SchemaType::Gem_erxTask));
}

TEST_F(JsonValidatorTest, TaskInvalidPrescriptionId)
{
    model::Task task = validTask();
    rapidjson::Document jsonDocument =
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(task.jsonDocument());
    rapidjson::Pointer prescriptionIdPointer("/identifier/0/value");
    prescriptionIdPointer.Set(jsonDocument, "invalid_id");

    // now the validation must fail.
    ASSERT_ANY_THROW(getJsonValidator()->validate(jsonDocument, SchemaType::Gem_erxTask));
}

TEST_F(JsonValidatorTest, DefaultBinary)
{
    model::Binary binary("id", "data");
    rapidjson::Document jsonDocument =
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(binary.jsonDocument());
    ASSERT_NO_THROW(getJsonValidator()->validate(jsonDocument, SchemaType::Gem_erxBinary));
}

TEST_F(JsonValidatorTest, BinaryInvalidData)
{
    model::Binary binary("id", "data");

    rapidjson::Document jsonDocument =
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(binary.jsonDocument());
    rapidjson::Pointer pointer("/data");
    pointer.Set(jsonDocument, 42);

    ASSERT_ANY_THROW(getJsonValidator()->validate(jsonDocument, SchemaType::Gem_erxBinary));
}
