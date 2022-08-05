/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "erp/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>
#include <filesystem>

using namespace fhirtools;

using namespace std::string_literals;
namespace
{
auto mkres(const std::filesystem::path& p)
{
    return ResourceManager::getAbsoluteFilename(p);
}
}
class ElementTest : public testing::Test
{
public:
    ElementTest()
    {
        auto fileList = {mkres("test/fhir-path/structure-definition.xml"), mkres("fhir/hl7.org/profiles-types.xml")};
        EXPECT_NO_THROW(repo.load(fileList));
    }
    FhirStructureRepository repo;
};

class ElementTaskTest : public ElementTest
{
public:
    ElementTaskTest()
    {
        EXPECT_NO_THROW(repo.load({mkres("fhir/hl7.org/profiles-resources.xml")}));
    }
};

TEST_F(ElementTest, TypeDetection)
{
    const auto* const testStructure = repo.findTypeById("Test");
    ASSERT_TRUE(testStructure);
    EXPECT_EQ(Element::GetElementType(&repo, testStructure, "Test"), Element::Type::Structured);
    EXPECT_EQ(Element::GetElementType(&repo, testStructure, "Test.string"), Element::Type::String);
    EXPECT_EQ(Element::GetElementType(&repo, testStructure, "Test.multiString"), Element::Type::String);
    EXPECT_EQ(Element::GetElementType(&repo, testStructure, "Test.num"), Element::Type::Integer);
    EXPECT_EQ(Element::GetElementType(&repo, testStructure, "Test.multiNum"), Element::Type::Integer);
    EXPECT_EQ(Element::GetElementType(&repo, testStructure, "Test.dec"), Element::Type::Decimal);
    EXPECT_EQ(Element::GetElementType(&repo, testStructure, "Test.multiDec"), Element::Type::Decimal);
    EXPECT_EQ(Element::GetElementType(&repo, testStructure, "Test.boolean"), Element::Type::Boolean);
    EXPECT_EQ(Element::GetElementType(&repo, testStructure, "Test.multibool"), Element::Type::Boolean);
    EXPECT_EQ(Element::GetElementType(&repo, testStructure, "Test.intAsString"), Element::Type::String);
    EXPECT_EQ(Element::GetElementType(&repo, testStructure, "Test.decimalAsString"), Element::Type::String);
    EXPECT_EQ(Element::GetElementType(&repo, testStructure, "Test.boolAsString"), Element::Type::String);
    EXPECT_EQ(Element::GetElementType(&repo, testStructure, "Test.date"), Element::Type::Date);
    EXPECT_EQ(Element::GetElementType(&repo, testStructure, "Test.dateTime"), Element::Type::DateTime);
    EXPECT_EQ(Element::GetElementType(&repo, testStructure, "Test.time"), Element::Type::Time);
    EXPECT_EQ(Element::GetElementType(&repo, testStructure, "Test.quantity"), Element::Type::Quantity);
}

TEST_F(ElementTaskTest, TypeDetectionTask)
{
    const auto* const hl7task = repo.findTypeById("Task");
    ASSERT_TRUE(hl7task);
    EXPECT_EQ(Element::GetElementType(&repo, hl7task, "Task"), Element::Type::Structured);
    EXPECT_EQ(Element::GetElementType(&repo, hl7task, "Task.id"), Element::Type::String);
    EXPECT_EQ(Element::GetElementType(&repo, hl7task, "Task.meta"), Element::Type::Structured);
    //    EXPECT_EQ(Element::GetElementType(&repo, hl7task, "Task.meta.id"), Element::Type::String);
    EXPECT_EQ(Element::GetElementType(&repo, hl7task, "Task.implicitRules"), Element::Type::String);
    EXPECT_EQ(Element::GetElementType(&repo, hl7task, "Task.language"), Element::Type::String);
    EXPECT_EQ(Element::GetElementType(&repo, hl7task, "Task.text"), Element::Type::Structured);
    EXPECT_EQ(Element::GetElementType(&repo, hl7task, "Task.contained"), Element::Type::Structured);
    EXPECT_EQ(Element::GetElementType(&repo, hl7task, "Task.extension"), Element::Type::Structured);
    EXPECT_EQ(Element::GetElementType(&repo, hl7task, "Task.implicitRules"), Element::Type::String);

    const auto* hl7Meta = repo.findTypeById("Meta");
    ASSERT_TRUE(hl7Meta);
    EXPECT_EQ(Element::GetElementType(&repo, hl7Meta, "Meta.id"), Element::Type::String);
    EXPECT_EQ(Element::GetElementType(&repo, hl7Meta, "Meta.lastUpdated"), Element::Type::DateTime);

    const auto* hl7instant = repo.findTypeById("instant");
    ASSERT_TRUE(hl7instant);
    EXPECT_EQ(Element::GetElementType(&repo, hl7instant, "instant.value"), Element::Type::DateTime);
}


