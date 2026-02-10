/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/expression/BooleanLogic.hxx"
#include "fhirtools/expression/Comparison.hxx"
#include "fhirtools/expression/Conversion.hxx"
#include "fhirtools/expression/Expression.hxx"
#include "fhirtools/expression/Functions.hxx"
#include "fhirtools/expression/LiteralExpression.hxx"
#include "fhirtools/expression/Math.hxx"
#include "fhirtools/expression/StringManipulation.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/groups/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"
#include "test/fhirtools/DefaultFhirStructureRepository.hxx"
#include "test/util/ResourceManager.hxx"

#undef Expect
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>

using namespace fhirtools;
using namespace std::string_literals;

class ExpressionMock : public fhirtools::Expression
{
public:
    using Expression::Expression;
    MOCK_METHOD(EvaluationContext,  eval, (const EvaluationContext&), (const, override));
};

class ExpressionTest : public testing::Test
{
public:
    void SetUp() override
    {
        testResource = model::NumberAsStringParserDocument::fromJson(
            ResourceManager::instance().getStringResource("test/fhir-path/test-resource.json"));

        const auto* testDef = repo.findTypeById("Test");
        ASSERT_NE(testDef, nullptr);
        rootElement = std::make_shared<ErpElement>(&backend, std::weak_ptr<Element>{}, testDef, "Test", &testResource);
    }
    const fhirtools::FhirStructureRepositoryBackend& backend = DefaultFhirStructureRepository::getBackendWithTest();
    std::shared_ptr<const fhirtools::FhirStructureRepositoryView> mRepo = DefaultFhirStructureRepository::getWithTest();
    const FhirStructureRepositoryView& repo = *mRepo;
    model::NumberAsStringParserDocument testResource;
    std::shared_ptr<ErpElement> rootElement;

protected:
    void pathSelectionTest(const std::string& selector, std::vector<PrimitiveElement> expected)
    {
        PathSelection pathSelection(selector);
        auto result = pathSelection.eval(EvaluationContext{mRepo, rootElement}).collection;
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
            lhs.has_value() ? std::make_shared<LiteralBooleanExpression>(&backend, lhs.value()) : nullptr,
            rhs.has_value() ? std::make_shared<LiteralBooleanExpression>(&backend, rhs.value()) : nullptr);
        const auto result = booleanImpliesOperator.eval({mRepo, {}, rootElement}).collection;
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
        StringConcatOperator op(s1 ? std::make_shared<LiteralStringExpression>(&backend, s1.value()) : nullptr,
                                s2 ? std::make_shared<LiteralStringExpression>(&backend, s2.value()) : nullptr);
        const auto result = op.eval({mRepo, {}, rootElement}).collection;
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
    void checkBoolResult(const EvaluationContext& result, bool expected)
    {
        checkBoolResult(result.collection, expected);
    }

    void checkIntResult(const Collection& result, int32_t expected)
    {
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result[0]->type(), Element::Type::Integer);
        ASSERT_EQ(result[0]->asInt(), expected);
    }
    void checkIntResult(const EvaluationContext& result, int32_t expected)
    {
        checkIntResult(result.collection, expected);
    }


    void checkDecResult(const Collection& result, DecimalType expected)
    {
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result[0]->type(), Element::Type::Decimal);
        ASSERT_EQ(result[0]->asDecimal(), expected);
    }
    void checkDecResult(const EvaluationContext& result, DecimalType expected)
    {
        checkDecResult(result.collection, expected);
    }

    void checkQuantResult(const Collection& result, Element::QuantityType expected)
    {
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result[0]->type(), Element::Type::Quantity);
        ASSERT_EQ(result[0]->asQuantity().equals(expected), true);
    }
    void checkQuantResult(const EvaluationContext& result, Element::QuantityType expected)
    {
        checkQuantResult(result.collection, expected);
    }

};

TEST_F(ExpressionTest, PathSelection)
{
    using namespace std::string_literals;

    pathSelectionTest("string", {PrimitiveElement{&backend, Element::Type::String, "value"s}});
    pathSelectionTest("multiString", {PrimitiveElement{&backend, Element::Type::String, "value"s},
                                      PrimitiveElement{&backend, Element::Type::String, "value2"s}});
    pathSelectionTest("num", {PrimitiveElement{&backend, Element::Type::Integer, 12}});
    pathSelectionTest("multiNum", {PrimitiveElement{&backend, Element::Type::Integer, 1},
                                   PrimitiveElement{&backend, Element::Type::Integer, 5},
                                   PrimitiveElement{&backend, Element::Type::Integer, 42}});
    pathSelectionTest("dec", {PrimitiveElement{&backend, Element::Type::Decimal, DecimalType("1.2")}});
    pathSelectionTest("multiDec", {PrimitiveElement{&backend, Element::Type::Decimal, DecimalType("0.1")},
                                   PrimitiveElement{&backend, Element::Type::Decimal, DecimalType("3.14")}});
    pathSelectionTest("boolean", {PrimitiveElement{&backend, Element::Type::Boolean, true}});
    pathSelectionTest("multibool", {PrimitiveElement{&backend, Element::Type::Boolean, true},
                                    PrimitiveElement{&backend, Element::Type::Boolean, false},
                                    PrimitiveElement{&backend, Element::Type::Boolean, false},
                                    PrimitiveElement{&backend, Element::Type::Boolean, false}});
}

TEST_F(ExpressionTest, ExistenceEmpty)
{
    ExistenceEmpty existenceEmptyFunction{};
    EvaluationContext context{mRepo, rootElement};
    auto trueResult = existenceEmptyFunction.eval(context()).collection;
    checkBoolResult(trueResult, true);

    auto falseResult =
        existenceEmptyFunction.eval(context({std::make_shared<PrimitiveElement>(&backend, Element::Type::Integer, 1),
                                             std::make_shared<PrimitiveElement>(&backend, Element::Type::Integer, 3)}));
    checkBoolResult(falseResult.collection, false);
}

TEST_F(ExpressionTest, ExistenceExists)
{
    {
        ExistenceExists existenceExistsFunction(nullptr);
        const auto result = existenceExistsFunction.eval(EvaluationContext{mRepo, rootElement});
        checkBoolResult(result, true);
    }
    {
        ExistenceExists existenceExistsFunction(std::make_shared<EqualityEqualsOperator>(
            std::make_shared<PathSelection>("string"), std::make_shared<LiteralStringExpression>(&backend, "value")));
        const auto result = existenceExistsFunction.eval(EvaluationContext{mRepo, rootElement});
        checkBoolResult(result, true);
    }
    {
        ExistenceExists existenceExistsFunction(std::make_shared<EqualityEqualsOperator>(
            std::make_shared<PathSelection>("string"), std::make_shared<LiteralStringExpression>(&backend, "FOO")));
        const auto result = existenceExistsFunction.eval(EvaluationContext{mRepo, rootElement});
        checkBoolResult(result, false);
    }
}

