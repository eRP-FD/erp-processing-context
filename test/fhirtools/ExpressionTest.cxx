/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/expression/BooleanLogic.hxx"
#include "fhirtools/expression/Comparison.hxx"
#include "fhirtools/expression/Conversion.hxx"
#include "fhirtools/expression/Expression.hxx"
#include "fhirtools/expression/Functions.hxx"
#include "fhirtools/expression/LiteralExpression.hxx"
#include "fhirtools/expression/Math.hxx"
#include "fhirtools/expression/StringManipulation.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "test/fhirtools/DefaultFhirStructureRepository.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>
#include <filesystem>

using namespace fhirtools;
using namespace std::string_literals;

class ExpressionTest : public testing::Test
{
public:
    void SetUp() override
    {
        testResource = model::NumberAsStringParserDocument::fromJson(
            ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));

        const auto* testDef = repo.findTypeById("Test");
        ASSERT_NE(testDef, nullptr);
        rootElement = std::make_shared<ErpElement>(&repo, std::weak_ptr<Element>{}, testDef , "Test", &testResource);
    }
    const FhirStructureRepository& repo = DefaultFhirStructureRepository::getWithTest();
    model::NumberAsStringParserDocument testResource;
    std::shared_ptr<ErpElement> rootElement;

protected:
    void pathSelectionTest(const std::string& selector, std::vector<PrimitiveElement> expected)
    {
        PathSelection pathSelection(&repo, selector);
        auto result = pathSelection.eval(Collection{rootElement});
        EXPECT_EQ(expected.size(), result.size());
        for (size_t i = 0, end = expected.size(); i < end; ++i)
        {
            ASSERT_EQ(expected[i].type(), result[i]->type());
            switch (expected[i].type())
            {

                case Element::Type::Integer:
                    EXPECT_EQ(expected[i].asInt(), result[i]->asInt());
                    break;
                case Element::Type::Decimal:
                    EXPECT_EQ((expected[i].asDecimal()), (result[i]->asDecimal()));
                    break;
                case Element::Type::String:
                    EXPECT_EQ(expected[i].asString(), result[i]->asString());
                    break;
                case Element::Type::Boolean:
                    EXPECT_EQ(expected[i].asBool(), result[i]->asBool());
                    break;
                case Element::Type::Structured:
                    FAIL();
                    break;
                case Element::Type::Date:
                    EXPECT_EQ(expected[i].asDate().compareTo(result[i]->asDate()), std::strong_ordering::equal);
                    break;
                case Element::Type::DateTime:
                    EXPECT_EQ(expected[i].asDateTime().compareTo(result[i]->asDateTime()), std::strong_ordering::equal);
                    break;
                case Element::Type::Time:
                    EXPECT_EQ(expected[i].asTime().compareTo(result[i]->asTime()), std::strong_ordering::equal);
                    break;
                case Element::Type::Quantity:
                    EXPECT_EQ(expected[i].asQuantity().compareTo(result[i]->asQuantity()), std::strong_ordering::equal);
                    break;
            }
        }
    }

    template<class BooleanOperator>
    void BooleanOperatorTest(std::optional<bool> lhs, std::optional<bool> rhs, std::optional<bool> expectedResult)
    {
        BooleanOperator booleanImpliesOperator(
            &repo, lhs.has_value() ? std::make_shared<LiteralBooleanExpression>(&repo, lhs.value()) : nullptr,
            rhs.has_value() ? std::make_shared<LiteralBooleanExpression>(&repo, rhs.value()) : nullptr);
        const auto result = booleanImpliesOperator.eval({});
        if (expectedResult.has_value())
        {
            checkBoolResult(result, expectedResult.value());
        }
        else
        {
            EXPECT_TRUE(result.empty());
        }
    }

    template<class StringConcatOperator>
    void StringConcatenationTest(std::optional<std::string> s1, std::optional<std::string> s2,
                                 std::optional<std::string> expectedResult)
    {
        StringConcatOperator op(&repo, s1 ? std::make_shared<LiteralStringExpression>(&repo, s1.value()) : nullptr,
                                s2 ? std::make_shared<LiteralStringExpression>(&repo, s2.value()) : nullptr);
        const auto result = op.eval({});
        if (expectedResult.has_value())
        {
            ASSERT_EQ(result.size(), 1);
            ASSERT_EQ(result.single()->type(), Element::Type::String);
            EXPECT_EQ(result.single()->asString(), expectedResult.value());
        }
        else
        {
            EXPECT_TRUE(result.empty());
        }
    }

    void checkBoolResult(const Collection& result, bool expected)
    {
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result[0]->type(), Element::Type::Boolean);
        ASSERT_EQ(result[0]->asBool(), expected);
    }

    void checkIntResult(const Collection& result, int32_t expected)
    {
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result[0]->type(), Element::Type::Integer);
        ASSERT_EQ(result[0]->asInt(), expected);
    }
};

TEST_F(ExpressionTest, PathSelection)
{
    using namespace std::string_literals;

    pathSelectionTest("string", {PrimitiveElement{&repo, Element::Type::String, "value"s}});
    pathSelectionTest("multiString", {PrimitiveElement{&repo, Element::Type::String, "value"s},
                                      PrimitiveElement{&repo, Element::Type::String, "value2"s}});
    pathSelectionTest("num", {PrimitiveElement{&repo, Element::Type::Integer, 12}});
    pathSelectionTest("multiNum", {PrimitiveElement{&repo, Element::Type::Integer, 1},
                                   PrimitiveElement{&repo, Element::Type::Integer, 5},
                                   PrimitiveElement{&repo, Element::Type::Integer, 42}});
    pathSelectionTest("dec", {PrimitiveElement{&repo, Element::Type::Decimal, DecimalType("1.2")}});
    pathSelectionTest("multiDec", {PrimitiveElement{&repo, Element::Type::Decimal, DecimalType("0.1")},
                                   PrimitiveElement{&repo, Element::Type::Decimal, DecimalType("3.14")}});
    pathSelectionTest("boolean", {PrimitiveElement{&repo, Element::Type::Boolean, true}});
    pathSelectionTest("multibool", {PrimitiveElement{&repo, Element::Type::Boolean, true},
                                    PrimitiveElement{&repo, Element::Type::Boolean, false},
                                    PrimitiveElement{&repo, Element::Type::Boolean, false},
                                    PrimitiveElement{&repo, Element::Type::Boolean, false}});
}

