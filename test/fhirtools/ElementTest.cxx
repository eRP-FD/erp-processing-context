/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "test/fhirtools/DefaultFhirStructureRepository.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>
#include <filesystem>

using namespace fhirtools;

using namespace std::string_literals;
class ElementTest : public testing::Test
{
public:
    std::shared_ptr<const fhirtools::FhirStructureRepository> mRepo = DefaultFhirStructureRepository::getWithTest();
    const FhirStructureRepository& repo = *mRepo;
};

class ElementTaskTest : public testing::Test
{
public:
    std::shared_ptr<const fhirtools::FhirStructureRepository> mRepo = DefaultFhirStructureRepository::getWithTest();
    const FhirStructureRepository& repo = *mRepo;
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
    ErpElement taskElement(mRepo, {}, hl7task, "Task", &taskResource);

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
    ErpElement erpElement(mRepo, {}, repo.findTypeById("Test"), "Test.string",
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
    ErpElement erpElement(mRepo, {}, repo.findTypeById("Test"), "Test.num",
                          rapidjson::Pointer("/num").Get(testResource));

    EXPECT_EQ(erpElement.asInt(), 12);
    EXPECT_EQ(erpElement.asDecimal(), DecimalType("12.0"));
    EXPECT_THROW((void) erpElement.asBool(), std::exception);
    EXPECT_EQ(erpElement.asString(), "12");
    EXPECT_THROW((void) erpElement.asDate(), std::exception);
    EXPECT_THROW((void) erpElement.asTime(), std::exception);
    EXPECT_THROW((void) erpElement.asDateTime(), std::exception);
    EXPECT_EQ(erpElement.asQuantity().compareTo(Element::QuantityType(12, "")), std::strong_ordering::equal);
}

TEST_F(ElementTest, ConvertFromDecimal)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));
    ErpElement erpElement(mRepo, {}, repo.findTypeById("Test"), "Test.dec",
                          rapidjson::Pointer("/dec").Get(testResource));

    EXPECT_THROW((void) erpElement.asInt(), std::exception);
    EXPECT_EQ(erpElement.asDecimal(), DecimalType("1.2"));
    EXPECT_THROW((void) erpElement.asBool(), std::exception);
    EXPECT_EQ(erpElement.asString(), "1.2");
    EXPECT_THROW((void) erpElement.asDate(), std::exception);
    EXPECT_THROW((void) erpElement.asTime(), std::exception);
    EXPECT_THROW((void) erpElement.asDateTime(), std::exception);
    EXPECT_EQ(erpElement.asQuantity().compareTo(Element::QuantityType(DecimalType("1.2"), "")),
              std::strong_ordering::equal);
}

TEST_F(ElementTest, ConvertFromBoolean)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));
    ErpElement erpElement(mRepo, {}, repo.findTypeById("Test"), "Test.boolean",
                          rapidjson::Pointer("/boolean").Get(testResource));

    EXPECT_EQ(erpElement.asInt(), 1);
    EXPECT_EQ(erpElement.asDecimal(), DecimalType("1.0"));
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
    ErpElement erpElement(mRepo, {}, repo.findTypeById("Test"), "Test.intAsString",
                          rapidjson::Pointer("/intAsString").Get(testResource));

    EXPECT_EQ(erpElement.asInt(), 32);
    EXPECT_EQ(erpElement.asDecimal(), DecimalType("32.0"));
    EXPECT_THROW((void) erpElement.asBool(), std::exception);
    EXPECT_EQ(erpElement.asString(), "32");
    EXPECT_THROW((void) erpElement.asDate(), std::exception);
    EXPECT_THROW((void) erpElement.asTime(), std::exception);
    EXPECT_THROW((void) erpElement.asDateTime(), std::exception);
    EXPECT_EQ(erpElement.asQuantity().compareTo(Element::QuantityType(32, "")), std::strong_ordering::equal);
}

TEST_F(ElementTest, ConvertFromDecimalAsString)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));
    ErpElement erpElement(mRepo, {}, repo.findTypeById("Test"), "Test.decimalAsString",
                          rapidjson::Pointer("/decimalAsString").Get(testResource));

    EXPECT_THROW((void) erpElement.asInt(), std::exception);
    EXPECT_EQ(erpElement.asDecimal(), DecimalType("3.14"));
    EXPECT_THROW((void) erpElement.asBool(), std::exception);
    EXPECT_EQ(erpElement.asString(), "3.14");
    EXPECT_THROW((void) erpElement.asDate(), std::exception);
    EXPECT_THROW((void) erpElement.asTime(), std::exception);
    EXPECT_THROW((void) erpElement.asDateTime(), std::exception);
    EXPECT_EQ(erpElement.asQuantity().compareTo(Element::QuantityType(DecimalType("3.14"), "")),
              std::strong_ordering::equal);
}

TEST_F(ElementTest, ConvertFromBoolAsString)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));
    ErpElement erpElement(mRepo, {}, repo.findTypeById("Test"), "Test.boolAsString",
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
    ErpElement erpElement(mRepo, {}, repo.findTypeById("Test"), "Test.date",
                          rapidjson::Pointer("/date").Get(testResource));

    EXPECT_THROW((void) erpElement.asInt(), std::exception);
    EXPECT_THROW((void) erpElement.asDecimal(), std::exception);
    EXPECT_THROW((void) erpElement.asBool(), std::exception);
    EXPECT_EQ(erpElement.asString(), "2022-04-29");
    EXPECT_EQ(erpElement.asDate().compareTo(Date("2022-04-29")), std::strong_ordering::equal);
    EXPECT_THROW((void) erpElement.asTime(), std::exception);
    EXPECT_EQ(erpElement.asDateTime().compareTo(DateTime("2022-04-29T")), std::strong_ordering::equal);
    EXPECT_THROW((void) erpElement.asQuantity(), std::exception);
}

TEST_F(ElementTest, ConvertFromDateTime)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));
    ErpElement erpElement(mRepo, {}, repo.findTypeById("Test"), "Test.dateTime",
                          rapidjson::Pointer("/dateTime").Get(testResource));

    EXPECT_THROW((void) erpElement.asInt(), std::exception);
    EXPECT_THROW((void) erpElement.asDecimal(), std::exception);
    EXPECT_THROW((void) erpElement.asBool(), std::exception);
    EXPECT_EQ(erpElement.asString(), "2022-04-29T11:47:30");
    EXPECT_EQ(erpElement.asDate().compareTo(Date("2022-04-29")), std::strong_ordering::equal);
    EXPECT_THROW((void) erpElement.asTime(), std::exception);
    EXPECT_EQ(erpElement.asDateTime().compareTo(DateTime("2022-04-29T11:47:30")), std::strong_ordering::equal);
    EXPECT_THROW((void) erpElement.asQuantity(), std::exception);
}