TEST_F(ExpressionTest, ExistenceFunctions)
{
    ExistenceAll existenceAllFunction(std::make_shared<EqualityEqualsOperator>(
        std::make_shared<DollarThis>(), std::make_shared<LiteralBooleanExpression>(&backend, true)));
    ExistenceAllTrue existenceAllTrueFunction;
    ExistenceAnyTrue existenceAnyTrueFunction;
    ExistenceAllFalse existenceAllFalseFunction;
    ExistenceAnyFalse existenceAnyFalseFunction;

    const EvaluationContext input1{mRepo,
                                   {std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, true),
                                    std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, true),
                                    std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, true)},
                                   rootElement};
    const EvaluationContext input2{mRepo,
                                   {std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, false),
                                    std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, false),
                                    std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, true)},
                                   rootElement};
    const EvaluationContext input3{mRepo,
                                   {std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, false),
                                    std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, false),
                                    std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, false)},
                                   rootElement};
    const EvaluationContext input4{mRepo, {}, rootElement};

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
    ExistenceCount existenceCountFunction;
    EXPECT_EQ(existenceCountFunction.eval({mRepo, {}, rootElement}).collection.singleOrEmpty()->asInt(), 0);
    EXPECT_EQ(
        existenceCountFunction
            .eval({mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, true)}, rootElement})
            .collection.singleOrEmpty()
            ->asInt(),
        1);
    EXPECT_EQ(existenceCountFunction.eval(EvaluationContext{mRepo, rootElement}).collection.singleOrEmpty()->asInt(),
              1);
    EXPECT_EQ(existenceCountFunction
                  .eval({mRepo,
                         {std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, true),
                          std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, true)},
                         rootElement})
                  .collection.singleOrEmpty()
                  ->asInt(),
              2);
}

TEST_F(ExpressionTest, ExistenceDistinct)
{
    ExistenceDistinct existenceDistinctFunction;


    const EvaluationContext input1{mRepo, {}, rootElement};
    const EvaluationContext input2{mRepo,
                                   {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, ""s),
                                    std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "1"s)},
                                   rootElement};
    const EvaluationContext input3{mRepo,
                                   {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, ""s),
                                    std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "1"s),
                                    std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "1"s),
                                    std::make_shared<PrimitiveElement>(&backend, Element::Type::String, ""s)},
                                   rootElement};
    const EvaluationContext input4{
        mRepo,
        {std::make_shared<PrimitiveElement>(&backend, Element::Type::Integer, 11),
         std::make_shared<PrimitiveElement>(&backend, Element::Type::Decimal, DecimalType(11)),
         std::make_shared<PrimitiveElement>(&backend, Element::Type::Quantity, Element::QuantityType(11, {}))},
        rootElement};

    {
        const auto result = existenceDistinctFunction.eval(input1);
        EXPECT_TRUE(result.collection.empty());
    }
    {
        const auto result = existenceDistinctFunction.eval({input2});
        EXPECT_EQ(result.collection.size(), 2);
    }
    {
        const auto result = existenceDistinctFunction.eval(input3);
        EXPECT_EQ(result.collection.size(), 2);
    }
    {
        const auto result = existenceDistinctFunction.eval(input4);
        EXPECT_EQ(result.collection.size(), 3);
    }

    ExistenceIsDistinct existenceIsDistinctFunction;
    checkBoolResult(existenceIsDistinctFunction.eval(input1), true);
    checkBoolResult(existenceIsDistinctFunction.eval(input2), true);
    checkBoolResult(existenceIsDistinctFunction.eval(input3), false);
    checkBoolResult(existenceIsDistinctFunction.eval(input4), true);
}

TEST_F(ExpressionTest, FilteringWhere)
{
    {
        FilteringWhere filteringWhere(std::make_shared<EqualityEqualsOperator>(
            std::make_shared<PathSelection>("num"), std::make_shared<LiteralIntegerExpression>(&backend, 12)));
        auto result = filteringWhere.eval(EvaluationContext{mRepo, rootElement}).collection;
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result[0]->type(), Element::Type::Structured);
    }
    {
        FilteringWhere filteringWhere(std::make_shared<EqualityEqualsOperator>(
            std::make_shared<PathSelection>("num"),
            std::make_shared<LiteralDecimalExpression>(&backend, DecimalType("12.0"))));
        auto result = filteringWhere.eval(EvaluationContext{mRepo, rootElement}).collection;
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result[0]->type(), Element::Type::Structured);
    }
    {
        FilteringWhere filteringWhere(std::make_shared<EqualityEqualsOperator>(
            std::make_shared<PathSelection>("num"),
            std::make_shared<LiteralDecimalExpression>(&backend, DecimalType("12.1"))));
        auto result = filteringWhere.eval(EvaluationContext{mRepo, rootElement}).collection;
        ASSERT_EQ(result.size(), 0);
    }
    {
        FilteringWhere filteringWhere(std::make_shared<EqualityEqualsOperator>(
            std::make_shared<PathSelection>("num"), std::make_shared<LiteralIntegerExpression>(&backend, 0)));
        auto result = filteringWhere.eval(EvaluationContext{mRepo, rootElement}).collection;
        ASSERT_EQ(result.size(), 0);
    }
    {
        FilteringWhere filteringWhere(std::make_shared<PathSelection>("num"));
        auto result = filteringWhere.eval(EvaluationContext{mRepo, rootElement}).collection;
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result[0]->type(), Element::Type::Structured);
    }
}

TEST_F(ExpressionTest, FilteringSelect)
{
    FilteringSelect filteringSelectFunction(std::make_shared<PathSelection>("num"));
    auto result = filteringSelectFunction.eval(EvaluationContext{mRepo, rootElement}).collection;
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0]->type(), Element::Type::Integer);
    EXPECT_EQ(result[0]->asInt(), 12);
}

TEST_F(ExpressionTest, EqualityEqualsOperator)
{
    {
        EqualityEqualsOperator equalsOperator(std::make_shared<LiteralIntegerExpression>(&backend, 33),
                                              std::make_shared<LiteralDecimalExpression>(&backend, 33));
        auto result = equalsOperator.eval({mRepo, {}, rootElement});
        checkBoolResult(result, true);
    }
    {
        EqualityEqualsOperator equalsOperator(std::make_shared<DollarThis>(), std::make_shared<DollarThis>());
        auto result = equalsOperator.eval(EvaluationContext{mRepo, rootElement});
        checkBoolResult(result, true);
    }
    {
        EqualityEqualsOperator equalsOperator(std::make_shared<DollarThis>(),
                                              std::make_shared<PathSelection>("quantity"));
        auto result = equalsOperator.eval(EvaluationContext{mRepo, rootElement});
        checkBoolResult(result, false);
    }
}

TEST_F(ExpressionTest, EqualityNotEqualsOperator)
{
    {
        EqualityNotEqualsOperator notEqualsOperator(std::make_shared<LiteralIntegerExpression>(&backend, 33),
                                                    std::make_shared<LiteralDecimalExpression>(&backend, 33));
        auto result = notEqualsOperator.eval({mRepo, {}, rootElement});
        checkBoolResult(result, false);
    }
    {
        EqualityNotEqualsOperator notEqualsOperator(std::make_shared<DollarThis>(), std::make_shared<DollarThis>());
        auto result = notEqualsOperator.eval(EvaluationContext{mRepo, rootElement});
        checkBoolResult(result, false);
    }
    {
        EqualityNotEqualsOperator notEqualsOperator(std::make_shared<DollarThis>(),
                                                    std::make_shared<PathSelection>("quantity"));
        auto result = notEqualsOperator.eval(EvaluationContext{mRepo, rootElement});
        checkBoolResult(result, true);
    }
}

