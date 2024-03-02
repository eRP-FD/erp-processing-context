/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/expression/Comparison.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/ExpressionTrace.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"

namespace fhirtools
{

ComparisonOperator::ComparisonOperator(const FhirStructureRepository* fhirStructureRepository, ExpressionPtr lhs,
                                       ExpressionPtr rhs)
    : BinaryExpression(fhirStructureRepository, std::move(lhs), std::move(rhs))
{
    FPExpect(mLhs && mRhs, "missing mandatory argument");
}

Collection ComparisonOperator::comparison(const Element& lhs, const Element& rhs,
                                          const std::vector<std::strong_ordering>& expected) const
{
    const auto result = lhs.compareTo(rhs);
    if (! result.has_value())
    {
        return {};
    }
    return {Expression::makeBoolElement(std::ranges::find(expected, *result) != expected.end())};
}

Collection EqualityEqualsOperator::eval(const Collection& collection) const
{
    EVAL_TRACE;
    const auto& lhs = mLhs->eval(collection);
    const auto& rhs = mRhs->eval(collection);
    if (lhs.empty() || rhs.empty())
    {
        return {};
    }
    const auto result = lhs.equals(rhs);
    return result.has_value() ? Collection{makeBoolElement(*result)} : Collection{};
}

Collection EqualityNotEqualsOperator::eval(const Collection& collection) const
{
    EVAL_TRACE;
    const auto& lhs = mLhs->eval(collection);
    const auto& rhs = mRhs->eval(collection);
    if (lhs.empty() || rhs.empty())
    {
        return {};
    }
    const auto result = lhs.equals(rhs);
    return result.has_value() ? Collection{makeBoolElement(! *result)} : Collection{};
}

Collection ComparisonOperatorGreaterThan::eval(const Collection& collection) const
{
    EVAL_TRACE;
    const auto& lhs = mLhs->eval(collection);
    const auto& rhs = mRhs->eval(collection);
    if (lhs.empty() || rhs.empty())
    {
        return {};
    }
    return comparison(*lhs.single(), *rhs.single(), {std::strong_ordering::greater});
}

Collection ComparisonOperatorLessThan::eval(const Collection& collection) const
{
    EVAL_TRACE;
    const auto& lhs = mLhs->eval(collection);
    const auto& rhs = mRhs->eval(collection);
    if (lhs.empty() || rhs.empty())
    {
        return {};
    }
    return comparison(*lhs.single(), *rhs.single(), {std::strong_ordering::less});
    FPFail("invalid operands for operator<");
}

Collection ComparisonOperatorGreaterOrEqual::eval(const Collection& collection) const
{
    EVAL_TRACE;
    const auto& lhs = mLhs->eval(collection);
    const auto& rhs = mRhs->eval(collection);
    if (lhs.empty() || rhs.empty())
    {
        return {};
    }
    return comparison(*lhs.single(), *rhs.single(), {std::strong_ordering::greater, std::strong_ordering::equal});
    FPFail("invalid operands for operator>=");
}

Collection ComparisonOperatorLessOrEqual::eval(const Collection& collection) const
{
    EVAL_TRACE;
    const auto& lhs = mLhs->eval(collection);
    const auto& rhs = mRhs->eval(collection);
    if (lhs.empty() || rhs.empty())
    {
        return {};
    }
    return comparison(*lhs.single(), *rhs.single(), {std::strong_ordering::less, std::strong_ordering::equal});
    FPFail("invalid operands for operator<=");
}
}