TEST_F(ElementTest, ConvertFromTime)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));
    ErpElement erpElement(mRepo, {}, repo.findTypeById("Test"), "Test.time",
                          rapidjson::Pointer("/time").Get(testResource));

    EXPECT_THROW((void) erpElement.asInt(), std::exception);
    EXPECT_THROW((void) erpElement.asDecimal(), std::exception);
    EXPECT_THROW((void) erpElement.asBool(), std::exception);
    EXPECT_EQ(erpElement.asString(), "11:47:30");
    EXPECT_THROW((void) erpElement.asDate(), std::exception);
    EXPECT_EQ(erpElement.asTime().compareTo(Time("11:47:30")), std::strong_ordering::equal);
    EXPECT_THROW((void) erpElement.asDateTime(), std::exception);
    EXPECT_THROW((void) erpElement.asQuantity(), std::exception);
}

TEST_F(ElementTest, ConvertFromQuantity)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));
    ErpElement erpElement(mRepo, {}, repo.findTypeById("Test"), "Test.quantity",
                          rapidjson::Pointer("/quantity").Get(testResource));

    EXPECT_THROW((void) erpElement.asInt(), std::exception);
    EXPECT_THROW((void) erpElement.asDecimal(), std::exception);
    EXPECT_THROW((void) erpElement.asBool(), std::exception);
    EXPECT_EQ(erpElement.asString(), "10.0 km");
    EXPECT_THROW((void) erpElement.asDate(), std::exception);
    EXPECT_THROW((void) erpElement.asTime(), std::exception);
    EXPECT_THROW((void) erpElement.asDateTime(), std::exception);
    EXPECT_EQ(erpElement.asQuantity().compareTo(Element::QuantityType(10, "km")), std::strong_ordering::equal);
}

TEST_F(ElementTest, QuantityArithmetik)
{
    EXPECT_EQ(Element::QuantityType(1, "x").compareTo(Element::QuantityType(2, "x")), std::strong_ordering::less);
    EXPECT_EQ(Element::QuantityType(2, "x").compareTo(Element::QuantityType(2, "x")), std::strong_ordering::equal);
    EXPECT_EQ(Element::QuantityType(3, "x").compareTo(Element::QuantityType(2, "x")), std::strong_ordering::greater);
    EXPECT_EQ(Element::QuantityType(3, "x").compareTo(Element::QuantityType(2, "y")), std::nullopt);
}