TEST_F(ExpressionTest, ComparisonOperatorGreaterThan)
{
    {
        ComparisonOperatorGreaterThan comparisonOperatorGreaterThan(
            std::make_shared<LiteralDateTimeExpression>(&backend, "2025-10-01T15:29:00+00:00"),
            std::make_shared<LiteralDateTimeExpression>(&backend, "2025-10-01T16:44:00.434+00:00"));
        auto result = comparisonOperatorGreaterThan.eval({mRepo, {}, rootElement});
        checkBoolResult(result, false);
    }
    {
        ComparisonOperatorGreaterThan comparisonOperatorGreaterThan(
            std::make_shared<LiteralDateTimeExpression>(&backend, "2025-10-01T16:44:00.434+00:00"),
            std::make_shared<LiteralDateTimeExpression>(&backend, "2025-10-01T15:29:00+00:00"));
        auto result = comparisonOperatorGreaterThan.eval({mRepo, {}, rootElement});
        checkBoolResult(result, true);
    }
    {
        ComparisonOperatorGreaterThan comparisonOperatorGreaterThan(
            std::make_shared<LiteralDateTimeExpression>(&backend, "2025-10-01T15:29:00.000+00:00"),
            std::make_shared<LiteralDateTimeExpression>(&backend, "2025-10-01T15:29:00+00:00"));
        auto result = comparisonOperatorGreaterThan.eval({mRepo, {}, rootElement});
        checkBoolResult(result, false);
    }
    {
        ComparisonOperatorGreaterThan comparisonOperatorGreaterThan(
            std::make_shared<LiteralDateTimeExpression>(&backend, "2025-10-01T15:29:00+00:00"),
            std::make_shared<LiteralDateTimeExpression>(&backend, "2025-10-01T15:29:00.000+00:00"));
        auto result = comparisonOperatorGreaterThan.eval({mRepo, {}, rootElement});
        checkBoolResult(result, false);
    }
}


TEST_F(ExpressionTest, SubsettingIndexer)
{
    {
        SubsettingIndexer subsettingIndexerFunction(std::make_shared<DollarThis>(),
                                                    std::make_shared<LiteralIntegerExpression>(&backend, 1));
        const auto result =
            subsettingIndexerFunction
                .eval({mRepo,
                       {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "foo"),
                        std::make_shared<PrimitiveElement>(&backend, Element::Type::Decimal, DecimalType(5))},
                       rootElement})
                .collection;
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result.singleOrEmpty()->type(), Element::Type::Decimal);
        EXPECT_EQ(result.singleOrEmpty()->asDecimal(), 5);
    }
    {
        SubsettingIndexer subsettingIndexerFunction(std::make_shared<DollarThis>(),
                                                    std::make_shared<LiteralIntegerExpression>(&backend, 2));
        const auto result =
            subsettingIndexerFunction
                .eval({mRepo,
                       {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "foo"),
                        std::make_shared<PrimitiveElement>(&backend, Element::Type::Decimal, DecimalType(5))},
                       rootElement})
                .collection;
        ASSERT_EQ(result.size(), 0);
    }
}

TEST_F(ExpressionTest, SubsettingFirst)
{
    SubsettingFirst subsettingFirst;
    const auto result =
        subsettingFirst
            .eval({mRepo,
                   {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "foo"),
                    std::make_shared<PrimitiveElement>(&backend, Element::Type::Decimal, DecimalType(5))},
                   rootElement})
            .collection;
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result.singleOrEmpty()->type(), Element::Type::String);
    EXPECT_EQ(result.singleOrEmpty()->asString(), "foo");
}

TEST_F(ExpressionTest, SubsettingLast)
{
    SubsettingLast subsettingFirst;
    const auto result =
        subsettingFirst
            .eval({mRepo,
                   {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "foo"),
                    std::make_shared<PrimitiveElement>(&backend, Element::Type::Decimal, DecimalType(5))},
                   rootElement})
            .collection;
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result.singleOrEmpty()->type(), Element::Type::Decimal);
    EXPECT_EQ(result.singleOrEmpty()->asDecimal(), DecimalType(5));
}

TEST_F(ExpressionTest, SubsettingLastOnEmpty)
{
    SubsettingLast subsettingFirst;
    const auto result = subsettingFirst.eval({mRepo, {}, rootElement}).collection;
    ASSERT_TRUE(result.empty());
}

TEST_F(ExpressionTest, SubsettingTail)
{
    SubsettingTail subsettingTail;
    const auto result =
        subsettingTail
            .eval({mRepo,
                   {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "foo"),
                    std::make_shared<PrimitiveElement>(&backend, Element::Type::Decimal, DecimalType(5)),
                    std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "bar")},
                   rootElement})
            .collection;
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0]->type(), Element::Type::Decimal);
    EXPECT_EQ(result[0]->asDecimal(), 5);
    EXPECT_EQ(result[1]->type(), Element::Type::String);
    EXPECT_EQ(result[1]->asString(), "bar");
}

TEST_F(ExpressionTest, SubsettingIntersect)
{
    {
        SubsettingIntersect subsettingIntersect(std::make_shared<DollarThis>());
        const auto result =
            subsettingIntersect
                .eval({mRepo,
                       {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "foo"),
                        std::make_shared<PrimitiveElement>(&backend, Element::Type::Decimal, DecimalType(5)),
                        std::make_shared<PrimitiveElement>(&backend, Element::Type::Decimal, DecimalType(5))},
                       rootElement})
                .collection;
        ASSERT_EQ(result.size(), 2);
        ASSERT_EQ(result[0]->type(), Element::Type::String);
        ASSERT_EQ(result[0]->asString(), "foo");
        ASSERT_EQ(result[1]->type(), Element::Type::Decimal);
        ASSERT_EQ(result[1]->asDecimal(), 5);
    }
    {
        SubsettingIntersect subsettingIntersect(std::make_shared<LiteralDecimalExpression>(&backend, DecimalType(5)));
        const auto result =
            subsettingIntersect
                .eval({mRepo,
                       {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "foo"),
                        std::make_shared<PrimitiveElement>(&backend, Element::Type::Decimal, DecimalType(5)),
                        std::make_shared<PrimitiveElement>(&backend, Element::Type::Decimal, DecimalType(5))},
                       rootElement})
                .collection;
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result[0]->type(), Element::Type::Decimal);
        ASSERT_EQ(result[0]->asDecimal(), 5);
    }
}

TEST_F(ExpressionTest, FilteringOfType)
{
    {
        FilteringOfType filteringOfTypeFunction(
            std::make_shared<LiteralStringExpression>(&backend, "http://hl7.org/fhirpath/System.String"));
        const auto result =
            filteringOfTypeFunction
                .eval(
                    {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "foo")}, rootElement})
                .collection;
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result[0]->type(), Element::Type::String);
    }
    {
        FilteringOfType filteringOfTypeFunction(
            std::make_shared<LiteralStringExpression>(&backend, "http://hl7.org/fhirpath/System.String"));
        const auto result = filteringOfTypeFunction
                                .eval({mRepo,
                                       {std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, true),
                                        std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "foo"),
                                        std::make_shared<PrimitiveElement>(&backend, Element::Type::Integer, 1),
                                        std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "bar"),
                                        std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, true)},
                                       rootElement})
                                .collection;
        ASSERT_EQ(result.size(), 2);
        EXPECT_EQ(result[0]->type(), Element::Type::String);
        EXPECT_EQ(result[1]->type(), Element::Type::String);
    }
    {
        FilteringOfType filteringOfTypeFunction(std::make_shared<LiteralStringExpression>(&backend, "string"));
        const auto result =
            filteringOfTypeFunction
                .eval(
                    {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "foo")}, rootElement})
                .collection;
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result[0]->type(), Element::Type::String);
    }
}