TEST_F(ExpressionTest, ExistenceEmpty)
{
    ExistenceEmpty existenceEmptyFunction(&repo);
    auto trueResult = existenceEmptyFunction.eval({});
    checkBoolResult(trueResult, true);

    auto falseResult =
        existenceEmptyFunction.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 1),
                                     std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 3)});
    checkBoolResult(falseResult, false);
}

TEST_F(ExpressionTest, ExistenceExists)
{
    {
        ExistenceExists existenceExistsFunction(&repo, {});
        const auto result = existenceExistsFunction.eval({rootElement});
        checkBoolResult(result, true);
    }
    {
        ExistenceExists existenceExistsFunction(
            &repo, std::make_shared<EqualityEqualsOperator>(&repo, std::make_shared<PathSelection>(&repo, "string"),
                                                            std::make_shared<LiteralStringExpression>(&repo, "value")));
        const auto result = existenceExistsFunction.eval({rootElement});
        checkBoolResult(result, true);
    }
    {
        ExistenceExists existenceExistsFunction(
            &repo, std::make_shared<EqualityEqualsOperator>(&repo, std::make_shared<PathSelection>(&repo, "string"),
                                                            std::make_shared<LiteralStringExpression>(&repo, "FOO")));
        const auto result = existenceExistsFunction.eval({rootElement});
        checkBoolResult(result, false);
    }
}

TEST_F(ExpressionTest, ExistenceFunctions)
{
    ExistenceAll existenceAllFunction(
        &repo, std::make_shared<EqualityEqualsOperator>(&repo, std::make_shared<DollarThis>(&repo),
                                                        std::make_shared<LiteralBooleanExpression>(&repo, true)));
    ExistenceAllTrue existenceAllTrueFunction(&repo);
    ExistenceAnyTrue existenceAnyTrueFunction(&repo);
    ExistenceAllFalse existenceAllFalseFunction(&repo);
    ExistenceAnyFalse existenceAnyFalseFunction(&repo);

    const Collection input1{std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, true),
                            std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, true),
                            std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, true)};
    const Collection input2{std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, false),
                            std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, false),
                            std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, true)};
    const Collection input3{std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, false),
                            std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, false),
                            std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, false)};
    const Collection input4{};

    checkBoolResult(existenceAllFunction.eval(input1), true);
    checkBoolResult(existenceAllFunction.eval(input2), false);
    checkBoolResult(existenceAllFunction.eval(input3), false);
    checkBoolResult(existenceAllFunction.eval(input4), true);
    checkBoolResult(existenceAllTrueFunction.eval(input1), true);
    checkBoolResult(existenceAllTrueFunction.eval(input2), false);
    checkBoolResult(existenceAllTrueFunction.eval(input3), false);
    checkBoolResult(existenceAllTrueFunction.eval(input4), true);
    checkBoolResult(existenceAnyTrueFunction.eval(input1), true);
    checkBoolResult(existenceAnyTrueFunction.eval(input2), true);
    checkBoolResult(existenceAnyTrueFunction.eval(input3), false);
    checkBoolResult(existenceAnyTrueFunction.eval(input4), false);
    checkBoolResult(existenceAllFalseFunction.eval(input1), false);
    checkBoolResult(existenceAllFalseFunction.eval(input2), false);
    checkBoolResult(existenceAllFalseFunction.eval(input3), true);
    checkBoolResult(existenceAllFalseFunction.eval(input4), true);
    checkBoolResult(existenceAnyFalseFunction.eval(input1), false);
    checkBoolResult(existenceAnyFalseFunction.eval(input2), true);
    checkBoolResult(existenceAnyFalseFunction.eval(input3), true);
    checkBoolResult(existenceAnyFalseFunction.eval(input4), false);
}

TEST_F(ExpressionTest, ExistenceCount)
{
    ExistenceCount existenceCountFunction(&repo);
    EXPECT_EQ(existenceCountFunction.eval({}).singleOrEmpty()->asInt(), 0);
    EXPECT_EQ(existenceCountFunction.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, true)})
                  .singleOrEmpty()
                  ->asInt(),
              1);
    EXPECT_EQ(existenceCountFunction.eval({rootElement}).singleOrEmpty()->asInt(), 1);
    EXPECT_EQ(existenceCountFunction
                  .eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, true),
                         std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, true)})
                  .singleOrEmpty()
                  ->asInt(),
              2);
}

TEST_F(ExpressionTest, ExistenceDistinct)
{
    ExistenceDistinct existenceDistinctFunction(&repo);


    const Collection input1{};
    const Collection input2{std::make_shared<PrimitiveElement>(&repo, Element::Type::String, ""s),
                            std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "1"s)};
    const Collection input3{std::make_shared<PrimitiveElement>(&repo, Element::Type::String, ""s),
                            std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "1"s),
                            std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "1"s),
                            std::make_shared<PrimitiveElement>(&repo, Element::Type::String, ""s)};
    const Collection input4{
        std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 11),
        std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType(11)),
        std::make_shared<PrimitiveElement>(&repo, Element::Type::Quantity, Element::QuantityType(11, {}))};

    {
        const auto result = existenceDistinctFunction.eval(input1);
        EXPECT_TRUE(result.empty());
    }
    {
        const auto result = existenceDistinctFunction.eval({input2});
        EXPECT_EQ(result.size(), 2);
    }
    {
        const auto result = existenceDistinctFunction.eval(input3);
        EXPECT_EQ(result.size(), 2);
    }
    {
        const auto result = existenceDistinctFunction.eval(input4);
        EXPECT_EQ(result.size(), 3);
    }

    ExistenceIsDistinct existenceIsDistinctFunction(&repo);
    checkBoolResult(existenceIsDistinctFunction.eval(input1), true);
    checkBoolResult(existenceIsDistinctFunction.eval(input2), true);
    checkBoolResult(existenceIsDistinctFunction.eval(input3), false);
    checkBoolResult(existenceIsDistinctFunction.eval(input4), true);
}