TEST_F(ElementTest, CompareTo)
{
    const PrimitiveElement elementInt3(mRepo, Element::Type::Integer, 3);
    const PrimitiveElement elementInt4(mRepo, Element::Type::Integer, 4);
    const PrimitiveElement elementDecPi(mRepo, Element::Type::Decimal, DecimalType("3.1415"));
    const PrimitiveElement elementDec3(mRepo, Element::Type::Decimal, DecimalType(3));
    const PrimitiveElement elementStrA(mRepo, Element::Type::String, "abc"s);
    const PrimitiveElement elementStrB(mRepo, Element::Type::String, "ABC"s);
    const PrimitiveElement elementQuantitiy1(mRepo, Element::Type::Quantity, Element::QuantityType(15, "cm"));
    const PrimitiveElement elementQuantitiy2(mRepo, Element::Type::Quantity, Element::QuantityType(16, "cm"));
    const PrimitiveElement elementQuantitiy3(mRepo, Element::Type::Quantity, Element::QuantityType(3, ""));

    EXPECT_EQ(elementInt3.compareTo(elementInt3), std::strong_ordering::equal);
    EXPECT_EQ(elementInt3.compareTo(elementInt4), std::strong_ordering::less);
    EXPECT_EQ(elementInt3.compareTo(elementDecPi), std::strong_ordering::less);
    EXPECT_EQ(elementInt3.compareTo(elementDec3), std::strong_ordering::equal);
    EXPECT_EQ(elementInt3.compareTo(elementStrA), std::strong_ordering::less);
    EXPECT_EQ(elementInt3.compareTo(elementStrB), std::strong_ordering::less);
    EXPECT_EQ(elementInt3.compareTo(elementQuantitiy1), std::nullopt);
    EXPECT_EQ(elementInt3.compareTo(elementQuantitiy2), std::nullopt);
    EXPECT_EQ(elementInt3.compareTo(elementQuantitiy3), std::strong_ordering::equal);

    EXPECT_EQ(elementInt4.compareTo(elementInt3), std::strong_ordering::greater);
    EXPECT_EQ(elementInt4.compareTo(elementInt4), std::strong_ordering::equal);
    EXPECT_EQ(elementInt4.compareTo(elementDecPi), std::strong_ordering::greater);
    EXPECT_EQ(elementInt4.compareTo(elementDec3), std::strong_ordering::greater);
    EXPECT_EQ(elementInt4.compareTo(elementStrA), std::strong_ordering::less);
    EXPECT_EQ(elementInt4.compareTo(elementStrB), std::strong_ordering::less);
    EXPECT_EQ(elementInt4.compareTo(elementQuantitiy1), std::nullopt);
    EXPECT_EQ(elementInt4.compareTo(elementQuantitiy2), std::nullopt);
    EXPECT_EQ(elementInt4.compareTo(elementQuantitiy3), std::strong_ordering::greater);

    EXPECT_EQ(elementDecPi.compareTo(elementInt3), std::strong_ordering::greater);
    EXPECT_EQ(elementDecPi.compareTo(elementInt4), std::strong_ordering::less);
    EXPECT_EQ(elementDecPi.compareTo(elementDecPi), std::strong_ordering::equal);
    EXPECT_EQ(elementDecPi.compareTo(elementDec3), std::strong_ordering::greater);
    EXPECT_EQ(elementDecPi.compareTo(elementStrA), std::strong_ordering::less);
    EXPECT_EQ(elementDecPi.compareTo(elementStrB), std::strong_ordering::less);
    EXPECT_EQ(elementDecPi.compareTo(elementQuantitiy1), std::nullopt);
    EXPECT_EQ(elementDecPi.compareTo(elementQuantitiy2), std::nullopt);
    EXPECT_EQ(elementDecPi.compareTo(elementQuantitiy3), std::strong_ordering::greater);

    EXPECT_EQ(elementDec3.compareTo(elementInt3), std::strong_ordering::equal);
    EXPECT_EQ(elementDec3.compareTo(elementInt4), std::strong_ordering::less);
    EXPECT_EQ(elementDec3.compareTo(elementDecPi), std::strong_ordering::less);
    EXPECT_EQ(elementDec3.compareTo(elementDec3), std::strong_ordering::equal);
    EXPECT_EQ(elementDec3.compareTo(elementStrA), std::strong_ordering::less);
    EXPECT_EQ(elementDec3.compareTo(elementStrB), std::strong_ordering::less);
    EXPECT_EQ(elementDec3.compareTo(elementQuantitiy1), std::nullopt);
    EXPECT_EQ(elementDec3.compareTo(elementQuantitiy2), std::nullopt);
    EXPECT_EQ(elementDec3.compareTo(elementQuantitiy3), std::strong_ordering::equal);

    EXPECT_EQ(elementStrA.compareTo(elementInt3), std::strong_ordering::greater);
    EXPECT_EQ(elementStrA.compareTo(elementInt4), std::strong_ordering::greater);
    EXPECT_EQ(elementStrA.compareTo(elementDecPi), std::strong_ordering::greater);
    EXPECT_EQ(elementStrA.compareTo(elementDec3), std::strong_ordering::greater);
    EXPECT_EQ(elementStrA.compareTo(elementStrA), std::strong_ordering::equal);
    EXPECT_EQ(elementStrA.compareTo(elementStrB), std::strong_ordering::greater);
    EXPECT_EQ(elementStrA.compareTo(elementQuantitiy1), std::strong_ordering::greater);
    EXPECT_EQ(elementStrA.compareTo(elementQuantitiy2), std::strong_ordering::greater);
    EXPECT_EQ(elementStrA.compareTo(elementQuantitiy3), std::strong_ordering::greater);

    EXPECT_EQ(elementStrB.compareTo(elementInt3), std::strong_ordering::greater);
    EXPECT_EQ(elementStrB.compareTo(elementInt4), std::strong_ordering::greater);
    EXPECT_EQ(elementStrB.compareTo(elementDecPi), std::strong_ordering::greater);
    EXPECT_EQ(elementStrB.compareTo(elementDec3), std::strong_ordering::greater);
    EXPECT_EQ(elementStrB.compareTo(elementStrA), std::strong_ordering::less);
    EXPECT_EQ(elementStrB.compareTo(elementStrB), std::strong_ordering::equal);
    EXPECT_EQ(elementStrB.compareTo(elementQuantitiy1), std::strong_ordering::greater);
    EXPECT_EQ(elementStrB.compareTo(elementQuantitiy2), std::strong_ordering::greater);
    EXPECT_EQ(elementStrB.compareTo(elementQuantitiy3), std::strong_ordering::greater);

    EXPECT_EQ(elementQuantitiy1.compareTo(elementInt3), std::nullopt);
    EXPECT_EQ(elementQuantitiy1.compareTo(elementInt4), std::nullopt);
    EXPECT_EQ(elementQuantitiy1.compareTo(elementDecPi), std::nullopt);
    EXPECT_EQ(elementQuantitiy1.compareTo(elementDec3), std::nullopt);
    EXPECT_EQ(elementQuantitiy1.compareTo(elementStrA), std::strong_ordering::less);
    EXPECT_EQ(elementQuantitiy1.compareTo(elementStrB), std::strong_ordering::less);
    EXPECT_EQ(elementQuantitiy1.compareTo(elementQuantitiy1), std::strong_ordering::equal);
    EXPECT_EQ(elementQuantitiy1.compareTo(elementQuantitiy2), std::strong_ordering::less);
    EXPECT_EQ(elementQuantitiy1.compareTo(elementQuantitiy3), std::nullopt);

    EXPECT_EQ(elementQuantitiy2.compareTo(elementInt3), std::nullopt);
    EXPECT_EQ(elementQuantitiy2.compareTo(elementInt4), std::nullopt);
    EXPECT_EQ(elementQuantitiy2.compareTo(elementDecPi), std::nullopt);
    EXPECT_EQ(elementQuantitiy2.compareTo(elementDec3), std::nullopt);
    EXPECT_EQ(elementQuantitiy2.compareTo(elementStrA), std::strong_ordering::less);
    EXPECT_EQ(elementQuantitiy2.compareTo(elementStrB), std::strong_ordering::less);
    EXPECT_EQ(elementQuantitiy2.compareTo(elementQuantitiy1), std::strong_ordering::greater);
    EXPECT_EQ(elementQuantitiy2.compareTo(elementQuantitiy2), std::strong_ordering::equal);
    EXPECT_EQ(elementQuantitiy2.compareTo(elementQuantitiy3), std::nullopt);

    EXPECT_EQ(elementQuantitiy3.compareTo(elementInt3), std::strong_ordering::equal);
    EXPECT_EQ(elementQuantitiy3.compareTo(elementInt4), std::strong_ordering::less);
    EXPECT_EQ(elementQuantitiy3.compareTo(elementDecPi), std::strong_ordering::less);
    EXPECT_EQ(elementQuantitiy3.compareTo(elementDec3), std::strong_ordering::equal);
    EXPECT_EQ(elementQuantitiy3.compareTo(elementStrA), std::strong_ordering::less);
    EXPECT_EQ(elementQuantitiy3.compareTo(elementStrB), std::strong_ordering::less);
    EXPECT_EQ(elementQuantitiy3.compareTo(elementQuantitiy1), std::nullopt);
    EXPECT_EQ(elementQuantitiy3.compareTo(elementQuantitiy2), std::nullopt);
    EXPECT_EQ(elementQuantitiy3.compareTo(elementQuantitiy3), std::strong_ordering::equal);
}

TEST_F(ElementTest, QuantityConstructor)
{
    Element::QuantityType q1("25m");
    Element::QuantityType q2("25.0m");
    Element::QuantityType q3("25.m");
    Element::QuantityType q4("0.0m");
    Element::QuantityType q5("25.11111111 cm");
    Element::QuantityType q6("25.22222222  mm");
    Element::QuantityType q7("25.33333333  ");

    EXPECT_EQ(q1.value(), DecimalType("25"));
    EXPECT_EQ(q1.unit(), "m");
    EXPECT_EQ(q2.value(), DecimalType("25"));
    EXPECT_EQ(q2.unit(), "m");
    EXPECT_EQ(q3.value(), DecimalType("25"));
    EXPECT_EQ(q3.unit(), "m");
    EXPECT_EQ(q4.value(), DecimalType("0"));
    EXPECT_EQ(q4.unit(), "m");
    EXPECT_EQ(q5.value(), DecimalType("25.11111111"));
    EXPECT_EQ(q5.unit(), "cm");
    EXPECT_EQ(q6.value(), DecimalType("25.22222222"));
    EXPECT_EQ(q6.unit(), "mm");
    EXPECT_EQ(q7.value(), DecimalType("25.33333333"));
    EXPECT_EQ(q7.unit(), "");
}