TEST_F(ExpressionTest, ConversionIif)
{
    {
        ConversionIif conversionIifFunction(
            std::make_shared<EqualityEqualsOperator>(std::make_shared<LiteralIntegerExpression>(&backend, 1),
                                                     std::make_shared<DollarThis>()),
            {std::make_shared<LiteralStringExpression>(&backend, "trueResult")}, {});

        {
            const auto result =
                conversionIifFunction
                    .eval(
                        {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::Integer, 1)}, rootElement})
                    .collection;
            ASSERT_EQ(result.size(), 1);
            EXPECT_EQ(result[0]->type(), Element::Type::String);
            EXPECT_EQ(result[0]->asString(), "trueResult");
        }
        {
            const auto result = conversionIifFunction.eval({mRepo, {}, rootElement}).collection;
            ASSERT_EQ(result.size(), 0);
        }
    }
    {
        ConversionIif conversionIifFunction(
            std::make_shared<EqualityEqualsOperator>(std::make_shared<LiteralIntegerExpression>(&backend, 1),
                                                     std::make_shared<DollarThis>()),
            {std::make_shared<LiteralStringExpression>(&backend, "trueResult")},
            {std::make_shared<LiteralStringExpression>(&backend, "falseResult")});
        {
            const auto result =
                conversionIifFunction
                    .eval(
                        {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::Integer, 1)}, rootElement})
                    .collection;
            ASSERT_EQ(result.size(), 1);
            EXPECT_EQ(result[0]->type(), Element::Type::String);
            EXPECT_EQ(result[0]->asString(), "trueResult");
        }
        {
            const auto result = conversionIifFunction.eval({mRepo, {}, rootElement}).collection;
            ASSERT_EQ(result.size(), 1);
            EXPECT_EQ(result[0]->type(), Element::Type::String);
            EXPECT_EQ(result[0]->asString(), "falseResult");
        }
    }
}

TEST_F(ExpressionTest, StringManipIndexOf)
{
    StringManipIndexOf stringManipIndexOf(std::make_shared<LiteralStringExpression>(&backend, "substring"));
    const auto result1 = stringManipIndexOf
                             .eval({mRepo,
                                    {std::make_shared<PrimitiveElement>(&backend, Element::Type::String,
                                                                        "contains the substring somewhere"s)},
                                    rootElement})
                             .collection;
    const auto result2 =
        stringManipIndexOf
            .eval({mRepo,
                   {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "does not contain it."s)},
                   rootElement})
            .collection;
    ASSERT_EQ(result1.size(), 1);
    EXPECT_EQ(result1[0]->type(), Element::Type::Integer);
    EXPECT_EQ(result1[0]->asInt(), 13);
    ASSERT_EQ(result2.size(), 1);
    EXPECT_EQ(result2[0]->type(), Element::Type::Integer);
    EXPECT_EQ(result2[0]->asInt(), -1);
}

TEST_F(ExpressionTest, TreeNavigationChildren)
{
    TreeNavigationChildren treeNavigationChildren;
    {
        const auto result = treeNavigationChildren.eval(EvaluationContext{mRepo, rootElement}).collection;
        ASSERT_EQ(result.size(), 22);
    }
    {
        const auto result = treeNavigationChildren.eval({mRepo, {}, rootElement}).collection;
        ASSERT_EQ(result.size(), 0);
    }
    {
        const auto result =
            treeNavigationChildren
                .eval({mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "")}, rootElement})
                .collection;
        ASSERT_EQ(result.size(), 0);
    }
}

TEST_F(ExpressionTest, TreeNavigationDescendants)
{
    TreeNavigationDescendants treeNavigationDescendants;
    {
        const auto result = treeNavigationDescendants.eval(EvaluationContext{mRepo, rootElement}).collection;
        ASSERT_EQ(result.size(), 24);
    }
    {
        const auto result = treeNavigationDescendants.eval({mRepo, {}, rootElement}).collection;
        ASSERT_EQ(result.size(), 0);
    }
    {
        const auto result =
            treeNavigationDescendants
                .eval({mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "")}, rootElement})
                .collection;
        ASSERT_EQ(result.size(), 0);
    }
}

TEST_F(ExpressionTest, TypesIsOperator)
{
    TypesIsOperator typesIsOperator(std::make_shared<DollarThis>(), "http://hl7.org/fhirpath/System.Decimal");
    const auto result1 =
        typesIsOperator
            .eval({mRepo,
                   {std::make_shared<PrimitiveElement>(&backend, Element::Type::Decimal, DecimalType("3.14"))},
                   rootElement})
            .collection;
    ASSERT_EQ(result1.size(), 1);
    EXPECT_EQ(result1.singleOrEmpty()->asBool(), true);
    const auto result2 =
        typesIsOperator
            .eval({mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::Integer, 3)}, rootElement})
            .collection;
    ASSERT_EQ(result2.size(), 1);
    EXPECT_EQ(result2.singleOrEmpty()->asBool(), false);
}

TEST_F(ExpressionTest, TypeAsOperator)
{
    TypeAsOperator typesAsOperator(backend, std::make_shared<DollarThis>(), "http://hl7.org/fhirpath/System.Decimal");
    const auto result1 =
        typesAsOperator
            .eval({mRepo,
                   {std::make_shared<PrimitiveElement>(&backend, Element::Type::Decimal, DecimalType("3.14"))},
                   rootElement})
            .collection;
    ASSERT_EQ(result1.size(), 1);
    EXPECT_EQ(result1.singleOrEmpty()->type(), Element::Type::Decimal);
    EXPECT_EQ(result1.singleOrEmpty()->asDecimal(), DecimalType("3.14"));
    const auto result2 =
        typesAsOperator
            .eval({mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::Integer, 3)}, rootElement})
            .collection;
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

TEST_F(ExpressionTest, BooleanAndOperator_lazyEval)
{
    auto lhs = std::make_shared<LiteralBooleanExpression>(&backend, false);
    auto rhs = std::make_shared<ExpressionMock>();
    EXPECT_CALL(*rhs, eval).Times(0);
    BooleanAndOperator implies{lhs, rhs};
    (void) implies.eval({mRepo, {}, rootElement});
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

TEST_F(ExpressionTest, BooleanOrOperator_lazyEval)
{
    auto lhs = std::make_shared<LiteralBooleanExpression>(&backend, true);
    auto rhs = std::make_shared<ExpressionMock>();
    EXPECT_CALL(*rhs, eval).Times(0);
    BooleanOrOperator implies{lhs, rhs};
    (void) implies.eval({mRepo, {}, rootElement});
}

TEST_F(ExpressionTest, BooleanNotOperator)
{
    {
        BooleanNotOperator booleanNotOperator;
        const auto result = booleanNotOperator.eval({mRepo, {}, rootElement}).collection;
        EXPECT_TRUE(result.empty());
    }
    {
        BooleanNotOperator booleanNotOperator;
        const auto result = booleanNotOperator.eval(
            {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, true)}, rootElement});
        checkBoolResult(result, false);
    }
    {
        BooleanNotOperator booleanNotOperator;
        const auto result = booleanNotOperator.eval(
            {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, false)}, rootElement});
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

TEST_F(ExpressionTest, BooleanXorOperator_lazyEval)
{
    // normally XOR cannot be lazy-eval but due to the ternary logic we can short-circuit on _empty_
    fhirtools::EvaluationContext context{mRepo, rootElement};
    auto lhs = std::make_shared<testing::NiceMock<ExpressionMock>>();
    ON_CALL(*lhs, eval).WillByDefault(::testing::Return(context()));
    auto rhs = std::make_shared<ExpressionMock>();
    EXPECT_CALL(*rhs, eval).Times(0);
    BooleanXorOperator implies{lhs, rhs};
    (void) implies.eval(context);
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

TEST_F(ExpressionTest, BooleanImpliesOperator_lazyEval)
{
    auto lhs = std::make_shared<LiteralBooleanExpression>(&backend, false);
    auto rhs = std::make_shared<ExpressionMock>();
    EXPECT_CALL(*rhs, eval).Times(0);
    BooleanImpliesOperator implies{lhs, rhs};
    (void) implies.eval({mRepo, {}, rootElement});
}

TEST_F(ExpressionTest, ConversionToString)
{
    ConversionToString conversionToString;
    const auto result1 =
        conversionToString
            .eval({mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, false)}, rootElement})
            .collection.single();
    EXPECT_EQ(result1->type(), Element::Type::String);
    EXPECT_EQ(result1->asString(), "false");

    const auto result2 =
        conversionToString
            .eval({mRepo,
                   {std::make_shared<PrimitiveElement>(&backend, Element::Type::Decimal, DecimalType("3.14"))},
                   rootElement})
            .collection.single();
    EXPECT_EQ(result2->type(), Element::Type::String);
    EXPECT_EQ(result2->asString(), "3.14");

    EXPECT_THROW(
        (void) conversionToString.eval({mRepo,
                                        {std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, false),
                                         std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, false)},
                                        rootElement}),
        std::exception);

    const auto result3 = conversionToString.eval(EvaluationContext{mRepo, rootElement}).collection.single();
    EXPECT_EQ(result3->type(), Element::Type::Boolean);
    EXPECT_EQ(result3->asBool(), false);
}