TEST_F(ElementTaskTest, SubElements)
{
    const auto* const hl7task = repo.findTypeById("Task");
    ASSERT_TRUE(hl7task);
    const std::string task = R"--(
{
  "resourceType": "Task",
  "id": "160.000.000.004.711.86",
  "meta": {
    "id": "x",
    "versionId": "2",
    "lastUpdated": "2020-02-18T10:05:05.038+00:00",
    "source": "#AsYR9plLkvONJAiv",
    "profile": [
      "https://gematik.de/fhir/StructureDefinition/ErxTask"
    ]
  }
}
)--";
    auto taskResource = model::NumberAsStringParserDocument::fromJson(task);
    ErpElement taskElement(&repo, {}, hl7task, "Task", &taskResource);

    auto subElementNames = taskElement.subElementNames();
    std::vector<std::string> expectedElementNames = {"id", "meta"};
    EXPECT_EQ(subElementNames, expectedElementNames);

    auto metaSubElements = taskElement.subElements("meta");
    ASSERT_EQ(metaSubElements.size(), 1);
    auto metaSubElement = metaSubElements[0];
    ASSERT_EQ(metaSubElement->type(), Element::Type::Structured);

    auto idSubElements = metaSubElement->subElements("id");
    ASSERT_EQ(idSubElements.size(), 1);
    auto idSubElement = idSubElements[0];
    ASSERT_EQ(idSubElement->type(), Element::Type::String);
}

TEST_F(ElementTest, ConvertFromString)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));
    ErpElement erpElement(&repo, {}, repo.findTypeById("Test"), "Test.string",
                          rapidjson::Pointer("/string").Get(testResource));

    EXPECT_THROW((void) erpElement.asInt(), std::exception);
    EXPECT_THROW((void) erpElement.asDecimal(), std::exception);
    EXPECT_THROW((void) erpElement.asBool(), std::exception);
    EXPECT_EQ(erpElement.asString(), "value");
    EXPECT_THROW((void) erpElement.asDate(), std::exception);
    EXPECT_THROW((void) erpElement.asTime(), std::exception);
    EXPECT_THROW((void) erpElement.asDateTime(), std::exception);
    EXPECT_THROW((void) erpElement.asQuantity(), std::exception);
}

TEST_F(ElementTest, ConvertFromInt)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));
    ErpElement erpElement(&repo, {}, repo.findTypeById("Test"), "Test.num",
                          rapidjson::Pointer("/num").Get(testResource));

    EXPECT_EQ(erpElement.asInt(), 12);
    EXPECT_EQ(erpElement.asDecimal(), 12.0);
    EXPECT_THROW((void) erpElement.asBool(), std::exception);
    EXPECT_EQ(erpElement.asString(), "12");
    EXPECT_THROW((void) erpElement.asDate(), std::exception);
    EXPECT_THROW((void) erpElement.asTime(), std::exception);
    EXPECT_THROW((void) erpElement.asDateTime(), std::exception);
    EXPECT_EQ(erpElement.asQuantity(), Element::QuantityType(12, ""));
}

TEST_F(ElementTest, ConvertFromDecimal)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));
    ErpElement erpElement(&repo, {}, repo.findTypeById("Test"), "Test.dec",
                          rapidjson::Pointer("/dec").Get(testResource));

    EXPECT_THROW((void) erpElement.asInt(), std::exception);
    EXPECT_EQ(erpElement.asDecimal(), Element::DecimalType(1.2));
    EXPECT_THROW((void) erpElement.asBool(), std::exception);
    EXPECT_EQ(erpElement.asString(), "1.2");
    EXPECT_THROW((void) erpElement.asDate(), std::exception);
    EXPECT_THROW((void) erpElement.asTime(), std::exception);
    EXPECT_THROW((void) erpElement.asDateTime(), std::exception);
    EXPECT_EQ(erpElement.asQuantity(), Element::QuantityType(1.2, ""));
}

TEST_F(ElementTest, ConvertFromBoolean)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));
    ErpElement erpElement(&repo, {}, repo.findTypeById("Test"), "Test.boolean",
                          rapidjson::Pointer("/boolean").Get(testResource));

    EXPECT_EQ(erpElement.asInt(), 1);
    EXPECT_EQ(erpElement.asDecimal(), Element::DecimalType(1.0));
    EXPECT_EQ(erpElement.asBool(), true);
    EXPECT_EQ(erpElement.asString(), "true");
    EXPECT_THROW((void) erpElement.asDate(), std::exception);
    EXPECT_THROW((void) erpElement.asTime(), std::exception);
    EXPECT_THROW((void) erpElement.asDateTime(), std::exception);
    EXPECT_THROW((void) erpElement.asQuantity(), std::exception);
}