class ElementNavigationTest : public ElementTest
{
public:
    void SetUp() override
    {
        ElementTest::SetUp();
        const auto& docStr =
            ResourceManager::instance().getStringResource("test/fhir-path/element-navigation-test.json");
        json = model::NumberAsStringParserDocument::fromJson(docStr);
        bundle = std::make_shared<ErpElement>(mRepo, std::weak_ptr<fhirtools::Element>{}, "Bundle", &json.value());
        ASSERT_EQ(bundle->resourceType(), "Bundle");

        entryElements = bundle->subElements("entry");
        ASSERT_EQ(entryElements.size(), 2);

        entry0DomainResource = entryElements[0]->subElements("resource");
        ASSERT_EQ(entry0DomainResource.size(), 1);

        entry0DomainResourceId = entry0DomainResource[0]->subElements("id");
        ASSERT_EQ(entry0DomainResourceId.size(), 1);

        entry0DomainResourceContained = entry0DomainResource[0]->subElements("contained");
        ASSERT_EQ(entry0DomainResourceContained.size(), 1);
        ASSERT_EQ(entry0DomainResourceContained[0]->resourceType(), "Resource");

        entry0DomainResourceContainedId = entry0DomainResourceContained[0]->subElements("id");
        ASSERT_EQ(entry0DomainResourceContainedId.size(), 1);

        entry0DomainResourceExtension = entry0DomainResource[0]->subElements("extension");
        ASSERT_EQ(entry0DomainResourceExtension.size(), 1);

        entry0DomainResourceExtension0Url = entry0DomainResourceExtension[0]->subElements("url");
        ASSERT_EQ(entry0DomainResourceExtension0Url.size(), 1);

        entry0DomainResourceExtension0ValueReference = entry0DomainResourceExtension[0]->subElements("valueReference");
        ASSERT_EQ(entry0DomainResourceExtension0ValueReference.size(), 1);
        entry0DomainResourceExtension0ValueReferenceReference =
            entry0DomainResourceExtension0ValueReference[0]->subElements("reference");
        ASSERT_EQ(entry0DomainResourceExtension0ValueReferenceReference.size(), 1);


        entry1Bundle = entryElements[1]->subElements("resource");
        ASSERT_EQ(entry1Bundle.size(), 1);
        ASSERT_EQ(entry1Bundle[0]->resourceType(), "Bundle");

        entry1BundleId = entry1Bundle[0]->subElements("id");
        ASSERT_EQ(entry1BundleId.size(), 1);

        entry1BundleEntry = entry1Bundle[0]->subElements("entry");
        ASSERT_EQ(entry1BundleEntry.size(), 1);

        entry1BundleEntry0Resource = entry1BundleEntry[0]->subElements("resource");
        ASSERT_EQ(entry1BundleEntry0Resource.size(), 1);
        ASSERT_EQ(entry1BundleEntry0Resource[0]->resourceType(), "Resource");

        entry1BundleEntry0ResourceId = entry1BundleEntry0Resource[0]->subElements("id");
        ASSERT_EQ(entry1BundleEntry0ResourceId.size(), 1);
    }

protected:
    void testResolution(const Element& baseElement, const std::string_view ref,
                        const std::shared_ptr<const Element>& expectedResolution)
    {
        const auto& [element, resultList] =
            baseElement.resolveReference(Element::IdentityAndResult::fromReferenceString(ref, "N/A").identity, "N/A");
        ASSERT_LE(resultList.highestSeverity(), Severity::debug) << resultList.summary(fhirtools::Severity::debug);
        ASSERT_EQ(element, expectedResolution) << resultList.summary(fhirtools::Severity::debug);
    }

    void testResolution(const Element& referenceElement, const std::shared_ptr<const Element>& expectedResolution)
    {
        const auto& [element, resultList] = referenceElement.resolveReference("N/A");
        ASSERT_LE(resultList.highestSeverity(), Severity::debug) << resultList.summary(fhirtools::Severity::debug);
        ASSERT_EQ(element, expectedResolution) << resultList.summary(fhirtools::Severity::debug);
    }

    void testReferenceTargetInfo(const Element& element, std::optional<std::string> expectedFullUrl)
    {
        const auto& res = element.referenceTargetIdentity("N/A");
        if (expectedFullUrl)
        {
            ASSERT_EQ(to_string(res.identity), *expectedFullUrl) << res.result.summary(fhirtools::Severity::debug);
            ASSERT_LE(res.result.highestSeverity(), Severity::debug) << res.result.summary(fhirtools::Severity::debug);
        }
        else
        {
            ASSERT_TRUE(res.identity.pathOrId.empty()) << res.identity.pathOrId;
            ASSERT_FALSE(res.identity.containedId.has_value()) << *res.identity.containedId;
        }
    }

    using ElementVec = std::vector<std::shared_ptr<const Element>>;

    std::shared_ptr<const Element> bundle;
    ElementVec entryElements;

    ElementVec entry0DomainResource;
    ElementVec entry0DomainResourceId;
    ElementVec entry0DomainResourceContained;
    ElementVec entry0DomainResourceContainedId;
    ElementVec entry0DomainResourceExtension;
    ElementVec entry0DomainResourceExtension0Url;
    ElementVec entry0DomainResourceExtension0ValueReference;
    ElementVec entry0DomainResourceExtension0ValueReferenceReference;

    ElementVec entry1Bundle;
    ElementVec entry1BundleId;
    ElementVec entry1BundleEntry;
    ElementVec entry1BundleEntry0Resource;
    ElementVec entry1BundleEntry0ResourceId;

private:
    std::optional<model::NumberAsStringParserDocument> json;
};

TEST_F(ElementNavigationTest, resourceRoot)
{
    EXPECT_EQ(bundle->resourceRoot(), bundle);

    EXPECT_EQ(entryElements[0]->resourceRoot(), bundle);
    EXPECT_EQ(entry0DomainResource[0]->resourceRoot(), entry0DomainResource[0]);
    EXPECT_EQ(entry0DomainResourceId[0]->resourceRoot(), entry0DomainResource[0]);
    EXPECT_EQ(entry0DomainResourceContained[0]->resourceRoot(), entry0DomainResourceContained[0]);
    EXPECT_EQ(entry0DomainResourceContainedId[0]->resourceRoot(), entry0DomainResourceContained[0]);
    EXPECT_EQ(entry0DomainResourceExtension[0]->resourceRoot(), entry0DomainResource[0]);
    EXPECT_EQ(entry0DomainResourceExtension0Url[0]->resourceRoot(), entry0DomainResource[0]);
    EXPECT_EQ(entry0DomainResourceExtension0ValueReference[0]->resourceRoot(), entry0DomainResource[0]);

    EXPECT_EQ(entryElements[1]->resourceRoot(), bundle);
    EXPECT_EQ(entry1Bundle[0]->resourceRoot(), entry1Bundle[0]);
    EXPECT_EQ(entry1BundleId[0]->resourceRoot(), entry1Bundle[0]);
    EXPECT_EQ(entry1BundleEntry[0]->resourceRoot(), entry1Bundle[0]);
    EXPECT_EQ(entry1BundleEntry0Resource[0]->resourceRoot(), entry1BundleEntry0Resource[0]);
    EXPECT_EQ(entry1BundleEntry0ResourceId[0]->resourceRoot(), entry1BundleEntry0Resource[0]);
}