TEST_F(ExpressionTest, FilteringWhere)
{
    {
        FilteringWhere filteringWhere(
            &repo, std::make_shared<EqualityEqualsOperator>(&repo, std::make_shared<PathSelection>(&repo, "num"),
                                                            std::make_shared<LiteralIntegerExpression>(&repo, 12)));
        auto result = filteringWhere.eval({rootElement});
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result[0]->type(), Element::Type::Structured);
    }
    {
        FilteringWhere filteringWhere(&repo,
                                      std::make_shared<EqualityEqualsOperator>(
                                          &repo, std::make_shared<PathSelection>(&repo, "num"),
                                          std::make_shared<LiteralDecimalExpression>(&repo, DecimalType("12.0"))));
        auto result = filteringWhere.eval({rootElement});
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result[0]->type(), Element::Type::Structured);
    }
    {
        FilteringWhere filteringWhere(&repo,
                                      std::make_shared<EqualityEqualsOperator>(
                                          &repo, std::make_shared<PathSelection>(&repo, "num"),
                                          std::make_shared<LiteralDecimalExpression>(&repo, DecimalType("12.1"))));
        auto result = filteringWhere.eval({rootElement});
        ASSERT_EQ(result.size(), 0);
    }
    {
        FilteringWhere filteringWhere(
            &repo, std::make_shared<EqualityEqualsOperator>(&repo, std::make_shared<PathSelection>(&repo, "num"),
                                                            std::make_shared<LiteralIntegerExpression>(&repo, 0)));
        auto result = filteringWhere.eval({rootElement});
        ASSERT_EQ(result.size(), 0);
    }
    {
        FilteringWhere filteringWhere(&repo, std::make_shared<PathSelection>(&repo, "num"));
        auto result = filteringWhere.eval({rootElement});
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result[0]->type(), Element::Type::Structured);
    }
}

TEST_F(ExpressionTest, FilteringSelect)
{
    FilteringSelect filteringSelectFunction(&repo, std::make_shared<PathSelection>(&repo, "num"));
    auto result = filteringSelectFunction.eval({rootElement});
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0]->type(), Element::Type::Integer);
    EXPECT_EQ(result[0]->asInt(), 12);
}

TEST_F(ExpressionTest, EqualityEqualsOperator)
{
    {
        EqualityEqualsOperator equalsOperator(&repo, std::make_shared<LiteralIntegerExpression>(&repo, 33),
                                              std::make_shared<LiteralDecimalExpression>(&repo, 33));
        auto result = equalsOperator.eval({});
        checkBoolResult(result, true);
    }
    {
        EqualityEqualsOperator equalsOperator(&repo, std::make_shared<DollarThis>(&repo),
                                              std::make_shared<DollarThis>(&repo));
        auto result = equalsOperator.eval({rootElement});
        checkBoolResult(result, true);
    }
    {
        EqualityEqualsOperator equalsOperator(&repo, std::make_shared<DollarThis>(&repo),
                                              std::make_shared<PathSelection>(&repo, "quantity"));
        auto result = equalsOperator.eval({rootElement});
        checkBoolResult(result, false);
    }
}

TEST_F(ExpressionTest, EqualityNotEqualsOperator)
{
    {
        EqualityNotEqualsOperator notEqualsOperator(&repo, std::make_shared<LiteralIntegerExpression>(&repo, 33),
                                                    std::make_shared<LiteralDecimalExpression>(&repo, 33));
        auto result = notEqualsOperator.eval({});
        checkBoolResult(result, false);
    }
    {
        EqualityNotEqualsOperator notEqualsOperator(&repo, std::make_shared<DollarThis>(&repo),
                                                    std::make_shared<DollarThis>(&repo));
        auto result = notEqualsOperator.eval({rootElement});
        checkBoolResult(result, false);
    }
    {
        EqualityNotEqualsOperator notEqualsOperator(&repo, std::make_shared<DollarThis>(&repo),
                                                    std::make_shared<PathSelection>(&repo, "quantity"));
        auto result = notEqualsOperator.eval({rootElement});
        checkBoolResult(result, true);
    }
}

TEST_F(ExpressionTest, SubsettingIndexer)
{
    {
        SubsettingIndexer subsettingIndexerFunction(&repo, std::make_shared<DollarThis>(&repo),
                                                    std::make_shared<LiteralIntegerExpression>(&repo, 1));
        const auto result = subsettingIndexerFunction.eval(
            {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "foo"),
             std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType(5))});
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result.singleOrEmpty()->type(), Element::Type::Decimal);
        EXPECT_EQ(result.singleOrEmpty()->asDecimal(), 5);
    }
    {
        SubsettingIndexer subsettingIndexerFunction(&repo, std::make_shared<DollarThis>(&repo),
                                                    std::make_shared<LiteralIntegerExpression>(&repo, 2));
        const auto result = subsettingIndexerFunction.eval(
            {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "foo"),
             std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType(5))});
        ASSERT_EQ(result.size(), 0);
    }
}