TEST_F(ElementTest, ConvertFromIntAsString)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));
    ErpElement erpElement(&repo, {}, repo.findTypeById("Test"), "Test.intAsString",
                          rapidjson::Pointer("/intAsString").Get(testResource));

    EXPECT_EQ(erpElement.asInt(), 32);
    EXPECT_EQ(erpElement.asDecimal(), Element::DecimalType(32.0));
    EXPECT_THROW((void) erpElement.asBool(), std::exception);
    EXPECT_EQ(erpElement.asString(), "32");
    EXPECT_THROW((void) erpElement.asDate(), std::exception);
    EXPECT_THROW((void) erpElement.asTime(), std::exception);
    EXPECT_THROW((void) erpElement.asDateTime(), std::exception);
    EXPECT_EQ(erpElement.asQuantity(), Element::QuantityType(32, ""));
}

TEST_F(ElementTest, ConvertFromDecimalAsString)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));
    ErpElement erpElement(&repo, {}, repo.findTypeById("Test"), "Test.decimalAsString",
                          rapidjson::Pointer("/decimalAsString").Get(testResource));

    EXPECT_THROW((void) erpElement.asInt(), std::exception);
    EXPECT_EQ(erpElement.asDecimal(), Element::DecimalType(3.14));
    EXPECT_THROW((void) erpElement.asBool(), std::exception);
    EXPECT_EQ(erpElement.asString(), "3.14");
    EXPECT_THROW((void) erpElement.asDate(), std::exception);
    EXPECT_THROW((void) erpElement.asTime(), std::exception);
    EXPECT_THROW((void) erpElement.asDateTime(), std::exception);
    EXPECT_EQ(erpElement.asQuantity(), Element::QuantityType(3.14, ""));
}

TEST_F(ElementTest, ConvertFromBoolAsString)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));
    ErpElement erpElement(&repo, {}, repo.findTypeById("Test"), "Test.boolAsString",
                          rapidjson::Pointer("/boolAsString").Get(testResource));

    EXPECT_THROW((void) erpElement.asInt(), std::exception);
    EXPECT_THROW((void) erpElement.asDecimal(), std::exception);
    EXPECT_EQ(erpElement.asBool(), true);
    EXPECT_EQ(erpElement.asString(), "T");
    EXPECT_THROW((void) erpElement.asDate(), std::exception);
    EXPECT_THROW((void) erpElement.asTime(), std::exception);
    EXPECT_THROW((void) erpElement.asDateTime(), std::exception);
    EXPECT_THROW((void) erpElement.asQuantity(), std::exception);
}

TEST_F(ElementTest, ConvertFromDate)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));
    ErpElement erpElement(&repo, {}, repo.findTypeById("Test"), "Test.date",
                          rapidjson::Pointer("/date").Get(testResource));

    EXPECT_THROW((void) erpElement.asInt(), std::exception);
    EXPECT_THROW((void) erpElement.asDecimal(), std::exception);
    EXPECT_THROW((void) erpElement.asBool(), std::exception);
    EXPECT_EQ(erpElement.asString(), "2022-04-29");
    EXPECT_EQ(erpElement.asDate(), Timestamp::fromXsDate("2022-04-29"));
    EXPECT_THROW((void) erpElement.asTime(), std::exception);
    EXPECT_EQ(erpElement.asDateTime(), Timestamp::fromXsDateTime("2022-04-29T00:00:00Z"));
    EXPECT_THROW((void) erpElement.asQuantity(), std::exception);
}

TEST_F(ElementTest, ConvertFromDateTime)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));
    ErpElement erpElement(&repo, {}, repo.findTypeById("Test"), "Test.dateTime",
                          rapidjson::Pointer("/dateTime").Get(testResource));

    EXPECT_THROW((void) erpElement.asInt(), std::exception);
    EXPECT_THROW((void) erpElement.asDecimal(), std::exception);
    EXPECT_THROW((void) erpElement.asBool(), std::exception);
    EXPECT_EQ(erpElement.asString(), "2022-04-29T11:47:30");
    EXPECT_EQ(erpElement.asDate(), Timestamp::fromXsDate("2022-04-29"));
    EXPECT_THROW((void) erpElement.asTime(), std::exception);
    EXPECT_EQ(erpElement.asDateTime(), Timestamp::fromXsDateTime("2022-04-29T11:47:30Z"));
    EXPECT_THROW((void) erpElement.asQuantity(), std::exception);
}