TEST_F(ElementNavigationTest, containerResource)
{
    EXPECT_EQ(bundle->containerResource(), nullptr);

    EXPECT_EQ(entryElements[0]->containerResource(), nullptr);
    EXPECT_EQ(entry0DomainResource[0]->containerResource(), entry0DomainResource[0]);
    EXPECT_EQ(entry0DomainResourceId[0]->containerResource(), entry0DomainResource[0]);
    EXPECT_EQ(entry0DomainResourceContained[0]->containerResource(), entry0DomainResource[0]);
    EXPECT_EQ(entry0DomainResourceContainedId[0]->containerResource(), entry0DomainResource[0]);
    EXPECT_EQ(entry0DomainResourceExtension[0]->containerResource(), entry0DomainResource[0]);
    EXPECT_EQ(entry0DomainResourceExtension0Url[0]->containerResource(), entry0DomainResource[0]);
    EXPECT_EQ(entry0DomainResourceExtension0ValueReference[0]->containerResource(), entry0DomainResource[0]);

    EXPECT_EQ(entryElements[1]->containerResource(), nullptr);
    EXPECT_EQ(entry1Bundle[0]->containerResource(), nullptr);
    EXPECT_EQ(entry1BundleId[0]->containerResource(), nullptr);
    EXPECT_EQ(entry1BundleEntry[0]->containerResource(), nullptr);
    EXPECT_EQ(entry1BundleEntry0Resource[0]->containerResource(), nullptr);
    EXPECT_EQ(entry1BundleEntry0ResourceId[0]->containerResource(), nullptr);
}

TEST_F(ElementNavigationTest, containingBundle)
{
    EXPECT_EQ(bundle->containingBundle(), nullptr);

    EXPECT_EQ(entryElements[0]->containingBundle(), nullptr);
    EXPECT_EQ(entry0DomainResource[0]->containingBundle(), bundle);
    EXPECT_EQ(entry0DomainResourceId[0]->containingBundle(), bundle);
    EXPECT_EQ(entry0DomainResourceContained[0]->containingBundle(), bundle);
    EXPECT_EQ(entry0DomainResourceContainedId[0]->containingBundle(), bundle);
    EXPECT_EQ(entry0DomainResourceExtension[0]->containingBundle(), bundle);
    EXPECT_EQ(entry0DomainResourceExtension0Url[0]->containingBundle(), bundle);
    EXPECT_EQ(entry0DomainResourceExtension0ValueReference[0]->containingBundle(), bundle);

    EXPECT_EQ(entryElements[1]->containingBundle(), nullptr);
    EXPECT_EQ(entry1Bundle[0]->containingBundle(), bundle);
    EXPECT_EQ(entry1BundleId[0]->containingBundle(), bundle);
    EXPECT_EQ(entry1BundleEntry[0]->containingBundle(), bundle);
    EXPECT_EQ(entry1BundleEntry0Resource[0]->containingBundle(), entry1Bundle[0]);
    EXPECT_EQ(entry1BundleEntry0ResourceId[0]->containingBundle(), entry1Bundle[0]);
}

TEST_F(ElementNavigationTest, parentResource)
{
    EXPECT_EQ(bundle->parentResource(), nullptr);

    EXPECT_EQ(entryElements[0]->parentResource(), nullptr);
    EXPECT_EQ(entry0DomainResource[0]->parentResource(), bundle);
    EXPECT_EQ(entry0DomainResourceId[0]->parentResource(), bundle);
    EXPECT_EQ(entry0DomainResourceContained[0]->parentResource(), entry0DomainResource[0]);
    EXPECT_EQ(entry0DomainResourceContainedId[0]->parentResource(), entry0DomainResource[0]);
    EXPECT_EQ(entry0DomainResourceExtension[0]->parentResource(), bundle);
    EXPECT_EQ(entry0DomainResourceExtension0Url[0]->parentResource(), bundle);
    EXPECT_EQ(entry0DomainResourceExtension0ValueReference[0]->parentResource(), bundle);

    EXPECT_EQ(entryElements[1]->parentResource(), nullptr);
    EXPECT_EQ(entry1Bundle[0]->parentResource(), bundle);
    EXPECT_EQ(entry1BundleId[0]->parentResource(), bundle);
    EXPECT_EQ(entry1BundleEntry[0]->parentResource(), bundle);
    EXPECT_EQ(entry1BundleEntry0Resource[0]->parentResource(), entry1Bundle[0]);
    EXPECT_EQ(entry1BundleEntry0ResourceId[0]->parentResource(), entry1Bundle[0]);
}

