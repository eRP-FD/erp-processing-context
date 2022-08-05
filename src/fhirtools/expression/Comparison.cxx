/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "fhirtools/expression/Comparison.hxx"

#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/ExpressionTrace.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"

namespace fhirtools
{
Collection EqualityEqualsOperator::eval(const Collection& collection) const
{
    EVAL_TRACE;
    const auto& lhs = mLhs->eval(collection);
    const auto& rhs = mRhs->eval(collection);
    if (lhs.empty() || rhs.empty())
    {
        return {};
    }
    return {makeBoolElement(lhs == rhs)};
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
    return {makeBoolElement(lhs != rhs)};
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
    if (isImplicitConvertible(lhs.single()->type(), rhs.single()->type()))
    {
        return {makeBoolElement(*lhs.single() > *rhs.single())};
    }
    else if (isImplicitConvertible(rhs.single()->type(), lhs.single()->type()))
    {
        return {makeBoolElement(*rhs.single() < *lhs.single())};
    }
    FPFail("invalid operands for operator>");
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
    if (isImplicitConvertible(lhs.single()->type(), rhs.single()->type()))
    {
        return {makeBoolElement(*lhs.single() < *rhs.single())};
    }
    else if (isImplicitConvertible(rhs.single()->type(), lhs.single()->type()))
    {
        return {makeBoolElement(*rhs.single() > *lhs.single())};
    }
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
    if (isImplicitConvertible(lhs.single()->type(), rhs.single()->type()))
    {
        return {makeBoolElement(*lhs.single() >= *rhs.single())};
    }
    else if (isImplicitConvertible(rhs.single()->type(), lhs.single()->type()))
    {
        return {makeBoolElement(*rhs.single() <= *lhs.single())};
    }
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
    if (isImplicitConvertible(lhs.single()->type(), rhs.single()->type()))
    {
        return {makeBoolElement(*lhs.single() <= *rhs.single())};
    }
    else if (isImplicitConvertible(rhs.single()->type(), lhs.single()->type()))
    {
        return {makeBoolElement(*rhs.single() >= *lhs.single())};
    }
    FPFail("invalid operands for operator<=");
}
}