TEST_F(ElementTest, ConvertFromTime)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));
    ErpElement erpElement(&repo, {}, repo.findTypeById("Test"), "Test.time",
                          rapidjson::Pointer("/time").Get(testResource));

    EXPECT_THROW((void) erpElement.asInt(), std::exception);
    EXPECT_THROW((void) erpElement.asDecimal(), std::exception);
    EXPECT_THROW((void) erpElement.asBool(), std::exception);
    EXPECT_EQ(erpElement.asString(), "11:47:30");
    EXPECT_THROW((void) erpElement.asDate(), std::exception);
    EXPECT_EQ(erpElement.asTime(), Timestamp::fromXsTime("11:47:30"));
    EXPECT_THROW((void) erpElement.asDateTime(), std::exception);
    EXPECT_THROW((void) erpElement.asQuantity(), std::exception);
}

TEST_F(ElementTest, ConvertFromQuantity)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));
    ErpElement erpElement(&repo, {}, repo.findTypeById("Test"), "Test.quantity",
                          rapidjson::Pointer("/quantity").Get(testResource));

    EXPECT_THROW((void) erpElement.asInt(), std::exception);
    EXPECT_THROW((void) erpElement.asDecimal(), std::exception);
    EXPECT_THROW((void) erpElement.asBool(), std::exception);
    EXPECT_EQ(erpElement.asString(), "10.0 km");
    EXPECT_THROW((void) erpElement.asDate(), std::exception);
    EXPECT_THROW((void) erpElement.asTime(), std::exception);
    EXPECT_THROW((void) erpElement.asDateTime(), std::exception);
    EXPECT_EQ(erpElement.asQuantity(), Element::QuantityType(10, "km"));
}

TEST_F(ElementTest, QuantityArithmetik)
{
    EXPECT_TRUE(Element::QuantityType(1, "x") < Element::QuantityType(2, "x"));
    EXPECT_TRUE(Element::QuantityType(1, "x") <= Element::QuantityType(2, "x"));
    EXPECT_TRUE(Element::QuantityType(2, "x") == Element::QuantityType(2, "x"));
    EXPECT_TRUE(Element::QuantityType(2, "x") >= Element::QuantityType(2, "x"));
    EXPECT_TRUE(Element::QuantityType(3, "x") > Element::QuantityType(2, "x"));
    EXPECT_THROW((void)(Element::QuantityType(3, "x") > Element::QuantityType(2, "y")), std::exception);
}