TEST_F(ElementNavigationTest, resourceReferenceInfo)
{
    EXPECT_EQ(to_string(bundle->resourceIdentity("Bundle").identity), "Bundle/rootBundle");

    EXPECT_EQ(to_string(entryElements[0]->resourceIdentity("Bundle.entry[0]").identity), "Bundle/rootBundle");
    EXPECT_EQ(to_string(entry0DomainResource[0]->resourceIdentity("Bundle.entry[0].resource{DomainResource}").identity),
              "http://erp.test/DomainResource/1");
    EXPECT_EQ(
        to_string(entry0DomainResourceId[0]->resourceIdentity("Bundle.entry[0].resource{DomainResource}.id").identity),
        "http://erp.test/DomainResource/1");
    EXPECT_EQ(to_string(entry0DomainResourceContained[0]
                            ->resourceIdentity("Bundle.entry[0].resource{DomainResource}.contained[0]{Resource}")
                            .identity),
              "http://erp.test/DomainResource/1#2");
    EXPECT_EQ(to_string(entry0DomainResourceContainedId[0]
                            ->resourceIdentity("Bundle.entry[0].resource{DomainResource}.contained[0]{Resource}.id")
                            .identity),
              "http://erp.test/DomainResource/1#2");
    EXPECT_EQ(to_string(entry0DomainResourceExtension[0]
                            ->resourceIdentity("Bundle.entry[0].resource{DomainResource}.extension[0]")
                            .identity),
              "http://erp.test/DomainResource/1");
    EXPECT_EQ(to_string(entry0DomainResourceExtension0Url[0]
                            ->resourceIdentity("Bundle.entry[0].resource{DomainResource}.extension[0].url")
                            .identity),
              "http://erp.test/DomainResource/1");
    EXPECT_EQ(to_string(entry0DomainResourceExtension0ValueReference[0]
                            ->resourceIdentity("Bundle.entry[0].resource{DomainResource}.extension[0].valueReference")
                            .identity),
              "http://erp.test/DomainResource/1");


    EXPECT_EQ(to_string(entryElements[1]->resourceIdentity("Bundle.entry[1]").identity), "Bundle/rootBundle");
    EXPECT_EQ(to_string(entry1Bundle[0]->resourceIdentity("Bundle.entry[1].resource{Bundle}").identity),
              "http://erp.test/Bundle/1");
    EXPECT_EQ(to_string(entry1BundleId[0]->resourceIdentity("Bundle.entry[1].resource{Bundle}.id").identity),
              "http://erp.test/Bundle/1");
    EXPECT_EQ(to_string(entry1BundleEntry[0]->resourceIdentity("Bundle.entry[1].resource{Bundle}.entry[0]").identity),
              "http://erp.test/Bundle/1");
    EXPECT_EQ(to_string(entry1BundleEntry0Resource[0]
                            ->resourceIdentity("Bundle.entry[1].resource{Bundle}.entry[0].resource{Resource}")
                            .identity),
              "http://erp.test/Resource/2");
    EXPECT_EQ(to_string(entry1BundleEntry0ResourceId[0]
                            ->resourceIdentity("Bundle.entry[1].resource{Bundle}.entry[0].resource{Resource}.id")
                            .identity),
              "http://erp.test/Resource/2");
}

TEST_F(ElementNavigationTest, referenceTargetInfo)
{
    EXPECT_NO_FATAL_FAILURE(testReferenceTargetInfo(*bundle, std::nullopt));

    EXPECT_NO_FATAL_FAILURE(testReferenceTargetInfo(*entryElements[0], std::nullopt));
    EXPECT_NO_FATAL_FAILURE(testReferenceTargetInfo(*entry0DomainResource[0], std::nullopt));
    EXPECT_NO_FATAL_FAILURE(testReferenceTargetInfo(*entry0DomainResourceId[0], std::nullopt));
    EXPECT_NO_FATAL_FAILURE(testReferenceTargetInfo(*entry0DomainResourceContained[0], std::nullopt));
    EXPECT_NO_FATAL_FAILURE(testReferenceTargetInfo(*entry0DomainResourceContainedId[0], std::nullopt));
    EXPECT_NO_FATAL_FAILURE(testReferenceTargetInfo(*entry0DomainResourceExtension[0], std::nullopt));
    // NOTE: the url field is a URL. As references might be non RESTful, we accept it as potantial reference
    EXPECT_NO_FATAL_FAILURE(
        testReferenceTargetInfo(*entry0DomainResourceExtension0Url[0], "http://erp.test/Reference"));

    EXPECT_NO_FATAL_FAILURE(testReferenceTargetInfo(*entry0DomainResourceExtension0ValueReference[0],
                                                    "http://erp.test/DomainResource/1#2"));
    EXPECT_NO_FATAL_FAILURE(testReferenceTargetInfo(*entry0DomainResourceExtension0ValueReferenceReference[0],
                                                    "http://erp.test/DomainResource/1#2"));

    EXPECT_NO_FATAL_FAILURE(testReferenceTargetInfo(*entryElements[1], std::nullopt));
    EXPECT_NO_FATAL_FAILURE(testReferenceTargetInfo(*entry1Bundle[0], std::nullopt));
    EXPECT_NO_FATAL_FAILURE(testReferenceTargetInfo(*entry1BundleId[0], std::nullopt));
    EXPECT_NO_FATAL_FAILURE(testReferenceTargetInfo(*entry1BundleEntry[0], std::nullopt));
    EXPECT_NO_FATAL_FAILURE(testReferenceTargetInfo(*entry1BundleEntry0Resource[0], std::nullopt));
    EXPECT_NO_FATAL_FAILURE(testReferenceTargetInfo(*entry1BundleEntry0ResourceId[0], std::nullopt));
}

TEST_F(ElementNavigationTest, resolveReferenceByString)
{
    // resolve root resource
    EXPECT_NO_FATAL_FAILURE(testResolution(*bundle, "Bundle/rootBundle", bundle));
    EXPECT_NO_FATAL_FAILURE(testResolution(*entryElements[0], "Bundle/rootBundle", bundle));
    EXPECT_NO_FATAL_FAILURE(testResolution(*entry0DomainResource[0], "Bundle/rootBundle", nullptr));

    // resolve http://erp.test/DomainResource/1
    EXPECT_NO_FATAL_FAILURE(testResolution(*bundle, "http://erp.test/DomainResource/1", nullptr));
    EXPECT_NO_FATAL_FAILURE(
        testResolution(*entry0DomainResource[0], "http://erp.test/DomainResource/1", entry0DomainResource[0]));
    EXPECT_NO_FATAL_FAILURE(
        testResolution(*entry1BundleEntry[0], "http://erp.test/DomainResource/1", entry0DomainResource[0]));
    EXPECT_NO_FATAL_FAILURE(
        testResolution(*entry1BundleEntry0Resource[0], "http://erp.test/DomainResource/1", nullptr));

    // resolve http://erp.test/DomainResource/1#2
    EXPECT_NO_FATAL_FAILURE(testResolution(*bundle, "http://erp.test/DomainResource/1#2", nullptr));
    EXPECT_NO_FATAL_FAILURE(testResolution(*entry0DomainResource[0], "http://erp.test/DomainResource/1#2",
                                           entry0DomainResourceContained[0]));
    EXPECT_NO_FATAL_FAILURE(
        testResolution(*entry1BundleEntry[0], "http://erp.test/DomainResource/1#2", entry0DomainResourceContained[0]));
    EXPECT_NO_FATAL_FAILURE(
        testResolution(*entry1BundleEntry0Resource[0], "http://erp.test/DomainResource/1#2", nullptr));


    // resolve http://erp.test/Bundle/1
    EXPECT_NO_FATAL_FAILURE(testResolution(*bundle, "http://erp.test/Bundle/1", nullptr));
    EXPECT_NO_FATAL_FAILURE(testResolution(*entry0DomainResource[0], "http://erp.test/Bundle/1", entry1Bundle[0]));
    EXPECT_NO_FATAL_FAILURE(testResolution(*entry1BundleEntry[0], "http://erp.test/Bundle/1", entry1Bundle[0]));
    EXPECT_NO_FATAL_FAILURE(testResolution(*entry1BundleEntry0Resource[0], "http://erp.test/Bundle/1", nullptr));

    // resolve http://erp.test/Resource/2
    EXPECT_NO_FATAL_FAILURE(testResolution(*bundle, "http://erp.test/Resource/2", nullptr));
    EXPECT_NO_FATAL_FAILURE(testResolution(*entry0DomainResource[0], "http://erp.test/Resource/2", nullptr));
    EXPECT_NO_FATAL_FAILURE(testResolution(*entry1BundleEntry[0], "http://erp.test/Resource/2", nullptr));
    EXPECT_NO_FATAL_FAILURE(
        testResolution(*entry1BundleEntry0Resource[0], "http://erp.test/Resource/2", entry1BundleEntry0Resource[0]));
}