TEST_F(ExpressionTest, ConversionToInt)
{
    ConversionToInteger conversionToInteger;
    {
        const auto result = conversionToInteger.eval(
            {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, false)}, rootElement});
        checkIntResult(result, 0);
    }
    {
        const auto result = conversionToInteger.eval(
            {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, true)}, rootElement});
        checkIntResult(result, 1);
    }
    {
        const auto result = conversionToInteger.eval(
            {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "1234"s)}, rootElement});
        checkIntResult(result, 1234);
    }
    {
        const auto result = conversionToInteger.eval(
            {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "1234x"s)}, rootElement});
        ASSERT_TRUE(result.collection.empty());
    }
    {
        const auto result = conversionToInteger.eval(
            {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::Integer, 321)}, rootElement});
        checkIntResult(result, 321);
    }
    {
        const auto result = conversionToInteger.eval(
            {mRepo,
             {std::make_shared<PrimitiveElement>(&backend, Element::Type::Decimal, DecimalType(123))},
             rootElement});
        ASSERT_TRUE(result.collection.empty());
    }
    {
        const auto result = conversionToInteger.eval(
            {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "2147483648")}, rootElement});
        ASSERT_TRUE(result.collection.empty());
    }
    {
        const auto result = conversionToInteger.eval(
            {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "-2147483649")}, rootElement});
        ASSERT_TRUE(result.collection.empty());
    }
}

TEST_F(ExpressionTest, StringManipSubstring)
{
    {
        StringManipSubstring stringManipSubstring(std::make_shared<LiteralIntegerExpression>(&backend, 5), {});
        {
            const auto result =
                stringManipSubstring
                    .eval({mRepo,
                           {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "hello world"s)},
                           rootElement})
                    .collection;
            EXPECT_EQ(result.single()->type(), Element::Type::String);
            EXPECT_EQ(result.single()->asString(), " world");
        }
        {
            const auto result = stringManipSubstring.eval(
                {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "hello"s)}, rootElement});
            EXPECT_TRUE(result.collection.empty());
        }
        {
            const auto result = stringManipSubstring.eval({mRepo, {}, rootElement});
            EXPECT_TRUE(result.collection.empty());
        }
    }
    {
        StringManipSubstring stringManipSubstring(std::make_shared<LiteralIntegerExpression>(&backend, 5),
                                                  std::make_shared<LiteralIntegerExpression>(&backend, 1));
        {
            const auto result =
                stringManipSubstring
                    .eval({mRepo,
                           {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "hello world"s)},
                           rootElement})
                    .collection;
            EXPECT_EQ(result.single()->type(), Element::Type::String);
            EXPECT_EQ(result.single()->asString(), " ");
        }
        {
            const auto result = stringManipSubstring.eval(
                {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "hello"s)}, rootElement});
            EXPECT_TRUE(result.collection.empty());
        }
    }
    {
        StringManipSubstring stringManipSubstring({}, {});
        {
            const auto result = stringManipSubstring.eval(
                {mRepo,
                 {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "hello world"s)},
                 rootElement});
            EXPECT_TRUE(result.collection.empty());
        }
    }
}

TEST_F(ExpressionTest, StringManipStartsWith)
{
    {
        StringManipStartsWith stringManipStartsWith(std::make_shared<LiteralStringExpression>(&backend, "hello"s));
        {
            const auto result = stringManipStartsWith.eval(
                {mRepo,
                 {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "hello world"s)},
                 rootElement});
            checkBoolResult(result, true);
        }
        {
            const auto result = stringManipStartsWith.eval(
                {mRepo,
                 {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "moin world"s)},
                 rootElement});
            checkBoolResult(result, false);
        }
        {
            const auto result = stringManipStartsWith.eval({mRepo, {}, rootElement});
            ASSERT_TRUE(result.collection.empty());
        }
    }
    {
        StringManipStartsWith stringManipStartsWith(std::make_shared<LiteralStringExpression>(&backend, ""s));
        const auto result = stringManipStartsWith.eval(
            {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "moin world"s)}, rootElement});
        checkBoolResult(result, true);
    }
    {
        StringManipStartsWith stringManipStartsWith{nullptr};
        const auto result = stringManipStartsWith.eval(
            {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "moin world"s)}, rootElement});
        ASSERT_TRUE(result.collection.empty());
    }
}

TEST_F(ExpressionTest, StringManipContains)
{
    {
        StringManipContains stringManipContains(std::make_shared<LiteralStringExpression>(&backend, " wor"s));
        {
            const auto result = stringManipContains.eval(
                {mRepo,
                 {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "hello world"s)},
                 rootElement});
            checkBoolResult(result, true);
        }
        {
            const auto result = stringManipContains.eval(
                {mRepo,
                 {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "moin world"s)},
                 rootElement});
            checkBoolResult(result, true);
        }
        {
            const auto result = stringManipContains.eval(
                {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "world"s)}, rootElement});
            checkBoolResult(result, false);
        }
        {
            const auto result = stringManipContains.eval({mRepo, {}, rootElement});
            ASSERT_TRUE(result.collection.empty());
        }
    }
    {
        StringManipContains stringManipContains{nullptr};
        {
            const auto result = stringManipContains.eval(
                {mRepo,
                 {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "hello world"s)},
                 rootElement});
            checkBoolResult(result, true);
        }
    }
    {
        StringManipContains stringManipContains(std::make_shared<LiteralStringExpression>(&backend, ""s));
        {
            const auto result = stringManipContains.eval(
                {mRepo,
                 {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "hello world"s)},
                 rootElement});
            checkBoolResult(result, true);
        }
    }
}

