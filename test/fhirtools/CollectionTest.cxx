/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/model/Collection.hxx"
#include "fhirtools/model/Element.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "test/fhirtools/DefaultFhirStructureRepository.hxx"
#include "test/util/ResourceManager.hxx"

#include <fhirtools/model/NumberAsStringParserDocument.hxx>
#include <fhirtools/model/erp/ErpElement.hxx>
#include <gtest/gtest.h>
#include <filesystem>

using namespace fhirtools;

class CollectionTest : public testing::Test
{
public:
    const std::shared_ptr<const FhirStructureRepository> repo{DefaultFhirStructureRepository::get()};
    std::shared_ptr<const fhirtools::FhirStructureRepository> mRepo = DefaultFhirStructureRepository::getWithTest();
};

TEST_F(CollectionTest, singleOrEmpty)
{
    {
        Collection c{std::make_shared<PrimitiveElement>(repo, Element::Type::Integer, 1)};
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
        Collection c{std::make_shared<PrimitiveElement>(repo, Element::Type::Integer, 1),
                     std::make_shared<PrimitiveElement>(repo, Element::Type::Integer, 1)};
        ASSERT_THROW((void) c.singleOrEmpty(), std::exception);
    }
    {
        Collection c{std::make_shared<PrimitiveElement>(repo, Element::Type::Integer, 1),
                     std::make_shared<PrimitiveElement>(repo, Element::Type::String, "1")};
        ASSERT_THROW((void) c.singleOrEmpty(), std::exception);
    }
}

TEST_F(CollectionTest, boolean)
{
    {
        Collection c{std::make_shared<PrimitiveElement>(repo, Element::Type::Integer, 1)};
        ASSERT_NO_THROW((void) c.boolean());
        ASSERT_TRUE(c.boolean());
        ASSERT_EQ(c.boolean()->type(), Element::Type::Boolean);
        ASSERT_EQ(c.singleOrEmpty()->asBool(), true);
    }
    {
        Collection c{std::make_shared<PrimitiveElement>(repo, Element::Type::Boolean, false)};
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
        Collection c{std::make_shared<PrimitiveElement>(repo, Element::Type::Boolean, false),
                     std::make_shared<PrimitiveElement>(repo, Element::Type::Boolean, false)};
        ASSERT_THROW((void) c.boolean(), std::exception);
    }
    {
        Collection c{std::make_shared<PrimitiveElement>(repo, Element::Type::Decimal, DecimalType(1))};
        ASSERT_NO_THROW((void) c.boolean());
        ASSERT_TRUE(c.boolean()->asBool());
    }
    {
        Collection c{std::make_shared<PrimitiveElement>(repo, Element::Type::Decimal, DecimalType("1.1")),
                     std::make_shared<PrimitiveElement>(repo, Element::Type::Integer, 2)};
        ASSERT_THROW((void) c.boolean(), std::exception);
    }
}

TEST_F(CollectionTest, contains)
{
    Collection c;
    Collection c2{std::make_shared<PrimitiveElement>(repo, Element::Type::Integer, 1)};
    Collection c3{std::make_shared<PrimitiveElement>(repo, Element::Type::Integer, 1),
                  std::make_shared<PrimitiveElement>(repo, Element::Type::Decimal, DecimalType("1.0"))};

    EXPECT_FALSE(c.contains(std::make_shared<PrimitiveElement>(repo, Element::Type::Integer, 1)));

    EXPECT_TRUE(c2.contains(std::make_shared<PrimitiveElement>(repo, Element::Type::Integer, 1)));
    EXPECT_FALSE(c2.contains(std::make_shared<PrimitiveElement>(repo, Element::Type::Decimal, DecimalType("1.0"))));

    EXPECT_TRUE(c3.contains(std::make_shared<PrimitiveElement>(repo, Element::Type::Integer, 1)));
    EXPECT_TRUE(c3.contains(std::make_shared<PrimitiveElement>(repo, Element::Type::Decimal, DecimalType("1.0"))));
}

TEST_F(CollectionTest, containsNotPrimitiveSubobject1)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        R"({"resourceType": "Test", "_stringPrimitive": {"extension": [{"url": "anyExtension", "valueCode": "unknown"}]}})");
    auto erpElement = std::make_shared<ErpElement>(mRepo, std::weak_ptr<const Element>{}, mRepo->findTypeById("Test"),
                                                   "Test", &testResource);
    Collection c{erpElement};
    bool result = false;
    ASSERT_NO_THROW(result = c.contains(erpElement));
    // a FHIR.string without `value` cannot be converted to `SYSTEM.string` so we are comparing undfined == undefined which is `false`.
    ASSERT_FALSE(result);
}