TEST_F(ElementNavigationTest, resolveReferenceFromElement)
{
    EXPECT_NO_FATAL_FAILURE(testResolution(*bundle, nullptr));

    EXPECT_NO_FATAL_FAILURE(testResolution(*entryElements[0], nullptr));
    EXPECT_NO_FATAL_FAILURE(testResolution(*entry0DomainResource[0], nullptr));
    EXPECT_NO_FATAL_FAILURE(testResolution(*entry0DomainResourceId[0], nullptr));
    EXPECT_NO_FATAL_FAILURE(testResolution(*entry0DomainResourceContained[0], nullptr));
    EXPECT_NO_FATAL_FAILURE(testResolution(*entry0DomainResourceContainedId[0], nullptr));
    EXPECT_NO_FATAL_FAILURE(testResolution(*entry0DomainResourceExtension[0], nullptr));
    EXPECT_NO_FATAL_FAILURE(testResolution(*entry0DomainResourceExtension0Url[0], nullptr));
    EXPECT_NO_FATAL_FAILURE(
        testResolution(*entry0DomainResourceExtension0ValueReference[0], entry0DomainResourceContained[0]));
    EXPECT_NO_FATAL_FAILURE(
        testResolution(*entry0DomainResourceExtension0ValueReferenceReference[0], entry0DomainResourceContained[0]));

    EXPECT_NO_FATAL_FAILURE(testResolution(*entryElements[1], nullptr));
    EXPECT_NO_FATAL_FAILURE(testResolution(*entry1Bundle[0], nullptr));
    EXPECT_NO_FATAL_FAILURE(testResolution(*entry1BundleId[0], nullptr));
    EXPECT_NO_FATAL_FAILURE(testResolution(*entry1BundleEntry[0], nullptr));
    EXPECT_NO_FATAL_FAILURE(testResolution(*entry1BundleEntry0Resource[0], nullptr));
    EXPECT_NO_FATAL_FAILURE(testResolution(*entry1BundleEntry0ResourceId[0], nullptr));
}

TEST_F(ElementNavigationTest, documentRoot)
{
    EXPECT_EQ(bundle->documentRoot(), bundle);

    EXPECT_EQ(entryElements[0]->documentRoot(), bundle);
    EXPECT_EQ(entry0DomainResource[0]->documentRoot(), bundle);
    EXPECT_EQ(entry0DomainResourceId[0]->documentRoot(), bundle);
    EXPECT_EQ(entry0DomainResourceContained[0]->documentRoot(), bundle);
    EXPECT_EQ(entry0DomainResourceContainedId[0]->documentRoot(), bundle);
    EXPECT_EQ(entry0DomainResourceExtension[0]->documentRoot(), bundle);
    EXPECT_EQ(entry0DomainResourceExtension0Url[0]->documentRoot(), bundle);
    EXPECT_EQ(entry0DomainResourceExtension0ValueReference[0]->documentRoot(), bundle);

    EXPECT_EQ(entryElements[1]->documentRoot(), bundle);
    EXPECT_EQ(entry1Bundle[0]->documentRoot(), bundle);
    EXPECT_EQ(entry1BundleId[0]->documentRoot(), bundle);
    EXPECT_EQ(entry1BundleEntry[0]->documentRoot(), bundle);
    EXPECT_EQ(entry1BundleEntry0Resource[0]->documentRoot(), bundle);
    EXPECT_EQ(entry1BundleEntry0ResourceId[0]->documentRoot(), bundle);
}

TEST(ElementIdentitySchemeTest, compare)
{
    using Scheme = Element::Identity::Scheme;
    EXPECT_EQ(Scheme{"http"}, Scheme{"https"});
    EXPECT_EQ(Scheme{"http"}, Scheme{"http"});
    EXPECT_EQ(Scheme{"https"}, Scheme{"http"});

    EXPECT_GT(Scheme{"AB"}, Scheme{"AA"});
    EXPECT_GE(Scheme{"AB"}, Scheme{"AA"});

    EXPECT_LT(Scheme{"A"}, Scheme{"AA"});
    EXPECT_LE(Scheme{"A"}, Scheme{"AA"});

    EXPECT_EQ(Scheme{"A"}, Scheme{"A"});
    EXPECT_LE(Scheme{"A"}, Scheme{"A"});
    EXPECT_GE(Scheme{"A"}, Scheme{"A"});

    EXPECT_NE(Scheme{"A"}, Scheme{"B"});
    EXPECT_NE(Scheme{"http"}, Scheme{"B"});
    EXPECT_NE(Scheme{"https"}, Scheme{"B"});

    EXPECT_FALSE(Scheme{"http"} != Scheme{"https"});
    EXPECT_FALSE(Scheme{"https"} != Scheme{"http"});

    EXPECT_TRUE(Element::Identity{.scheme{"https"}} == Element::Identity{.scheme{"http"}});
    EXPECT_EQ(Element::Identity{.scheme{"https"}}, Element::Identity{.scheme{"http"}});
}


class ElementIdentityFromStringTest : public ::testing::TestWithParam<std::tuple<std::string, Element::Identity>>
{
public:
    using initializer_list = std::initializer_list<ParamType>;
};

TEST_P(ElementIdentityFromStringTest, run)
{
    const auto& [candidate, expected] = GetParam();
    auto parsedCandidate = Element::IdentityAndResult::fromReferenceString(candidate, "N/A").identity;
    EXPECT_EQ(parsedCandidate.scheme, expected.scheme);
    EXPECT_EQ(parsedCandidate.pathOrId, expected.pathOrId);
    EXPECT_EQ(parsedCandidate.containedId, expected.containedId);
    EXPECT_EQ(parsedCandidate, expected);
}