TEST_F(ElementTest, RelationalOperators)
{
    PrimitiveElement elementInt3(&repo, Element::Type::Integer, 3);
    PrimitiveElement elementInt4(&repo, Element::Type::Integer, 4);
    PrimitiveElement elementDecPi(&repo, Element::Type::Decimal, Element::DecimalType(3.1415));
    PrimitiveElement elementDec3(&repo, Element::Type::Decimal, Element::DecimalType(3));
    PrimitiveElement elementStrA(&repo, Element::Type::String, "abc"s);
    PrimitiveElement elementStrB(&repo, Element::Type::String, "ABC"s);
    PrimitiveElement elementQuantitiy1(&repo, Element::Type::Quantity, Element::QuantityType(15, "cm"));
    PrimitiveElement elementQuantitiy2(&repo, Element::Type::Quantity, Element::QuantityType(16, "cm"));
    PrimitiveElement elementQuantitiy3(&repo, Element::Type::Quantity, Element::QuantityType(3, ""));

    EXPECT_TRUE(elementInt3 < elementInt4);
    EXPECT_TRUE(elementInt3 < elementDecPi);
    EXPECT_FALSE(elementInt3 < elementDec3);
    EXPECT_THROW((void) (elementInt3 < elementStrA), std::exception);
    EXPECT_THROW((void) (elementInt3 < elementQuantitiy1), std::exception);
    EXPECT_FALSE(elementInt3 < elementQuantitiy3);

    EXPECT_TRUE(elementInt3 <= elementInt4);
    EXPECT_TRUE(elementInt3 <= elementDecPi);
    EXPECT_TRUE(elementInt3 <= elementDec3);
    EXPECT_THROW((void) (elementInt3 <= elementStrA), std::exception);
    EXPECT_THROW((void) (elementInt3 <= elementQuantitiy1), std::exception);
    EXPECT_TRUE(elementInt3 <= elementQuantitiy3);

    EXPECT_FALSE(elementInt3 == elementInt4);
    EXPECT_FALSE(elementInt3 == elementDecPi);
    EXPECT_TRUE(elementInt3 == elementDec3);
    EXPECT_FALSE(elementInt3 == elementStrA);
    EXPECT_FALSE(elementInt3 == elementQuantitiy1);
    EXPECT_TRUE(elementInt3 == elementQuantitiy3);

    EXPECT_FALSE(elementInt3 >= elementInt4);
    EXPECT_FALSE(elementInt3 >= elementDecPi);
    EXPECT_TRUE(elementInt3 >= elementDec3);
    EXPECT_THROW((void) (elementInt3 >= elementStrA), std::exception);
    EXPECT_THROW((void) (elementInt3 >= elementQuantitiy1), std::exception);
    EXPECT_TRUE(elementInt3 >= elementQuantitiy3);

    EXPECT_FALSE(elementInt3 > elementInt4);
    EXPECT_FALSE(elementInt3 > elementDecPi);
    EXPECT_FALSE(elementInt3 > elementDec3);
    EXPECT_THROW((void) (elementInt3 > elementStrA), std::exception);
    EXPECT_THROW((void) (elementInt3 > elementQuantitiy1), std::exception);
    EXPECT_FALSE(elementInt3 > elementQuantitiy3);

    EXPECT_THROW((void) (elementDecPi < elementInt4), std::exception);
    EXPECT_FALSE(elementDecPi < elementDec3);
    EXPECT_THROW((void) (elementDecPi < elementStrA), std::exception);
    EXPECT_THROW((void) (elementDecPi < elementQuantitiy1), std::exception);
    EXPECT_FALSE(elementDecPi < elementQuantitiy3);

    EXPECT_THROW((void) (elementDecPi <= elementInt4), std::exception);
    EXPECT_FALSE(elementDecPi <= elementDec3);
    EXPECT_THROW((void) (elementDecPi <= elementStrA), std::exception);
    EXPECT_THROW((void) (elementDecPi <= elementQuantitiy1), std::exception);
    EXPECT_FALSE(elementDecPi <= elementQuantitiy3);

    EXPECT_FALSE(elementDecPi == elementInt4);
    EXPECT_FALSE(elementDecPi == elementDec3);
    EXPECT_FALSE(elementDecPi == elementStrA);
    EXPECT_FALSE(elementDecPi == elementQuantitiy1);
    EXPECT_FALSE(elementDecPi == elementQuantitiy3);

    EXPECT_THROW((void) (elementDecPi >= elementInt4), std::exception);
    EXPECT_TRUE(elementDecPi >= elementDec3);
    EXPECT_THROW((void) (elementDecPi >= elementStrA), std::exception);
    EXPECT_THROW((void) (elementDecPi >= elementQuantitiy1), std::exception);
    EXPECT_TRUE(elementDecPi >= elementQuantitiy3);

    EXPECT_THROW((void) (elementDecPi > elementInt4), std::exception);
    EXPECT_TRUE(elementDecPi > elementDec3);
    EXPECT_THROW((void) (elementDecPi > elementStrA), std::exception);
    EXPECT_THROW((void) (elementDecPi > elementQuantitiy1), std::exception);
    EXPECT_TRUE(elementDecPi > elementQuantitiy3);


    EXPECT_THROW((void) (elementStrA < elementInt4), std::exception);
    EXPECT_THROW((void) (elementStrA < elementDecPi), std::exception);
    EXPECT_FALSE(elementStrA < elementStrB);
    EXPECT_THROW((void) (elementStrA < elementQuantitiy1), std::exception);

    EXPECT_FALSE(elementStrA <= elementStrB);
    EXPECT_FALSE(elementStrA == elementStrB);
    EXPECT_TRUE(elementStrA >= elementStrB);
    EXPECT_TRUE(elementStrA > elementStrB);


    EXPECT_THROW((void) (elementQuantitiy1 < elementInt4), std::exception);
    EXPECT_THROW((void) (elementQuantitiy1 < elementDec3), std::exception);
    EXPECT_THROW((void) (elementQuantitiy1 < elementStrB), std::exception);
    EXPECT_THROW((void) (elementQuantitiy1 < elementQuantitiy3), std::exception);
    EXPECT_TRUE(elementQuantitiy1 < elementQuantitiy2);
    EXPECT_TRUE(elementQuantitiy1 <= elementQuantitiy2);
    EXPECT_FALSE(elementQuantitiy1 == elementQuantitiy2);
    EXPECT_FALSE(elementQuantitiy1 >= elementQuantitiy2);
    EXPECT_FALSE(elementQuantitiy1 > elementQuantitiy2);
}
