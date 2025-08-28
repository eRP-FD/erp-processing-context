/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/expression/Comparison.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/ExpressionTrace.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"

namespace fhirtools
{

ComparisonOperator::ComparisonOperator(
    std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView, ExpressionPtr lhs,
    ExpressionPtr rhs)
    : BinaryExpression(std::move(fhirStructureRepositoryView), std::move(lhs), std::move(rhs))
{
    FPExpect(mLhs && mRhs, "missing mandatory argument");
}

template <bool (*cmp)(std::partial_ordering)>
EvaluationContext ComparisonOperator::comparison(const EvaluationContext& lhs, const EvaluationContext& rhs) const
{
    const auto& context = lhs; // use arbitrary context as return context
    if (lhs.collection.empty() || rhs.collection.empty())
    {
        return context();
    }
    const auto result = lhs.collection.single()->compareTo(*rhs.collection.single());
    if (! result.has_value())
    {
        return context();
    }
    return context.makeBoolElement(cmp(*result));
}

EvaluationContext EqualityEqualsOperator::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    const auto& lhs = mLhs->eval(context);
    const auto& rhs = mRhs->eval(context);
    if (lhs.collection.empty() || rhs.collection.empty())
    {
        return context();
    }
    const auto result = lhs.collection.equals(rhs.collection);
    return result.has_value() ? context.makeBoolElement(*result) : context();
}

EvaluationContext EqualityNotEqualsOperator::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    const auto& lhs = mLhs->eval(context);
    const auto& rhs = mRhs->eval(context);
    if (lhs.collection.empty() || rhs.collection.empty())
    {
        return context();
    }
    const auto result = lhs.collection.equals(rhs.collection);
    return result.has_value() ? context.makeBoolElement(! *result) : context();
}

EvaluationContext ComparisonOperatorGreaterThan::eval(const EvaluationContext& context) const
{
    return comparison<std::is_gt>(mLhs->eval(context), mRhs->eval(context));
}

EvaluationContext ComparisonOperatorLessThan::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    return comparison<std::is_lt>(mLhs->eval(context), mRhs->eval(context));
}

EvaluationContext ComparisonOperatorGreaterOrEqual::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    return comparison<std::is_gteq>(mLhs->eval(context), mRhs->eval(context));
}

EvaluationContext ComparisonOperatorLessOrEqual::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    return comparison<std::is_lteq>(mLhs->eval(context), mRhs->eval(context));
}
}