TEST_F(ExpressionTest, SubsettingFirst)
{
    SubsettingFirst subsettingFirst(&repo);
    const auto result =
        subsettingFirst.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "foo"),
                              std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType(5))});
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result.singleOrEmpty()->type(), Element::Type::String);
    EXPECT_EQ(result.singleOrEmpty()->asString(), "foo");
}

TEST_F(ExpressionTest, SubsettingTail)
{
    SubsettingTail subsettingTail(&repo);
    const auto result =
        subsettingTail.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "foo"),
                             std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType(5)),
                             std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "bar")});
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0]->type(), Element::Type::Decimal);
    EXPECT_EQ(result[0]->asDecimal(), 5);
    EXPECT_EQ(result[1]->type(), Element::Type::String);
    EXPECT_EQ(result[1]->asString(), "bar");
}

TEST_F(ExpressionTest, SubsettingIntersect)
{
    {
        SubsettingIntersect subsettingIntersect(&repo, std::make_shared<DollarThis>(&repo));
        const auto result = subsettingIntersect.eval(
            {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "foo"),
             std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType(5)),
             std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType(5))});
        ASSERT_EQ(result.size(), 2);
        ASSERT_EQ(result[0]->type(), Element::Type::String);
        ASSERT_EQ(result[0]->asString(), "foo");
        ASSERT_EQ(result[1]->type(), Element::Type::Decimal);
        ASSERT_EQ(result[1]->asDecimal(), 5);
    }
    {
        SubsettingIntersect subsettingIntersect(&repo,
                                                std::make_shared<LiteralDecimalExpression>(&repo, DecimalType(5)));
        const auto result = subsettingIntersect.eval(
            {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "foo"),
             std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType(5)),
             std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType(5))});
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result[0]->type(), Element::Type::Decimal);
        ASSERT_EQ(result[0]->asDecimal(), 5);
    }
}

TEST_F(ExpressionTest, FilteringOfType)
{
    {
        FilteringOfType filteringOfTypeFunction(
            &repo, std::make_shared<LiteralStringExpression>(&repo, "http://hl7.org/fhirpath/System.String"));
        const auto result =
            filteringOfTypeFunction.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "foo")});
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result[0]->type(), Element::Type::String);
    }
    {
        FilteringOfType filteringOfTypeFunction(
            &repo, std::make_shared<LiteralStringExpression>(&repo, "http://hl7.org/fhirpath/System.String"));
        const auto result =
            filteringOfTypeFunction.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, true),
                                          std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "foo"),
                                          std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 1),
                                          std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "bar"),
                                          std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, true)});
        ASSERT_EQ(result.size(), 2);
        EXPECT_EQ(result[0]->type(), Element::Type::String);
        EXPECT_EQ(result[1]->type(), Element::Type::String);
    }
    {
        FilteringOfType filteringOfTypeFunction(
            &repo, std::make_shared<LiteralStringExpression>(&repo, "string"));
        const auto result =
            filteringOfTypeFunction.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "foo")});
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result[0]->type(), Element::Type::String);
    }
}

TEST_F(ExpressionTest, ConversionIif)
{
    {
        ConversionIif conversionIifFunction(
            &repo,
            std::make_shared<EqualityEqualsOperator>(&repo, std::make_shared<LiteralIntegerExpression>(&repo, 1),
                                                     std::make_shared<DollarThis>(&repo)),
            {std::make_shared<LiteralStringExpression>(&repo, "trueResult")}, {});

        {
            const auto result =
                conversionIifFunction.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 1)});
            ASSERT_EQ(result.size(), 1);
            EXPECT_EQ(result[0]->type(), Element::Type::String);
            EXPECT_EQ(result[0]->asString(), "trueResult");
        }
        {
            const auto result = conversionIifFunction.eval({});
            ASSERT_EQ(result.size(), 0);
        }
    }
    {
        ConversionIif conversionIifFunction(
            &repo,
            std::make_shared<EqualityEqualsOperator>(&repo, std::make_shared<LiteralIntegerExpression>(&repo, 1),
                                                     std::make_shared<DollarThis>(&repo)),
            {std::make_shared<LiteralStringExpression>(&repo, "trueResult")},
            {std::make_shared<LiteralStringExpression>(&repo, "falseResult")});
        {
            const auto result =
                conversionIifFunction.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 1)});
            ASSERT_EQ(result.size(), 1);
            EXPECT_EQ(result[0]->type(), Element::Type::String);
            EXPECT_EQ(result[0]->asString(), "trueResult");
        }
        {
            const auto result = conversionIifFunction.eval({});
            ASSERT_EQ(result.size(), 1);
            EXPECT_EQ(result[0]->type(), Element::Type::String);
            EXPECT_EQ(result[0]->asString(), "falseResult");
        }
    }
}

TEST_F(ExpressionTest, StringManipIndexOf)
{
    StringManipIndexOf stringManipIndexOf(&repo, std::make_shared<LiteralStringExpression>(&repo, "substring"));
    const auto result1 = stringManipIndexOf.eval(
        {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "contains the substring somewhere"s)});
    const auto result2 = stringManipIndexOf.eval(
        {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "does not contain it."s)});
    ASSERT_EQ(result1.size(), 1);
    EXPECT_EQ(result1[0]->type(), Element::Type::Integer);
    EXPECT_EQ(result1[0]->asInt(), 13);
    ASSERT_EQ(result2.size(), 1);
    EXPECT_EQ(result2[0]->type(), Element::Type::Integer);
    EXPECT_EQ(result2[0]->asInt(), -1);
}

TEST_F(ExpressionTest, TreeNavigationChildren)
{
    TreeNavigationChildren treeNavigationChildren(&repo);
    {
        const auto result = treeNavigationChildren.eval({rootElement});
        ASSERT_EQ(result.size(), 22);
    }
    {
        const auto result = treeNavigationChildren.eval({});
        ASSERT_EQ(result.size(), 0);
    }
    {
        const auto result =
            treeNavigationChildren.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "")});
        ASSERT_EQ(result.size(), 0);
    }
}