TEST_F(ExpressionTest, StringManipMatches)
{
    {
        StringManipMatches stringManipMatches(std::make_shared<LiteralStringExpression>(&backend, ".*"s));
        {
            const auto result = stringManipMatches.eval({mRepo, {}, rootElement});
            EXPECT_TRUE(result.collection.empty());
        }
        {
            const auto result = stringManipMatches.eval(
                {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, ""s)}, rootElement});
            checkBoolResult(result, true);
        }
        {
            const auto result = stringManipMatches.eval(
                {mRepo,
                 {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "hello world"s)},
                 rootElement});
            checkBoolResult(result, true);
        }
    }
    {
        StringManipMatches stringManipMatches(nullptr);
        {
            const auto result = stringManipMatches.eval(
                {mRepo,
                 {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "hello world"s)},
                 rootElement});
            EXPECT_TRUE(result.collection.empty());
        }
    }
    {
        StringManipMatches stringManipMatches(std::make_shared<LiteralStringExpression>(&backend, "."s));
        {
            const auto result = stringManipMatches.eval({mRepo, {}, rootElement});
            EXPECT_TRUE(result.collection.empty());
        }
        {
            const auto result = stringManipMatches.eval(
                {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, ""s)}, rootElement});
            checkBoolResult(result, false);
        }
        {
            const auto result = stringManipMatches.eval(
                {mRepo,
                 {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "hello world"s)},
                 rootElement});
            checkBoolResult(result, false);
        }
        {
            const auto result = stringManipMatches.eval(
                {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "h"s)}, rootElement});
            checkBoolResult(result, true);
        }
    }
    {
        const StringManipMatches stringManipMatches(std::make_shared<LiteralStringExpression>(&backend, "^\\d{8}$"s));
        {
            const auto result = stringManipMatches.eval(
                {mRepo,
                 {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "06313728"s)},
                 rootElement});
            checkBoolResult(result, true);
        }
        {
            const auto result = stringManipMatches.eval(
                {mRepo,
                 {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "006313728"s)},
                 rootElement});
            checkBoolResult(result, false);
        }
    }
    {
        const StringManipMatches stringManipMatches(std::make_shared<LiteralStringExpression>(&backend, "^[^-].*$"s));
        {
            const auto result = stringManipMatches.eval(
                {mRepo,
                 {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "+3,1415"s)},
                 rootElement});
            checkBoolResult(result, true);
        }
        {
            const auto result = stringManipMatches.eval(
                {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "3,1415"s)}, rootElement});
            checkBoolResult(result, true);
        }
        {
            const auto result = stringManipMatches.eval(
                {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "+"s)}, rootElement});
            checkBoolResult(result, true);
        }
        {
            const auto result = stringManipMatches.eval(
                {mRepo,
                 {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "-3,1415"s)},
                 rootElement});
            checkBoolResult(result, false);
        }
        {
            const auto result = stringManipMatches.eval(
                {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "-"s)}, rootElement});
            checkBoolResult(result, false);
        }
    }
}

TEST_F(ExpressionTest, StringManipReplaceMatches)
{
    StringManipReplaceMatches stringManipReplaceMatches(std::make_shared<LiteralStringExpression>(&backend, "\\\\..*"s),
                                                        std::make_shared<LiteralStringExpression>(&backend, ""));
    {
        const auto result =
            stringManipReplaceMatches
                .eval({mRepo,
                       {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "hallo\\\\world"s)},
                       rootElement})
                .collection;
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result.single()->asString(), "hallo");
    }
    {
        const auto result =
            stringManipReplaceMatches
                .eval({mRepo,
                       {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "hallo\\"s)},
                       rootElement})
                .collection;
        ASSERT_EQ(result.size(), 1);
        ASSERT_EQ(result.single()->asString(), "hallo\\");
    }
}
TEST_F(ExpressionTest, StringManipReplaceMatches2)
{
    GTEST_SKIP_("this sample from http://hl7.org/fhirpath/#replacematchesregex-string-substitution-string-string "
                "is not working with the current implementation");

    StringManipReplaceMatches stringManipReplaceMatches(
        std::make_shared<LiteralStringExpression>(&backend,
                                                  "\\b(?<month>\\d{1,2})/(?<day>\\d{1,2})/(?<year>\\d{2,4})\\b"s),
        std::make_shared<LiteralStringExpression>(&backend, "${day}-${month}-${year}"));
    const auto result = stringManipReplaceMatches
                            .eval({mRepo,
                                   {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "11/30/1972"s)},
                                   rootElement})
                            .collection;
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result.single()->asString(), "30-11-1972");
}

TEST_F(ExpressionTest, StringManipLength)
{
    StringManipLength stringManipLength;
    {
        const auto result = stringManipLength.eval({mRepo, {}, rootElement});
        EXPECT_TRUE(result.collection.empty());
    }
    {
        const auto result = stringManipLength.eval(
            {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, ""s)}, rootElement});
        checkIntResult(result, 0);
    }
    {
        const auto result = stringManipLength.eval(
            {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "1"s)}, rootElement});
        checkIntResult(result, 1);
    }
    {
        const auto result = stringManipLength.eval(
            {mRepo,
             {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "hello world"s)},
             rootElement});
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

struct StringManipSplitTestParam{
  std::list<std::string> input;
  std::string delimiter;
  std::vector<std::string> expect;
};

void PrintTo(const StringManipSplitTestParam& param,  std::ostream* out)
{
    (*out) << R"("input": [)";
    for (std::string_view sep{}; const auto& i: param.input)
    {
        (*out) << sep << '"' << i << '"';
        sep = ", ";
    }
    (*out) << R"(], "delimiter": ")" << param.delimiter;
    (*out) << R"(", "expect": [)";
    for (std::string_view sep{}; const auto& e: param.expect)
    {
        (*out) << sep << '"' << e << '"';
        sep = ", ";
    }
    (*out) << "]";
}

class StringManipSplitTest : public ExpressionTest, public testing::WithParamInterface<StringManipSplitTestParam>
{
public:
    EvaluationContext toEvaluationContext(const std::list<std::string>& strings)
    {
        EvaluationContext result{mRepo, {}, rootElement};
        for (const auto& s : strings)
        {
            result.collection.emplace_back(std::make_shared<PrimitiveElement>(&backend, Element::Type::String, s));
        }
        return result;
    }
};

TEST_P(StringManipSplitTest, StringManipSplit)
{
    PrintTo(GetParam(), &std::cerr);
    EvaluationContext input = toEvaluationContext(GetParam().input);
    ExpressionPtr delimiter = std::make_shared<LiteralStringExpression>(&backend, GetParam().delimiter);
    const auto& expected = GetParam().expect;
    StringManipSplit split{delimiter};
    auto actualCollection = split.eval(input).collection;
    ASSERT_EQ(actualCollection.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i)
    {
        auto actual = actualCollection.at(i);
        EXPECT_EQ(actual->type(), fhirtools::Element::Type::String);
        EXPECT_EQ(actual->asString(), expected.at(i));
    }
}

INSTANTIATE_TEST_SUITE_P(singleInput, StringManipSplitTest,
                         testing::ValuesIn<std::list<StringManipSplitTestParam>>({
                             {
                                 .input = {"Hello World!"},
                                 .delimiter = " ",
                                 .expect = {"Hello", "World!"},
                             },
                             {
                                 .input = {"Hello wide World!"},
                                 .delimiter = " wide ",
                                 .expect = {"Hello", "World!"},
                             },
                             {
                                 .input = {"A,B,C,D"},
                                 .delimiter = ",",
                                 .expect = {"A", "B", "C", "D"},
                             },
                             {
                                 .input = {"A, B, C, D"},
                                 .delimiter = ", ",
                                 .expect = {"A", "B", "C", "D"},
                             },
                             {
                                 .input = {"ABCD"},
                                 .delimiter = "",
                                 .expect = {"A", "B", "C", "D"},
                             },
                             {
                                 .input = {},
                                 .delimiter = ",",
                                 .expect = {},
                             },
                             {
                                 .input = {""},
                                 .delimiter = "",
                                 .expect = {""},
                             },
                             {
                                 .input = {""},
                                 .delimiter = ",",
                                 .expect = {""},
                             },
                             {
                                 .input = {","},
                                 .delimiter = ",",
                                 .expect = {"", ""},
                             },
                             {
                                 .input = {",,,"},
                                 .delimiter = ",",
                                 .expect = {"", "", "", ""},
                             },
                             {
                                 .input = {"A,,B"},
                                 .delimiter = ",",
                                 .expect = {"A", "", "B"},
                             },
                             {
                                 .input = {",A,B"},
                                 .delimiter = ",",
                                 .expect = {"", "A", "B"},
                             },
                             {
                                 .input = {"A,B,"},
                                 .delimiter = ",",
                                 .expect = {"A", "B", ""},
                             },
                         }));