// clang-format off
INSTANTIATE_TEST_SUITE_P(simple, ElementIdentityFromStringTest,
                         ::testing::ValuesIn(ElementIdentityFromStringTest::initializer_list{
    {
       "http://erp.test/DomainResource/1",
       {.scheme = "http", .pathOrId = "//erp.test/DomainResource/1"}
    },
    {
        "DomainResource/1",
        {.pathOrId = "DomainResource/1"}
    },
    {
        "http://erp.test/DomainResource/1#2",
        {.scheme = "http", .pathOrId = "//erp.test/DomainResource/1", .containedId = "2"}
    },
    {
        "DomainResource/1#2",
        {.pathOrId = "DomainResource/1", .containedId = "2"}
    },
    {
        "#2",
        {.containedId = "2"}
    }
}));

INSTANTIATE_TEST_SUITE_P(ignoreQuery, ElementIdentityFromStringTest,
                         ::testing::ValuesIn(ElementIdentityFromStringTest::initializer_list{
    {
       "http://erp.test/DomainResource/1?ignore=1&otherIgnore=2",
       {.scheme = "http", .pathOrId = "//erp.test/DomainResource/1"}
    },
    {
        "DomainResource/1?ignore=1&otherIgnore=2",
        {.pathOrId = "DomainResource/1"}
    },
    {
        "http://erp.test/DomainResource/1?ignore=1&otherIgnore=2#2",
        {.scheme = "http", .pathOrId = "//erp.test/DomainResource/1", .containedId = "2"}
    },
    {
        "DomainResource/1?ignore=1&otherIgnore=2#2",
        {.pathOrId = "DomainResource/1", .containedId = "2"}
    },
    {
        "?ignore=1&otherIgnore=2#2",
        {.containedId = "2"}
    }
}));
// clang-format on

class ElementIdentityToStringTest : public ::testing::TestWithParam<std::tuple<Element::Identity, std::string>>
{
public:
    using initializer_list = std::initializer_list<ParamType>;
};

TEST_P(ElementIdentityToStringTest, run)
{
    const auto& [candidate, expected] = GetParam();
    EXPECT_EQ(to_string(candidate), expected);
}

// clang-format off
INSTANTIATE_TEST_SUITE_P(simple, ElementIdentityToStringTest,
                         ::testing::ValuesIn(ElementIdentityToStringTest::initializer_list{
   {
       {.scheme = "http", .pathOrId = "//erp.test/DomainResource/1"},
       "http://erp.test/DomainResource/1"
    },
    {
        {.pathOrId = "DomainResource/1"},
        "DomainResource/1"
    },
    {
        {.scheme = "http", .pathOrId = "//erp.test/DomainResource/1", .containedId = "2"},
        "http://erp.test/DomainResource/1#2"
    },
    {
        {.pathOrId = "DomainResource/1", .containedId = "2"},
        "DomainResource/1#2"
    },
    {
        {.containedId = "2"},
        "#2"
    }
}));
// clang-format on

class ElementIdentityEqualityTest : public ::testing::TestWithParam<std::tuple<std::string, std::string, bool>>
{
public:
    using initializer_list = std::initializer_list<ParamType>;
};

TEST_P(ElementIdentityEqualityTest, run)
{
    const auto& [lhs, rhs, isEqual] = GetParam();
    auto parsedLhs = Element::IdentityAndResult::fromReferenceString(lhs, "N/A").identity;
    auto parsedRhs = Element::IdentityAndResult::fromReferenceString(rhs, "N/A").identity;

    if (isEqual)
    {
        EXPECT_EQ(parsedLhs, parsedRhs);
    }
    else
    {
        EXPECT_NE(parsedLhs, parsedRhs);
    }
}
// clang-format off
INSTANTIATE_TEST_SUITE_P(same, ElementIdentityEqualityTest,
                         ::testing::ValuesIn(ElementIdentityEqualityTest::initializer_list{
   {"urn:uuid:cd232626-e29c-49d7-a191-1c797238f48a", "urn:uuid:cd232626-e29c-49d7-a191-1c797238f48a", true},
   {"urn:uuid:cd232626-e29c-49d7-a191-1c797238f48a#1", "urn:uuid:cd232626-e29c-49d7-a191-1c797238f48a#1", true},
   {"http://erp.test/DomainResource/1", "http://erp.test/DomainResource/1", true},
   {"DomainResource/1", "DomainResource/1", true},
   {"http://erp.test/DomainResource/1#2", "http://erp.test/DomainResource/1#2", true},
   {"DomainResource/1#2", "DomainResource/1#2", true},
   {"#2", "#2", true}
 }));

INSTANTIATE_TEST_SUITE_P(httpEqualsHttps, ElementIdentityEqualityTest,
                         ::testing::ValuesIn(ElementIdentityEqualityTest::initializer_list{
   {"https://erp.test/DomainResource/1", "http://erp.test/DomainResource/1", true},
   {"https://erp.test/DomainResource/1#2", "http://erp.test/DomainResource/1#2", true}
}));

INSTANTIATE_TEST_SUITE_P(otherScheme, ElementIdentityEqualityTest,
                         ::testing::ValuesIn(ElementIdentityEqualityTest::initializer_list{
   {"http://erp.test/DomainResource/1", "ftp://erp.test/DomainResource/1", false},
   {"https://erp.test/DomainResource/1#2", "ftp://erp.test/DomainResource/1#2", false}
}));

INSTANTIATE_TEST_SUITE_P(otherPath, ElementIdentityEqualityTest,
                         ::testing::ValuesIn(ElementIdentityEqualityTest::initializer_list{
   {"https://erp.test/other/DomainResource/1", "http://erp.test/DomainResource/1", false},
   {"https://erp.test/DomainResource/1#2", "http://erp.test/other/DomainResource/1#2", false}
}));

INSTANTIATE_TEST_SUITE_P(otherFragment, ElementIdentityEqualityTest,
                         ::testing::ValuesIn( ElementIdentityEqualityTest::initializer_list{
   {"https://erp.test/DomainResource/1#1", "http://erp.test/DomainResource/1#3", false},
   {"https://erp.test/DomainResource/1#2", "http://erp.test/DomainResource/1", false},
   {"DomainResource/1#2", "DomainResource/1", false},
   {"urn:uuid:cd232626-e29c-49d7-a191-1c797238f48a#1", "urn:uuid:cd232626-e29c-49d7-a191-1c797238f48a#2", false},
   {"urn:uuid:cd232626-e29c-49d7-a191-1c797238f48a", "urn:uuid:cd232626-e29c-49d7-a191-1c797238f48a#1", false}
}));
// clang-format on