TEST_F(ExpressionTest, TreeNavigationDescendants)
{
    TreeNavigationDescendants treeNavigationDescendants(&repo);
    {
        const auto result = treeNavigationDescendants.eval({rootElement});
        ASSERT_EQ(result.size(), 24);
    }
    {
        const auto result = treeNavigationDescendants.eval({});
        ASSERT_EQ(result.size(), 0);
    }
    {
        const auto result =
            treeNavigationDescendants.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "")});
        ASSERT_EQ(result.size(), 0);
    }
}

TEST_F(ExpressionTest, TypesIsOperator)
{
    TypesIsOperator typesIsOperator(&repo, std::make_shared<DollarThis>(&repo),
                                    "http://hl7.org/fhirpath/System.Decimal");
    const auto result1 =
        typesIsOperator.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType("3.14"))});
    ASSERT_EQ(result1.size(), 1);
    EXPECT_EQ(result1.singleOrEmpty()->asBool(), true);
    const auto result2 = typesIsOperator.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 3)});
    ASSERT_EQ(result2.size(), 1);
    EXPECT_EQ(result2.singleOrEmpty()->asBool(), false);
}

TEST_F(ExpressionTest, TypeAsOperator)
{
    TypeAsOperator typesAsOperator(&repo, std::make_shared<DollarThis>(&repo),
                                   "http://hl7.org/fhirpath/System.Decimal");
    const auto result1 =
        typesAsOperator.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType("3.14"))});
    ASSERT_EQ(result1.size(), 1);
    EXPECT_EQ(result1.singleOrEmpty()->type(), Element::Type::Decimal);
    EXPECT_EQ(result1.singleOrEmpty()->asDecimal(), DecimalType("3.14"));
    const auto result2 = typesAsOperator.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 3)});
    ASSERT_TRUE(result2.empty());
}

TEST_F(ExpressionTest, BooleanAndOperator)
{
    BooleanOperatorTest<BooleanAndOperator>(true, true, true);
    BooleanOperatorTest<BooleanAndOperator>(true, false, false);
    BooleanOperatorTest<BooleanAndOperator>(true, {}, {});
    BooleanOperatorTest<BooleanAndOperator>(false, true, false);
    BooleanOperatorTest<BooleanAndOperator>(false, false, false);
    BooleanOperatorTest<BooleanAndOperator>(false, {}, false);
    BooleanOperatorTest<BooleanAndOperator>({}, true, {});
    BooleanOperatorTest<BooleanAndOperator>({}, false, false);
    BooleanOperatorTest<BooleanAndOperator>({}, {}, {});
}

TEST_F(ExpressionTest, BooleanOrOperator)
{
    BooleanOperatorTest<BooleanOrOperator>(true, true, true);
    BooleanOperatorTest<BooleanOrOperator>(true, false, true);
    BooleanOperatorTest<BooleanOrOperator>(true, {}, true);
    BooleanOperatorTest<BooleanOrOperator>(false, true, true);
    BooleanOperatorTest<BooleanOrOperator>(false, false, false);
    BooleanOperatorTest<BooleanOrOperator>(false, {}, {});
    BooleanOperatorTest<BooleanOrOperator>({}, true, true);
    BooleanOperatorTest<BooleanOrOperator>({}, false, {});
    BooleanOperatorTest<BooleanOrOperator>({}, {}, {});
}

TEST_F(ExpressionTest, BooleanNotOperator)
{
    {
        BooleanNotOperator booleanNotOperator(&repo);
        const auto result = booleanNotOperator.eval({});
        EXPECT_TRUE(result.empty());
    }
    {
        BooleanNotOperator booleanNotOperator(&repo);
        const auto result =
            booleanNotOperator.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, true)});
        checkBoolResult(result, false);
    }
    {
        BooleanNotOperator booleanNotOperator(&repo);
        const auto result =
            booleanNotOperator.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, false)});
        checkBoolResult(result, true);
    }
}

TEST_F(ExpressionTest, BooleanXorOperator)
{
    BooleanOperatorTest<BooleanXorOperator>(true, true, false);
    BooleanOperatorTest<BooleanXorOperator>(true, false, true);
    BooleanOperatorTest<BooleanXorOperator>(true, {}, {});
    BooleanOperatorTest<BooleanXorOperator>(false, true, true);
    BooleanOperatorTest<BooleanXorOperator>(false, false, false);
    BooleanOperatorTest<BooleanXorOperator>(false, {}, {});
    BooleanOperatorTest<BooleanXorOperator>({}, true, {});
    BooleanOperatorTest<BooleanXorOperator>({}, false, {});
    BooleanOperatorTest<BooleanXorOperator>({}, {}, {});
}


TEST_F(ExpressionTest, BooleanImpliesOperator)
{
    BooleanOperatorTest<BooleanImpliesOperator>(true, true, true);
    BooleanOperatorTest<BooleanImpliesOperator>(true, false, false);
    BooleanOperatorTest<BooleanImpliesOperator>(true, {}, {});
    BooleanOperatorTest<BooleanImpliesOperator>(false, true, true);
    BooleanOperatorTest<BooleanImpliesOperator>(false, false, true);
    BooleanOperatorTest<BooleanImpliesOperator>(false, {}, true);
    BooleanOperatorTest<BooleanImpliesOperator>({}, true, true);
    BooleanOperatorTest<BooleanImpliesOperator>({}, false, {});
    BooleanOperatorTest<BooleanImpliesOperator>({}, {}, {});
}

