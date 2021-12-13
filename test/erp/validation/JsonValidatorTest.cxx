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
    ASSERT_NO_THROW(getJsonValidator()->validate(jsonDocument, SchemaType::fhir));
}

TEST_F(JsonValidatorTest, EmptyDocument)
{
    rapidjson::Document jsonDocument;
    jsonDocument.Parse(R"({})");
    ASSERT_ANY_THROW(getJsonValidator()->validate(jsonDocument, SchemaType::fhir));
}

TEST_F(JsonValidatorTest, TaskUnallowedAdditionalProperty)
{
    model::Task task = validTask();
    rapidjson::Document jsonDocument =
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(task.jsonDocument());
    rapidjson::Pointer pointer("/unallowed_property");
    pointer.Set(jsonDocument, "unallowed_property");

    ASSERT_ANY_THROW(getJsonValidator()->validate(jsonDocument, SchemaType::fhir));
}