INSTANTIATE_TEST_SUITE_P(multiInput, StringManipSplitTest,
                         testing::ValuesIn<std::list<StringManipSplitTestParam>>({
                             {
                                 .input = {"Hello", "World!"},
                                 .delimiter = "l",
                                 .expect = {"He", "", "o", "Wor", "d!"},
                             },
                             {
                                 .input = {"A,B", "C,D,"},
                                 .delimiter = ",",
                                 .expect = {"A", "B", "C", "D", ""},
                             },
                             {
                                 .input = {"", "A,B"},
                                 .delimiter = ",",
                                 .expect = {"", "A", "B"},
                             },
                             {
                                 .input = {"A,B", ""},
                                 .delimiter = ",",
                                 .expect = {"A", "B", ""},
                             },
                         }));

TEST_F(ExpressionTest, Trace)
{
    UtilityTrace utilityTrace(std::make_shared<LiteralStringExpression>(&backend, "trace_name"), {});
    (void) utilityTrace.eval(EvaluationContext{mRepo, rootElement});
    (void) utilityTrace.eval({mRepo, {}, rootElement});
    (void) utilityTrace.eval(
        {mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, true)}, rootElement});
    (void) utilityTrace.eval(
        {mRepo,
         {std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, true),
          std::make_shared<PrimitiveElement>(&backend, Element::Type::Decimal, DecimalType("3.14"))},
         rootElement});
}

TEST_F(ExpressionTest, CombiningUnion)
{
    {
        CombiningUnion combiningUnion(std::make_shared<DollarThis>(), std::make_shared<DollarThis>());
        const Collection input{std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "hello"s),
                               std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, true)};
        const auto result = combiningUnion.eval({mRepo, input, rootElement});
        EXPECT_EQ(input.equals(result.collection), true);
    }
    {
        CombiningUnion combiningUnion(std::make_shared<DollarThis>(),
                                      std::make_shared<LiteralStringExpression>(&backend, "hello"s));
        {
            const Collection input{std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "hello"s),
                                   std::make_shared<PrimitiveElement>(&backend, Element::Type::Boolean, true)};
            const auto result = combiningUnion.eval({mRepo, input, rootElement});
            EXPECT_EQ(input.equals(result.collection), true);
        }
        const auto result = combiningUnion
                                .eval({mRepo,
                                       {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "world"s)},
                                       rootElement})
                                .collection;
        EXPECT_EQ(result.size(), 2);
        EXPECT_TRUE(result.contains(std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "hello"s)));
        EXPECT_TRUE(result.contains(std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "world"s)));
    }
}

TEST_F(ExpressionTest, CombiningCombine)
{
    CombiningCombine combiningCombine(std::make_shared<DollarThis>());
    const auto result =
        combiningCombine
            .eval({mRepo, {std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "world"s)}, rootElement})
            .collection;
    ASSERT_EQ(result.size(), 2);
    ASSERT_EQ(result[0]->asString(), "world"s);
    ASSERT_EQ(result[1]->asString(), "world"s);
}

TEST_F(ExpressionTest, CollectionsInOperator)
{
    CollectionsInOperator collectionsInOperator(std::make_shared<LiteralQuantityExpression>(&backend, "10 m"),
                                                std::make_shared<DollarThis>());
    CollectionsInOperator emptycollectionsInOperator({}, std::make_shared<DollarThis>());
    {
        const auto result = collectionsInOperator.eval(
            {mRepo,
             {std::make_shared<PrimitiveElement>(&backend, Element::Type::Quantity, Element::QuantityType(10, "m"))},
             rootElement});
        checkBoolResult(result, true);
    }
    {
        const auto result = collectionsInOperator.eval(
            {mRepo,
             {std::make_shared<PrimitiveElement>(&backend, Element::Type::Quantity, Element::QuantityType(10, "m")),
              std::make_shared<PrimitiveElement>(&backend, Element::Type::Quantity, Element::QuantityType(10, "m"))},
             rootElement});
        checkBoolResult(result, true);
    }
    {
        const auto result = collectionsInOperator.eval(
            {mRepo,
             {std::make_shared<PrimitiveElement>(&backend, Element::Type::Quantity, Element::QuantityType(10, "m")),
              std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "x"s)},
             rootElement});
        checkBoolResult(result, true);
    }
    {
        const auto result =
            collectionsInOperator.eval({mRepo,
                                        {std::make_shared<PrimitiveElement>(&backend, Element::Type::Integer, 10),
                                         std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "x"s)},
                                        rootElement});
        checkBoolResult(result, false);
    }
    {
        const auto result = collectionsInOperator.eval({mRepo, {}, rootElement});
        checkBoolResult(result, false);
    }
    {
        const auto result = emptycollectionsInOperator.eval({mRepo, {}, rootElement});
        EXPECT_TRUE(result.collection.empty());
    }
    {
        const auto result = emptycollectionsInOperator.eval(
            {mRepo,
             {std::make_shared<PrimitiveElement>(&backend, Element::Type::Integer, 10),
              std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "x"s)},
             rootElement});
        EXPECT_TRUE(result.collection.empty());
    }
}

TEST_F(ExpressionTest, CollectionsContainsOperator)
{
    CollectionsContainsOperator collectionsContainsOperator(
        std::make_shared<DollarThis>(), std::make_shared<LiteralQuantityExpression>(&backend, "10 m"));
    CollectionsContainsOperator emptycollectionsContainsOperator(std::make_shared<DollarThis>(), {});
    {
        const auto result = collectionsContainsOperator.eval(
            {mRepo,
             {std::make_shared<PrimitiveElement>(&backend, Element::Type::Quantity, Element::QuantityType(10, "m"))},
             rootElement});
        checkBoolResult(result, true);
    }
    {
        const auto result = collectionsContainsOperator.eval(
            {mRepo,
             {std::make_shared<PrimitiveElement>(&backend, Element::Type::Quantity, Element::QuantityType(10, "m")),
              std::make_shared<PrimitiveElement>(&backend, Element::Type::Quantity, Element::QuantityType(10, "m"))},
             rootElement});
        checkBoolResult(result, true);
    }
    {
        const auto result = collectionsContainsOperator.eval(
            {mRepo,
             {std::make_shared<PrimitiveElement>(&backend, Element::Type::Quantity, Element::QuantityType(10, "m")),
              std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "x"s)},
             rootElement});
        checkBoolResult(result, true);
    }
    {
        const auto result = collectionsContainsOperator.eval(
            {mRepo,
             {std::make_shared<PrimitiveElement>(&backend, Element::Type::Integer, 10),
              std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "x"s)},
             rootElement});
        checkBoolResult(result, false);
    }
    {
        const auto result = collectionsContainsOperator.eval({mRepo, {}, rootElement});
        checkBoolResult(result, false);
    }
    {
        const auto result = emptycollectionsContainsOperator.eval({mRepo, {}, rootElement});
        EXPECT_TRUE(result.collection.empty());
    }
    {
        const auto result = emptycollectionsContainsOperator.eval(
            {mRepo,
             {std::make_shared<PrimitiveElement>(&backend, Element::Type::Integer, 10),
              std::make_shared<PrimitiveElement>(&backend, Element::Type::String, "x"s)},
             rootElement});
        EXPECT_TRUE(result.collection.empty());
    }
}

