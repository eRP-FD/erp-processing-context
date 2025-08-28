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
    const auto rhs = mRhs ? mRhs->eval(context).collection.boolean() : nullptr;
    if (lhs && lhs->asBool() && rhs && rhs->asBool())
    {
        return context.makeBoolElement(true);
    }
    if ((lhs && ! lhs->asBool()) || (rhs && ! rhs->asBool()))
    {
        return context.makeBoolElement(false);
    }
    return context();
}

EvaluationContext BooleanOrOperator::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    const auto lhs = mLhs ? mLhs->eval(context).collection.boolean() : nullptr;
    const auto rhs = mRhs ? mRhs->eval(context).collection.boolean() : nullptr;
    if (lhs && ! lhs->asBool() && rhs && ! rhs->asBool())
    {
        return context.makeBoolElement(false);
    }
    if ((lhs && lhs->asBool()) || (rhs && rhs->asBool()))
    {
        return context.makeBoolElement(true);
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
    const auto rhs = mRhs ? mRhs->eval(context).collection.boolean() : nullptr;
    if (! lhs || ! rhs)
    {
        return context();
    }
    if ((lhs->asBool() && ! rhs->asBool()) || (! lhs->asBool() && rhs->asBool()))
    {
        return context.makeBoolElement(true);
    }
    return context.makeBoolElement(false);
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