TEST_F(ExpressionTest, ConversionToString)
{
    ConversionToString conversionToString(&repo);
    const auto result1 =
        conversionToString.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, false)}).single();
    EXPECT_EQ(result1->type(), Element::Type::String);
    EXPECT_EQ(result1->asString(), "false");

    const auto result2 =
        conversionToString
            .eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType("3.14"))})
            .single();
    EXPECT_EQ(result2->type(), Element::Type::String);
    EXPECT_EQ(result2->asString(), "3.14");

    EXPECT_THROW(conversionToString.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, false),
                                          std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, false)}),
                 std::exception);

    const auto result3 = conversionToString.eval({rootElement}).single();
    EXPECT_EQ(result3->type(), Element::Type::Boolean);
    EXPECT_EQ(result3->asBool(), false);
}

TEST_F(ExpressionTest, ConversionToInt)
{
    ConversionToInteger conversionToInteger(&repo);
    {
        const auto result =
            conversionToInteger.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, false)});
        checkIntResult(result, 0);
    }
    {
        const auto result =
            conversionToInteger.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, true)});
        checkIntResult(result, 1);
    }
    {
        const auto result =
            conversionToInteger.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "1234"s)});
        checkIntResult(result, 1234);
    }
    {
        const auto result =
            conversionToInteger.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "1234x"s)});
        ASSERT_TRUE(result.empty());
    }
    {
        const auto result =
            conversionToInteger.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 321)});
        checkIntResult(result, 321);
    }
    {
        const auto result = conversionToInteger.eval(
            {std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType(123))});
        ASSERT_TRUE(result.empty());
    }
    {
        const auto result =
            conversionToInteger.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "2147483648")});
        ASSERT_TRUE(result.empty());
    }
    {
        const auto result =
            conversionToInteger.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "-2147483649")});
        ASSERT_TRUE(result.empty());
    }
}

TEST_F(ExpressionTest, StringManipSubstring)
{
    {
        StringManipSubstring stringManipSubstring(&repo, std::make_shared<LiteralIntegerExpression>(&repo, 5), {});
        {
            const auto result = stringManipSubstring.eval(
                {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "hello world"s)});
            EXPECT_EQ(result.single()->type(), Element::Type::String);
            EXPECT_EQ(result.single()->asString(), " world");
        }
        {
            const auto result =
                stringManipSubstring.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "hello"s)});
            EXPECT_TRUE(result.empty());
        }
        {
            const auto result = stringManipSubstring.eval({});
            EXPECT_TRUE(result.empty());
        }
    }
    {
        StringManipSubstring stringManipSubstring(&repo, std::make_shared<LiteralIntegerExpression>(&repo, 5),
                                                  std::make_shared<LiteralIntegerExpression>(&repo, 1));
        {
            const auto result = stringManipSubstring.eval(
                {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "hello world"s)});
            EXPECT_EQ(result.single()->type(), Element::Type::String);
            EXPECT_EQ(result.single()->asString(), " ");
        }
        {
            const auto result =
                stringManipSubstring.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "hello"s)});
            EXPECT_TRUE(result.empty());
        }
    }
    {
        StringManipSubstring stringManipSubstring(&repo, {}, {});
        {
            const auto result = stringManipSubstring.eval(
                {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "hello world"s)});
            EXPECT_TRUE(result.empty());
        }
    }
}

TEST_F(ExpressionTest, StringManipStartsWith)
{
    {
        StringManipStartsWith stringManipStartsWith(&repo, std::make_shared<LiteralStringExpression>(&repo, "hello"s));
        {
            const auto result = stringManipStartsWith.eval(
                {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "hello world"s)});
            checkBoolResult(result, true);
        }
        {
            const auto result = stringManipStartsWith.eval(
                {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "moin world"s)});
            checkBoolResult(result, false);
        }
        {
            const auto result = stringManipStartsWith.eval({});
            ASSERT_TRUE(result.empty());
        }
    }
    {
        StringManipStartsWith stringManipStartsWith(&repo, std::make_shared<LiteralStringExpression>(&repo, ""s));
        const auto result = stringManipStartsWith.eval(
            {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "moin world"s)});
        checkBoolResult(result, true);
    }
    {
        StringManipStartsWith stringManipStartsWith(&repo, {});
        const auto result = stringManipStartsWith.eval(
            {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "moin world"s)});
        ASSERT_TRUE(result.empty());
    }
}

TEST_F(ExpressionTest, StringManipContains)
{
    {
        StringManipContains stringManipContains(&repo, std::make_shared<LiteralStringExpression>(&repo, " wor"s));
        {
            const auto result = stringManipContains.eval(
                {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "hello world"s)});
            checkBoolResult(result, true);
        }
        {
            const auto result = stringManipContains.eval(
                {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "moin world"s)});
            checkBoolResult(result, true);
        }
        {
            const auto result =
                stringManipContains.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "world"s)});
            checkBoolResult(result, false);
        }
        {
            const auto result = stringManipContains.eval({});
            ASSERT_TRUE(result.empty());
        }
    }
    {
        StringManipContains stringManipContains(&repo, {});
        {
            const auto result = stringManipContains.eval(
                {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "hello world"s)});
            checkBoolResult(result, true);
        }
    }
    {
        StringManipContains stringManipContains(&repo, std::make_shared<LiteralStringExpression>(&repo, ""s));
        {
            const auto result = stringManipContains.eval(
                {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "hello world"s)});
            checkBoolResult(result, true);
        }
    }
}

