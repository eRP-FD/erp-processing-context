/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/model/Collection.hxx"
#include "fhirtools/model/Element.hxx"
#include "test/fhirtools/DefaultFhirStructureRepository.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>
#include <filesystem>

using namespace fhirtools;

class CollectionTest : public testing::Test
{
public:
    const FhirStructureRepository& repo = DefaultFhirStructureRepository::get();
};

TEST_F(CollectionTest, singleOrEmpty)
{
    {
        Collection c{std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 1)};
        ASSERT_NO_THROW((void) c.singleOrEmpty());
        ASSERT_TRUE(c.singleOrEmpty());
        ASSERT_EQ(c.singleOrEmpty()->type(), Element::Type::Integer);
        ASSERT_EQ(c.singleOrEmpty()->asInt(), 1);
    }
    {
        Collection c{};
        ASSERT_NO_THROW((void) c.singleOrEmpty());
        ASSERT_FALSE(c.singleOrEmpty());
    }
    {
        Collection c{std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 1),
                     std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 1)};
        ASSERT_THROW((void) c.singleOrEmpty(), std::exception);
    }
    {
        Collection c{std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 1),
                     std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "1")};
        ASSERT_THROW((void) c.singleOrEmpty(), std::exception);
    }
}

TEST_F(CollectionTest, boolean)
{
    {
        Collection c{std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 1)};
        ASSERT_NO_THROW((void) c.boolean());
        ASSERT_TRUE(c.boolean());
        ASSERT_EQ(c.boolean()->type(), Element::Type::Boolean);
        ASSERT_EQ(c.singleOrEmpty()->asBool(), true);
    }
    {
        Collection c{std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, false)};
        ASSERT_NO_THROW((void) c.boolean());
        ASSERT_TRUE(c.boolean());
        ASSERT_EQ(c.boolean()->type(), Element::Type::Boolean);
        ASSERT_EQ(c.singleOrEmpty()->asBool(), false);
    }
    {
        Collection c{};
        ASSERT_NO_THROW((void) c.boolean());
        ASSERT_FALSE(c.boolean());
    }
    {
        Collection c{std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, false),
                     std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, false)};
        ASSERT_THROW((void) c.boolean(), std::exception);
    }
    {
        Collection c{std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType(1))};
        ASSERT_NO_THROW((void) c.boolean());
        ASSERT_TRUE(c.boolean()->asBool());
    }
    {
        Collection c{std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType("1.1")),
                     std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 2)};
        ASSERT_THROW((void) c.boolean(), std::exception);
    }
}

TEST_F(CollectionTest, contains)
{
    Collection c;
    Collection c2{std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 1)};
    Collection c3{std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 1),
                  std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType("1.0"))};

    EXPECT_FALSE(c.contains(std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 1)));

    EXPECT_TRUE(c2.contains(std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 1)));
    EXPECT_FALSE(
        c2.contains(std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType("1.0"))));

    EXPECT_TRUE(c3.contains(std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 1)));
    EXPECT_TRUE(
        c3.contains(std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType("1.0"))));
}

TEST_F(CollectionTest, append)
{
    Collection c;
    Collection c2{std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 1)};
    Collection c3{std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 1),
                  std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType("1.0"))};

    c2.append(std::move(c3));
    EXPECT_EQ(c2.size(), 3);
    c.append(std::move(c2));
    ASSERT_EQ(c.size(), 3);
    EXPECT_EQ(c[0]->asInt(), 1);
    EXPECT_EQ(c[1]->asInt(), 1);
    EXPECT_EQ(c[2]->asDecimal(), DecimalType("1.0"));
    c.append({});
    ASSERT_EQ(c.size(), 3);
}

TEST_F(CollectionTest, comparison)
{
    Collection c;
    Collection c2{std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 1)};
    Collection c3{std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 1),
                  std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType("1.0"))};
    Collection c4{std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 1),
                  std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 1)};

    EXPECT_EQ(c.equals(c), std::nullopt);
    EXPECT_EQ(c.equals(c2), std::nullopt);
    EXPECT_EQ(c2.equals(c3), std::optional<bool>(false));
    EXPECT_EQ(c3.equals(c4), std::optional<bool>(true));
}
