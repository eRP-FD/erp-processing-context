/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/expression/BooleanLogic.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/ExpressionTrace.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"

namespace fhirtools
{
EvaluationContext BooleanAndOperator::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    const auto lhs = mLhs ? mLhs->eval(context).collection.boolean() : nullptr;
    if (lhs && !lhs->asBool())
    {
        return context.makeBoolElement(false);
    }
    const auto rhs = mRhs ? mRhs->eval(context).collection.boolean() : nullptr;
    if (rhs && !rhs->asBool())
    {
        return context.makeBoolElement(false);
    }
    if (lhs && rhs)
    {
        return context.makeBoolElement(true);
    }
    return context();
}

EvaluationContext BooleanOrOperator::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    const auto lhs = mLhs ? mLhs->eval(context).collection.boolean() : nullptr;
    if (lhs && lhs->asBool())
    {
        return context.makeBoolElement(true);
    }
    const auto rhs = mRhs ? mRhs->eval(context).collection.boolean() : nullptr;
    if (rhs && rhs->asBool())
    {
        return context.makeBoolElement(true);
    }
    if (lhs && rhs)
    {
        return context.makeBoolElement(false);
    }
    return context();
}

EvaluationContext BooleanNotOperator::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    if (context.collection.empty())
    {
        return context();
    }
    return context.makeBoolElement(! context.collection.single()->asBool());
}

EvaluationContext BooleanXorOperator::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    const auto lhs = mLhs ? mLhs->eval(context).collection.boolean() : nullptr;
    if (! lhs)
    {
        return context();
    }
    const auto rhs = mRhs ? mRhs->eval(context).collection.boolean() : nullptr;
    if (! rhs)
    {
        return context();
    }
    return context.makeBoolElement(lhs->asBool() != rhs->asBool());
}

EvaluationContext BooleanImpliesOperator::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    const auto lhs = mLhs ? mLhs->eval(context).collection.boolean() : nullptr;
    if (lhs)
    {
        if (lhs->asBool())
        {
            const auto rhs = mRhs ? mRhs->eval(context).collection.boolean() : nullptr;
            if (! rhs)
            {
                return context();
            }
            return context(rhs);
        }
        else
        {
            return context.makeBoolElement(true);
        }
    }
    else
    {
        const auto rhs = mRhs ? mRhs->eval(context).collection.boolean() : nullptr;
        if (rhs && rhs->asBool())
        {
            return context.makeBoolElement(true);
        }
    }
    return context();
}
}