TEST_F(ExpressionTest, StringManipMatches)
{
    {
        StringManipMatches stringManipMatches(&repo, std::make_shared<LiteralStringExpression>(&repo, ".*"s));
        {
            const auto result = stringManipMatches.eval({});
            EXPECT_TRUE(result.empty());
        }
        {
            const auto result =
                stringManipMatches.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, ""s)});
            checkBoolResult(result, true);
        }
        {
            const auto result = stringManipMatches.eval(
                {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "hello world"s)});
            checkBoolResult(result, true);
        }
    }
    {
        StringManipMatches stringManipMatches(&repo, {});
        {
            const auto result = stringManipMatches.eval(
                {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "hello world"s)});
            EXPECT_TRUE(result.empty());
        }
    }
    {
        StringManipMatches stringManipMatches(&repo, std::make_shared<LiteralStringExpression>(&repo, "."s));
        {
            const auto result = stringManipMatches.eval({});
            EXPECT_TRUE(result.empty());
        }
        {
            const auto result =
                stringManipMatches.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, ""s)});
            checkBoolResult(result, false);
        }
        {
            const auto result = stringManipMatches.eval(
                {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "hello world"s)});
            checkBoolResult(result, false);
        }
        {
            const auto result =
                stringManipMatches.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "h"s)});
            checkBoolResult(result, true);
        }
    }
    {
        const StringManipMatches stringManipMatches(&repo,
                                                    std::make_shared<LiteralStringExpression>(&repo, "^\\d{8}$"s));
        {
            const auto result = stringManipMatches.eval(
                {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "06313728"s)});
            checkBoolResult(result, true);
        }
        {
            const auto result = stringManipMatches.eval(
                {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "006313728"s)});
            checkBoolResult(result, false);
        }
    }
}

TEST_F(ExpressionTest, StringManipReplaceMatches)
{
    StringManipReplaceMatches stringManipReplaceMatches(&repo,
                                                        std::make_shared<LiteralStringExpression>(&repo, "\\\\..*"s),
                                                        std::make_shared<LiteralStringExpression>(&repo, ""));
    {
        const auto result = stringManipReplaceMatches.eval(
            {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "hallo\\\\world"s)});
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result.single()->asString(), "hallo");
    }
    {
        const auto result = stringManipReplaceMatches.eval(
            {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "hallo\\"s)});
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result.single()->asString(), "hallo\\");
    }
}
TEST_F(ExpressionTest, StringManipReplaceMatches2)
{
    GTEST_SKIP_("this sample from http://hl7.org/fhirpath/#replacematchesregex-string-substitution-string-string "
                "is not working with the current implementation");

    StringManipReplaceMatches stringManipReplaceMatches(
        &repo,
        std::make_shared<LiteralStringExpression>(&repo,
                                                  "\\b(?<month>\\d{1,2})/(?<day>\\d{1,2})/(?<year>\\d{2,4})\\b"s),
        std::make_shared<LiteralStringExpression>(&repo, "${day}-${month}-${year}"));
    const auto result = stringManipReplaceMatches.eval(
        {std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "11/30/1972"s)});
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.single()->asString(), "30-11-1972");
}

TEST_F(ExpressionTest, StringManipLength)
{
    StringManipLength stringManipLength(&repo);
    {
        const auto result = stringManipLength.eval({});
        EXPECT_TRUE(result.empty());
    }
    {
        const auto result =
            stringManipLength.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, ""s)});
        checkIntResult(result, 0);
    }
    {
        const auto result =
            stringManipLength.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "1"s)});
        checkIntResult(result, 1);
    }
    {
        const auto result =
            stringManipLength.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "hello world"s)});
        checkIntResult(result, 11);
    }
}

TEST_F(ExpressionTest, PlusOperator)
{
    StringConcatenationTest<PlusOperator>({}, {}, {});
    StringConcatenationTest<PlusOperator>("a"s, {}, {});
    StringConcatenationTest<PlusOperator>({}, "b"s, {});
    StringConcatenationTest<PlusOperator>("a"s, "b"s, "ab"s);
    StringConcatenationTest<PlusOperator>(""s, "b"s, "b"s);
    StringConcatenationTest<PlusOperator>("a"s, ""s, "a"s);
}

TEST_F(ExpressionTest, AmpersandOperator)
{
    StringConcatenationTest<AmpersandOperator>({}, {}, ""s);
    StringConcatenationTest<AmpersandOperator>("a"s, {}, "a"s);
    StringConcatenationTest<AmpersandOperator>({}, "b"s, "b"s);
    StringConcatenationTest<AmpersandOperator>("a"s, "b"s, "ab"s);
    StringConcatenationTest<AmpersandOperator>(""s, "b"s, "b"s);
    StringConcatenationTest<AmpersandOperator>("a"s, ""s, "a"s);
}

TEST_F(ExpressionTest, Trace)
{
    UtilityTrace utilityTrace(&repo, std::make_shared<LiteralStringExpression>(&repo, "trace_name"), {});
    utilityTrace.eval({rootElement});
    utilityTrace.eval({});
    utilityTrace.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, true)});
    utilityTrace.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, true),
                       std::make_shared<PrimitiveElement>(&repo, Element::Type::Decimal, DecimalType("3.14"))});
}

TEST_F(ExpressionTest, CombiningUnion)
{
    {
        CombiningUnion combiningUnion(&repo, std::make_shared<DollarThis>(&repo), std::make_shared<DollarThis>(&repo));
        const Collection input{std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "hello"s),
                               std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, true)};
        const auto result = combiningUnion.eval(input);
        EXPECT_EQ(input.equals(result), true);
    }
    {
        CombiningUnion combiningUnion(&repo, std::make_shared<DollarThis>(&repo),
                                      std::make_shared<LiteralStringExpression>(&repo, "hello"s));
        {
            const Collection input{std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "hello"s),
                                   std::make_shared<PrimitiveElement>(&repo, Element::Type::Boolean, true)};
            const auto result = combiningUnion.eval(input);
            EXPECT_EQ(input.equals(result), true);
        }
        const auto result =
            combiningUnion.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "world"s)});
        EXPECT_EQ(result.size(), 2);
        EXPECT_TRUE(result.contains(std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "hello"s)));
        EXPECT_TRUE(result.contains(std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "world"s)));
    }
}

