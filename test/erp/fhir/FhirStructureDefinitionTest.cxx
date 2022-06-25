/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include <gtest/gtest.h>

#include "erp/fhir/Fhir.hxx"
#include "erp/fhir/FhirStructureDefinition.hxx"

TEST(FhirStructureDefinitionTest, derivationEnum)//NOLINT(readability-function-cognitive-complexity)
{
    using Derivation = FhirStructureDefinition::Derivation;

    // clang-format off
    EXPECT_EQ(stringToDerivation("basetype"      ), Derivation::basetype);
    EXPECT_EQ(stringToDerivation("specialization"), Derivation::specialization);
    EXPECT_EQ(stringToDerivation("constraint"    ), Derivation::constraint);
    EXPECT_ANY_THROW(stringToDerivation("unknown"));

    EXPECT_EQ(to_string(Derivation::basetype      ), "basetype");
    EXPECT_EQ(to_string(Derivation::specialization), "specialization");
    EXPECT_EQ(to_string(Derivation::constraint    ), "constraint");
    // clang-format on
}

TEST(FhirStructureDefinitionTest, kindEnum)//NOLINT(readability-function-cognitive-complexity)
{
    using Kind = FhirStructureDefinition::Kind;

    // clang-format off
    EXPECT_EQ(stringToKind("primitive-type"), Kind::primitiveType);
    EXPECT_EQ(stringToKind("complex-type"  ), Kind::complexType  );
    EXPECT_EQ(stringToKind("resource"      ), Kind::resource     );
    EXPECT_EQ(stringToKind("logical"       ), Kind::logical      );
    EXPECT_EQ(stringToKind("systemBoolean" ), Kind::systemBoolean);
    EXPECT_EQ(stringToKind("systemString"  ), Kind::systemString );
    EXPECT_EQ(stringToKind("systemDouble"  ), Kind::systemDouble );
    EXPECT_EQ(stringToKind("systemInteger" ), Kind::systemInteger);

    EXPECT_EQ(to_string(Kind::primitiveType), "primitive-type");
    EXPECT_EQ(to_string(Kind::complexType  ), "complex-type");
    EXPECT_EQ(to_string(Kind::resource     ), "resource");
    EXPECT_EQ(to_string(Kind::logical      ), "logical");
    EXPECT_EQ(to_string(Kind::systemBoolean), "systemBoolean");
    EXPECT_EQ(to_string(Kind::systemString ), "systemString");
    EXPECT_EQ(to_string(Kind::systemDouble ), "systemDouble");
    EXPECT_EQ(to_string(Kind::systemInteger), "systemInteger");
    // clang-format on
}


TEST(FhirStructureDefinitionTest, Builder)//NOLINT(readability-function-cognitive-complexity)
{
    using Kind = FhirStructureDefinition::Kind;
    using Derivation = FhirStructureDefinition::Derivation;
    using Representation = FhirElement::Representation;
    using namespace std::string_view_literals;

    const std::string typeId = "TestType";
    const std::string url = "http://erp.test.definitions/TestType";
    const std::string baseUrl = "http://erp.test.definitions/TestBase";


    auto rootElement = FhirElement::Builder{}
        .name(typeId)
        .isArray(true)
        .getAndReset();

    auto valueElement = FhirElement::Builder{}
        .name(typeId + ".element")
        .typeId("Coding")
        .isArray(true)
        .representation(FhirElement::Representation::element)
        .getAndReset();

    auto testDefinition = FhirStructureDefinition::Builder{}
        .typeId("TestType")
        .url(url)
        .baseDefinition(baseUrl)
        .kind(FhirStructureDefinition::Kind::resource)
        .derivation(Derivation::specialization)
        .addElement(std::move(rootElement))
        .addElement(std::move(valueElement))
        .getAndReset();

    EXPECT_EQ(testDefinition.url(), url);
    EXPECT_EQ(testDefinition.kind(), Kind::resource);
    EXPECT_EQ(testDefinition.baseDefinition(), baseUrl);
    EXPECT_EQ(testDefinition.derivation(), Derivation::specialization);

    const auto testBaseElement = testDefinition.findElement(typeId);
    ASSERT_TRUE(testBaseElement);
    EXPECT_EQ(testBaseElement->typeId(), typeId);
    EXPECT_EQ(testBaseElement->name(), typeId);
    EXPECT_TRUE(testBaseElement->isRoot());
    EXPECT_TRUE(testBaseElement->isArray());

    const auto testElement = testDefinition.findElement(typeId + ".element");
    ASSERT_TRUE(testElement);
    EXPECT_EQ(testElement->typeId(), "Coding"sv);
    EXPECT_EQ(testElement->name(), typeId + ".element");
    EXPECT_EQ(testElement->representation(), Representation::element);
    EXPECT_FALSE(testElement->isRoot());
    EXPECT_TRUE(testElement->isArray());
}


