/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include <gtest/gtest.h>

#include "fhirtools/repository/FhirElement.hxx"

using namespace fhirtools;

TEST(FhirElementTest, representationEnum)//NOLINT(readability-function-cognitive-complexity)
{
    using Representation = FhirElement::Representation;
    // clang-format off
    EXPECT_EQ(stringToRepresentation("xmlAttr" ), Representation::xmlAttr );
    EXPECT_EQ(stringToRepresentation("xmlText" ), Representation::xmlText );
    EXPECT_EQ(stringToRepresentation("typeAttr"), Representation::typeAttr);
    EXPECT_EQ(stringToRepresentation("cdaText" ), Representation::cdaText );
    EXPECT_EQ(stringToRepresentation("xhtml"   ), Representation::xhtml   );
    EXPECT_EQ(stringToRepresentation("element" ), Representation::element );
    EXPECT_ANY_THROW(stringToRepresentation("unknown"));

    EXPECT_EQ(to_string(Representation::xmlAttr ), "xmlAttr" );
    EXPECT_EQ(to_string(Representation::xmlText ), "xmlText" );
    EXPECT_EQ(to_string(Representation::typeAttr), "typeAttr");
    EXPECT_EQ(to_string(Representation::cdaText ), "cdaText" );
    EXPECT_EQ(to_string(Representation::xhtml   ), "xhtml"   );
    EXPECT_EQ(to_string(Representation::element ), "element" );
    // clang-format on
}

TEST(FhirElementTest, Builder)//NOLINT(readability-function-cognitive-complexity)
{
    using namespace std::string_view_literals;
    using Representation = FhirElement::Representation;
    auto testElement1 = FhirElement::Builder{}
        .name("name")
        .typeId("text")
        .isArray(true)
        .representation(Representation::xhtml)
        .contentReference("contentReference")
        .getAndReset();
    ASSERT_TRUE(testElement1);
    EXPECT_EQ(testElement1->name(), "name"sv);
    EXPECT_EQ(testElement1->typeId(), "text"sv);
    EXPECT_EQ(testElement1->contentReference(), "contentReference"sv);
    EXPECT_TRUE(testElement1->isArray());
    EXPECT_EQ(testElement1->representation(), Representation::xhtml);
    EXPECT_EQ(testElement1->isRoot(), false);

    auto testElement2 = FhirElement::Builder{*testElement1}.name("otherName").getAndReset();
    ASSERT_TRUE(testElement2);
    EXPECT_EQ(testElement2->name(), "otherName"sv);
    EXPECT_EQ(testElement2->typeId(), "text"sv);
    EXPECT_TRUE(testElement2->isArray());
    EXPECT_EQ(testElement2->representation(), Representation::xhtml);
    EXPECT_EQ(testElement2->isRoot(), false);
}

TEST(FhirElementTest, RootElement)
{
    using namespace std::string_view_literals;
    using Representation = FhirElement::Representation;
    auto testElement1 = FhirElement::Builder{}
        .name("Meta")
        .isArray(true)
        .isRoot(true)
        .representation(Representation::xhtml)
        .getAndReset();
    ASSERT_TRUE(testElement1);
    EXPECT_EQ(testElement1->name(), "Meta"sv);
    EXPECT_EQ(testElement1->typeId(), "Meta"sv);
    EXPECT_TRUE(testElement1->isArray());
    EXPECT_EQ(testElement1->representation(), Representation::xhtml);
    EXPECT_EQ(testElement1->isRoot(), true);
}