TEST_F(ExpressionTest, CombiningCombine)
{
    CombiningCombine combiningCombine(&repo, std::make_shared<DollarThis>(&repo));
    const auto result =
        combiningCombine.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "world"s)});
    ASSERT_EQ(result.size(), 2);
    ASSERT_EQ(result[0]->asString(), "world"s);
    ASSERT_EQ(result[1]->asString(), "world"s);
}

TEST_F(ExpressionTest, CollectionsInOperator)
{
    CollectionsInOperator collectionsInOperator(&repo, std::make_shared<LiteralQuantityExpression>(&repo, "10 m"),
                                                std::make_shared<DollarThis>(&repo));
    CollectionsInOperator emptycollectionsInOperator(&repo, {}, std::make_shared<DollarThis>(&repo));
    {
        const auto result = collectionsInOperator.eval(
            {std::make_shared<PrimitiveElement>(&repo, Element::Type::Quantity, Element::QuantityType(10, "m"))});
        checkBoolResult(result, true);
    }
    {
        const auto result = collectionsInOperator.eval(
            {std::make_shared<PrimitiveElement>(&repo, Element::Type::Quantity, Element::QuantityType(10, "m")),
             std::make_shared<PrimitiveElement>(&repo, Element::Type::Quantity, Element::QuantityType(10, "m"))});
        checkBoolResult(result, true);
    }
    {
        const auto result = collectionsInOperator.eval(
            {std::make_shared<PrimitiveElement>(&repo, Element::Type::Quantity, Element::QuantityType(10, "m")),
             std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "x"s)});
        checkBoolResult(result, true);
    }
    {
        const auto result =
            collectionsInOperator.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 10),
                                        std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "x"s)});
        checkBoolResult(result, false);
    }
    {
        const auto result = collectionsInOperator.eval({});
        checkBoolResult(result, false);
    }
    {
        const auto result = emptycollectionsInOperator.eval({});
        EXPECT_TRUE(result.empty());
    }
    {
        const auto result =
            emptycollectionsInOperator.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 10),
                                             std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "x"s)});
        EXPECT_TRUE(result.empty());
    }
}

TEST_F(ExpressionTest, CollectionsContainsOperator)
{
    CollectionsContainsOperator collectionsContainsOperator(&repo, std::make_shared<DollarThis>(&repo),
                                                            std::make_shared<LiteralQuantityExpression>(&repo, "10 m"));
    CollectionsContainsOperator emptycollectionsContainsOperator(&repo, std::make_shared<DollarThis>(&repo), {});
    {
        const auto result = collectionsContainsOperator.eval(
            {std::make_shared<PrimitiveElement>(&repo, Element::Type::Quantity, Element::QuantityType(10, "m"))});
        checkBoolResult(result, true);
    }
    {
        const auto result = collectionsContainsOperator.eval(
            {std::make_shared<PrimitiveElement>(&repo, Element::Type::Quantity, Element::QuantityType(10, "m")),
             std::make_shared<PrimitiveElement>(&repo, Element::Type::Quantity, Element::QuantityType(10, "m"))});
        checkBoolResult(result, true);
    }
    {
        const auto result = collectionsContainsOperator.eval(
            {std::make_shared<PrimitiveElement>(&repo, Element::Type::Quantity, Element::QuantityType(10, "m")),
             std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "x"s)});
        checkBoolResult(result, true);
    }
    {
        const auto result =
            collectionsContainsOperator.eval({std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 10),
                                              std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "x"s)});
        checkBoolResult(result, false);
    }
    {
        const auto result = collectionsContainsOperator.eval({});
        checkBoolResult(result, false);
    }
    {
        const auto result = emptycollectionsContainsOperator.eval({});
        EXPECT_TRUE(result.empty());
    }
    {
        const auto result = emptycollectionsContainsOperator.eval(
            {std::make_shared<PrimitiveElement>(&repo, Element::Type::Integer, 10),
             std::make_shared<PrimitiveElement>(&repo, Element::Type::String, "x"s)});
        EXPECT_TRUE(result.empty());
    }
}

TEST_F(ExpressionTest, MathModTest)
{
    {
        MathModOperator mathModOperator(&repo, std::make_shared<LiteralIntegerExpression>(&repo, 5),
                                        std::make_shared<LiteralIntegerExpression>(&repo, 2));
        const auto result = mathModOperator.eval({});
        checkIntResult(result, 1);
    }
    {
        MathModOperator mathModOperator(&repo, std::make_shared<LiteralIntegerExpression>(&repo, 5),
                                        std::make_shared<LiteralIntegerExpression>(&repo, 0));
        const auto result = mathModOperator.eval({});
        EXPECT_TRUE(result.empty());
    }
    {
        MathModOperator mathModOperator(&repo, std::make_shared<LiteralDecimalExpression>(&repo, DecimalType("5.5")),
                                        std::make_shared<LiteralDecimalExpression>(&repo, DecimalType("0.7")));
        const auto result = mathModOperator.eval({});
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result.single()->asDecimal(), DecimalType("0.6")) << result.single()->asDecimal().str();
    }
    {
        MathModOperator mathModOperator(&repo, std::make_shared<LiteralDecimalExpression>(&repo, DecimalType(5)),
                                        std::make_shared<LiteralIntegerExpression>(&repo, 2));
        const auto result = mathModOperator.eval({});
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result.single()->asDecimal(), DecimalType("1"));
    }
    {
        MathModOperator mathModOperator(&repo, std::make_shared<LiteralIntegerExpression>(&repo, 5),
                                        std::make_shared<LiteralDecimalExpression>(&repo, DecimalType(2)));
        const auto result = mathModOperator.eval({});
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result.single()->asDecimal(), DecimalType("1"));
    }
}