TEST_F(ExpressionTest, MathModTest)
{
    {
        MathModOperator mathModOperator(std::make_shared<LiteralIntegerExpression>(&backend, 5),
                                        std::make_shared<LiteralIntegerExpression>(&backend, 2));
        const auto result = mathModOperator.eval({mRepo, {}, rootElement});
        checkIntResult(result, 1);
    }
    {
        MathModOperator mathModOperator(std::make_shared<LiteralIntegerExpression>(&backend, 5),
                                        std::make_shared<LiteralIntegerExpression>(&backend, 0));
        const auto result = mathModOperator.eval({mRepo, {}, rootElement});
        EXPECT_TRUE(result.collection.empty());
    }
    {
        MathModOperator mathModOperator(std::make_shared<LiteralDecimalExpression>(&backend, DecimalType("5.5")),
                                        std::make_shared<LiteralDecimalExpression>(&backend, DecimalType("0.7")));
        const auto result = mathModOperator.eval({mRepo, {}, rootElement}).collection;
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result.single()->asDecimal(), DecimalType("0.6")) << result.single()->asDecimal().str();
    }
    {
        MathModOperator mathModOperator(std::make_shared<LiteralDecimalExpression>(&backend, DecimalType(5)),
                                        std::make_shared<LiteralIntegerExpression>(&backend, 2));
        const auto result = mathModOperator.eval({mRepo, {}, rootElement}).collection;
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result.single()->asDecimal(), DecimalType("1"));
    }
    {
        MathModOperator mathModOperator(std::make_shared<LiteralIntegerExpression>(&backend, 5),
                                        std::make_shared<LiteralDecimalExpression>(&backend, DecimalType(2)));
        const auto result = mathModOperator.eval({mRepo, {}, rootElement}).collection;
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result.single()->asDecimal(), DecimalType("1"));
    }
}

TEST_F(ExpressionTest, MathPlusTest)
{
    {
        PlusOperator plusOperator(std::make_shared<LiteralIntegerExpression>(&backend, 5),
                                  std::make_shared<LiteralIntegerExpression>(&backend, 1));
        const auto result = plusOperator.eval({mRepo, {}, rootElement});
        checkIntResult(result, 6);
    }
    {
        PlusOperator plusOperator(std::make_shared<LiteralIntegerExpression>(&backend, 0),
                                  std::make_shared<LiteralIntegerExpression>(&backend, 0));
        const auto result = plusOperator.eval({mRepo, {}, rootElement});
        checkIntResult(result, 0);
    }
    {
        PlusOperator plusOperator(std::make_shared<LiteralIntegerExpression>(&backend, -1),
                                  std::make_shared<LiteralIntegerExpression>(&backend, 1));
        const auto result = plusOperator.eval({mRepo, {}, rootElement});
        checkIntResult(result, 0);
    }
    {
        PlusOperator plusOperator(std::make_shared<LiteralDecimalExpression>(&backend, DecimalType{-1.5}),
                                  std::make_shared<LiteralDecimalExpression>(&backend, DecimalType{1.0}));
        const auto result = plusOperator.eval({mRepo, {}, rootElement});
        checkDecResult(result, DecimalType{-0.5});
    }
    {
        PlusOperator plusOperator(std::make_shared<LiteralDecimalExpression>(&backend, DecimalType{-1.5}),
                                  std::make_shared<LiteralIntegerExpression>(&backend, 1));
        const auto result = plusOperator.eval({mRepo, {}, rootElement});
        checkDecResult(result, DecimalType{-0.5});
    }
    {
        PlusOperator plusOperator(std::make_shared<LiteralQuantityExpression>(&backend, "-1.5m"),
                                  std::make_shared<LiteralQuantityExpression>(&backend, "1m"));
        const auto result = plusOperator.eval({mRepo, {}, rootElement});
        checkQuantResult(result, Element::QuantityType{DecimalType{-0.5}, "m"});
    }
    {
        PlusOperator plusOperator(std::make_shared<LiteralQuantityExpression>(&backend, "-1.5"),
                                  std::make_shared<LiteralDecimalExpression>(&backend, DecimalType{1.0}));
        const auto result = plusOperator.eval({mRepo, {}, rootElement});
        checkQuantResult(result, Element::QuantityType{DecimalType{-0.5}, ""});
    }
    {
        PlusOperator plusOperator(std::make_shared<LiteralQuantityExpression>(&backend, "-1.5"),
                                  std::make_shared<LiteralIntegerExpression>(&backend, 1));
        const auto result = plusOperator.eval({mRepo, {}, rootElement});
        checkQuantResult(result, Element::QuantityType{DecimalType{-0.5}, ""});
    }
}

TEST_F(ExpressionTest, MathMinusTest)
{
    {
        MathMinusOperator minusOperator(std::make_shared<LiteralIntegerExpression>(&backend, 5),
                                        std::make_shared<LiteralIntegerExpression>(&backend, 1));
        const auto result = minusOperator.eval({mRepo, {}, rootElement});
        checkIntResult(result, 4);
    }
    {
        MathMinusOperator minusOperator(std::make_shared<LiteralIntegerExpression>(&backend, 0),
                                        std::make_shared<LiteralIntegerExpression>(&backend, 0));
        const auto result = minusOperator.eval({mRepo, {}, rootElement});
        checkIntResult(result, 0);
    }
    {
        MathMinusOperator minusOperator(std::make_shared<LiteralIntegerExpression>(&backend, -1),
                                        std::make_shared<LiteralIntegerExpression>(&backend, 1));
        const auto result = minusOperator.eval({mRepo, {}, rootElement});
        checkIntResult(result, -2);
    }
    {
        MathMinusOperator minusOperator(std::make_shared<LiteralDecimalExpression>(&backend, DecimalType{-1.5}),
                                        std::make_shared<LiteralDecimalExpression>(&backend, DecimalType{1.0}));
        const auto result = minusOperator.eval({mRepo, {}, rootElement});
        checkDecResult(result, DecimalType{-2.5});
    }
    {
        MathMinusOperator minusOperator(std::make_shared<LiteralDecimalExpression>(&backend, DecimalType{-1.5}),
                                        std::make_shared<LiteralIntegerExpression>(&backend, 1));
        const auto result = minusOperator.eval({mRepo, {}, rootElement});
        checkDecResult(result, DecimalType{-2.5});
    }
    {
        MathMinusOperator minusOperator(std::make_shared<LiteralQuantityExpression>(&backend, "-1.5m"),
                                        std::make_shared<LiteralQuantityExpression>(&backend, "1m"));
        const auto result = minusOperator.eval({mRepo, {}, rootElement});
        checkQuantResult(result, Element::QuantityType{DecimalType{-2.5}, "m"});
    }
    {
        MathMinusOperator minusOperator(std::make_shared<LiteralQuantityExpression>(&backend, "-1.5"),
                                        std::make_shared<LiteralDecimalExpression>(&backend, DecimalType{1.0}));
        const auto result = minusOperator.eval({mRepo, {}, rootElement});
        checkQuantResult(result, Element::QuantityType{DecimalType{-2.5}, ""});
    }
    {
        MathMinusOperator minusOperator(std::make_shared<LiteralQuantityExpression>(&backend, "-1.5"),
                                        std::make_shared<LiteralIntegerExpression>(&backend, 1));
        const auto result = minusOperator.eval({mRepo, {}, rootElement});
        checkQuantResult(result, Element::QuantityType{DecimalType{-2.5}, ""});
    }
}

TEST(PercentContextTest, context)
{
    FhirResourceGroupConst resolver{"test"};
    FhirStructureRepositoryBackend backend;
    backend.load({
        ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/minifhirtypes.xml")
    }, resolver);
    auto doc = ResourceManager::instance().getStringResource("test/fhir-path/samples/ERP-29015-context-variable.json");
    auto jsonDoc = model::NumberAsStringParserDocument::fromJson(doc);
    auto element =
        std::make_shared<ErpElement>(&backend, std::weak_ptr<const fhirtools::Element>{}, "Resource", &jsonDoc);

    auto expr =
        FhirPathParser::parse(&backend, "extension.value.toString().substring(0, %context.extension.value.length())");
    Collection result;
    ASSERT_NO_THROW(result = expr->eval(EvaluationContext{backend.defaultView(), element}).collection);
    LOG(INFO) << result;
    EXPECT_EQ(result.size(), 1);

}
