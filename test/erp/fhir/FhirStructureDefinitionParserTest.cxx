/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include <gtest/gtest.h>

#include "erp/fhir/internal/FhirStructureDefinitionParser.hxx"
#include "test/util/ResourceManager.hxx"
#include "test_config.h"

TEST(FhirStructureDefinitionParserTest, TestPrimitives) //NOLINT
{
    using namespace std::string_view_literals;
    using Kind = FhirStructureDefinition::Kind;
    using Derivation = FhirStructureDefinition::Derivation;
    using Representation = FhirElement::Representation;

    const auto& fhirResourcesXml = ResourceManager::getAbsoluteFilename("test/fhir/TestPrimitives.xml");
    auto types = FhirStructureDefinitionParser::parse(fhirResourcesXml);
    ASSERT_EQ(types.size(), 2);
    const FhirStructureDefinition* stringDef = nullptr;
    const FhirStructureDefinition* integerDef = nullptr;
    for (const auto& type: types)
    {
        if (type.typeId() == "string")
        {
            stringDef = &type;
        }
        else if (type.typeId() == "integer")
        {
            integerDef = &type;
        }
    }
    ASSERT_TRUE(stringDef != nullptr);
    EXPECT_EQ(stringDef->url(), "http://hl7.org/fhir/StructureDefinition/string"sv);
    EXPECT_EQ(stringDef->kind(), Kind::primitiveType);
    EXPECT_EQ(stringDef->baseDefinition(), "http://hl7.org/fhir/StructureDefinition/Element"sv);
    EXPECT_EQ(stringDef->derivation(), Derivation::specialization);

    const auto stringBaseElement = stringDef->findElement("string");
    ASSERT_TRUE(stringBaseElement);
    EXPECT_EQ(stringBaseElement->typeId(), "string"sv);
    EXPECT_EQ(stringBaseElement->name(), "string"sv);
    EXPECT_TRUE(stringBaseElement->isRoot());
    EXPECT_TRUE(stringBaseElement->isArray());

    const auto stringValueElement = stringDef->findElement("string.value");
    ASSERT_TRUE(stringValueElement);
    EXPECT_EQ(stringValueElement->typeId(), "http://hl7.org/fhirpath/System.String"sv);
    EXPECT_EQ(stringValueElement->name(), "string.value"sv);
    EXPECT_EQ(stringValueElement->representation(), Representation::xmlAttr);
    EXPECT_FALSE(stringValueElement->isRoot());
    EXPECT_FALSE(stringValueElement->isArray());


    ASSERT_TRUE(integerDef != nullptr);
    EXPECT_EQ(integerDef->url(), "http://hl7.org/fhir/StructureDefinition/integer"sv);
    EXPECT_EQ(integerDef->kind(), Kind::primitiveType);
    EXPECT_EQ(integerDef->baseDefinition(), "http://hl7.org/fhir/StructureDefinition/Element"sv);
    EXPECT_EQ(integerDef->derivation(), Derivation::specialization);

    const auto integerBaseElement = integerDef->findElement("integer");
    ASSERT_TRUE(integerBaseElement);
    EXPECT_EQ(integerBaseElement->typeId(), "integer"sv);
    EXPECT_EQ(integerBaseElement->name(), "integer"sv);
    EXPECT_TRUE(integerBaseElement->isRoot());
    EXPECT_TRUE(integerBaseElement->isArray());

    const auto integerValueElement = integerDef->findElement("integer.value");
    ASSERT_TRUE(integerValueElement);
    EXPECT_EQ(integerValueElement->typeId(), "http://hl7.org/fhirpath/System.Integer"sv);
    EXPECT_EQ(integerValueElement->name(), "integer.value"sv);
    EXPECT_EQ(integerValueElement->representation(), Representation::xmlAttr);
    EXPECT_FALSE(integerValueElement->isRoot());
    EXPECT_FALSE(integerValueElement->isArray());
}


TEST(FhirStructureDefinitionParserTest, InvalidXml) //NOLINT
{
    const auto& fhirResourcesXml = ResourceManager::getAbsoluteFilename("test/fhir/InvalidStructureDefinition.xml");
    ASSERT_ANY_THROW(FhirStructureDefinitionParser::parse(fhirResourcesXml));
}
