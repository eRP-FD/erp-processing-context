/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "fhirtools/expression/BooleanLogic.hxx"

#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/ExpressionTrace.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"

namespace fhirtools
{
Collection BooleanAndOperator::eval(const Collection& collection) const
{
    EVAL_TRACE;
    const auto lhs = mLhs ? mLhs->eval(collection).boolean() : nullptr;
    const auto rhs = mRhs ? mRhs->eval(collection).boolean() : nullptr;
    if (lhs && lhs->asBool() && rhs && rhs->asBool())
    {
        return {makeBoolElement(true)};
    }
    if ((lhs && ! lhs->asBool()) || (rhs && ! rhs->asBool()))
    {
        return {makeBoolElement(false)};
    }
    return {};
}

Collection BooleanOrOperator::eval(const Collection& collection) const
{
    EVAL_TRACE;
    const auto lhs = mLhs ? mLhs->eval(collection).boolean() : nullptr;
    const auto rhs = mRhs ? mRhs->eval(collection).boolean() : nullptr;
    if (lhs && ! lhs->asBool() && rhs && ! rhs->asBool())
    {
        return {makeBoolElement(false)};
    }
    if ((lhs && lhs->asBool()) || (rhs && rhs->asBool()))
    {
        return {makeBoolElement(true)};
    }
    return {};
}

Collection BooleanNotOperator::eval(const Collection& collection) const
{
    EVAL_TRACE;
    if (collection.empty())
    {
        return {};
    }
    return {makeBoolElement(! collection.single()->asBool())};
}

Collection BooleanXorOperator::eval(const Collection& collection) const
{
    EVAL_TRACE;
    const auto lhs = mLhs ? mLhs->eval(collection).boolean() : nullptr;
    const auto rhs = mRhs ? mRhs->eval(collection).boolean() : nullptr;
    if (! lhs || ! rhs)
    {
        return {};
    }
    if ((lhs->asBool() && ! rhs->asBool()) || (! lhs->asBool() && rhs->asBool()))
    {
        return {makeBoolElement(true)};
    }
    return {makeBoolElement(false)};
}

Collection BooleanImpliesOperator::eval(const Collection& collection) const
{
    EVAL_TRACE;
    const auto lhs = mLhs ? mLhs->eval(collection).boolean() : nullptr;
    const auto rhs = mRhs ? mRhs->eval(collection).boolean() : nullptr;
    if (lhs)
    {
        if (lhs->asBool())
        {
            if (! rhs)
            {
                return {};
            }
            return {rhs};
        }
        else
        {
            return {makeBoolElement(true)};
        }
    }
    else
    {
        if (rhs && rhs->asBool())
        {
            return {makeBoolElement(true)};
        }
    }
    return {};
}
}