TEST_F(CollectionTest, containsPrimitiveSubobject2)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        R"({"resourceType": "Test", "stringPrimitive": "bla", "_stringPrimitive": {"extension": [{"url": "anyExtension", "valueCode": "unknown"}]}})");
    auto erpElement = std::make_shared<ErpElement>(mRepo, std::weak_ptr<const Element>{}, mRepo->findTypeById("Test"),
                                                   "Test", &testResource);
    Collection c{erpElement};
    bool result = false;
    ASSERT_NO_THROW(result = c.contains(erpElement));
    ASSERT_TRUE(result);
}

TEST_F(CollectionTest, containsPrimitiveSubobject1)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        R"({"resourceType": "Test", "stringPrimitive": "bla", "_stringPrimitive": {"extension": [{"url": "anyExtension", "valueCode": "unknown"}]}})");
    auto erpElement = std::make_shared<ErpElement>(mRepo, std::weak_ptr<const Element>{}, mRepo->findTypeById("Test"),
                                                   "Test", &testResource);
    Collection c{erpElement};

    auto queryResource =
        model::NumberAsStringParserDocument::fromJson(R"({"resourceType": "Test", "stringPrimitive": "bla"})");
    auto queryElement = std::make_shared<ErpElement>(mRepo, std::weak_ptr<const Element>{}, mRepo->findTypeById("Test"),
                                                     "Test", &queryResource);

    bool result = false;
    ASSERT_NO_THROW(result = c.contains(queryElement));
    ASSERT_TRUE(result);
}

TEST_F(CollectionTest, containsNotPrimitiveSubobject2)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
            R"({"resourceType": "Test", "stringPrimitive": "bla", "_stringPrimitive": {"extension": [{"url": "anyExtension", "valueCode": "unknown"}]}})");
    auto erpElement = std::make_shared<ErpElement>(mRepo, std::weak_ptr<const Element>{}, mRepo->findTypeById("Test"),
                                                   "Test", &testResource);
    Collection c{erpElement};

    auto queryResource = model::NumberAsStringParserDocument::fromJson(
        R"({"resourceType": "Test", "_stringPrimitive": {"extension": [{"url": "anyExtension", "valueCode": "unknown"}]}})");
    auto queryElement = std::make_shared<ErpElement>(mRepo, std::weak_ptr<const Element>{}, mRepo->findTypeById("Test"),
                                                     "Test", &queryResource);

    bool result = false;
    ASSERT_NO_THROW(result = c.contains(queryElement));
    ASSERT_FALSE(result);
}

TEST_F(CollectionTest, containsPrimitiveSubobject3)
{
    auto testResource = model::NumberAsStringParserDocument::fromJson(
        R"({"resourceType": "Test", "stringPrimitive": "bla", "_stringPrimitive": {"extension": [{"url": "anyExtension", "valueCode": "unknown"}]}})");
    auto erpElement = std::make_shared<ErpElement>(mRepo, std::weak_ptr<const Element>{}, mRepo->findTypeById("Test"),
                                                   "Test", &testResource);
    Collection c{erpElement};

    auto queryResource = model::NumberAsStringParserDocument::fromJson(
        R"({"resourceType": "Test", "stringPrimitive": "bla", "_stringPrimitive": {"extension": [{"url": "anyExtension", "valueCode": "KNOWN"}]}})");
    auto queryElement = std::make_shared<ErpElement>(mRepo, std::weak_ptr<const Element>{}, mRepo->findTypeById("Test"),
                                                     "Test", &queryResource);

    bool result = false;
    ASSERT_NO_THROW(result = c.contains(queryElement));
    ASSERT_TRUE(result);
}

TEST_F(CollectionTest, append)
{
    Collection c;
    Collection c2{std::make_shared<PrimitiveElement>(repo, Element::Type::Integer, 1)};
    Collection c3{std::make_shared<PrimitiveElement>(repo, Element::Type::Integer, 1),
                  std::make_shared<PrimitiveElement>(repo, Element::Type::Decimal, DecimalType("1.0"))};

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
    Collection c2{std::make_shared<PrimitiveElement>(repo, Element::Type::Integer, 1)};
    Collection c3{std::make_shared<PrimitiveElement>(repo, Element::Type::Integer, 1),
                  std::make_shared<PrimitiveElement>(repo, Element::Type::Decimal, DecimalType("1.0"))};
    Collection c4{std::make_shared<PrimitiveElement>(repo, Element::Type::Integer, 1),
                  std::make_shared<PrimitiveElement>(repo, Element::Type::Integer, 1)};

    EXPECT_EQ(c.equals(c), std::nullopt);
    EXPECT_EQ(c.equals(c2), std::nullopt);
    EXPECT_EQ(c2.equals(c3), std::optional<bool>(false));
    EXPECT_EQ(c3.equals(c4), std::optional<bool>(true));
}
