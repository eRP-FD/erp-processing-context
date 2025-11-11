/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/expression/Conversion.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/ExpressionTrace.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"

namespace fhirtools
{
ConversionIif::ConversionIif(ExpressionPtr criterion, ExpressionPtr trueResult, ExpressionPtr falseResult)
    : mCriterion(std::move(criterion))
    , mTrueResult(std::move(trueResult))
    , mFalseResult(std::move(falseResult))
{
    FPExpect(mCriterion, "Criterion is mandatory");
}

EvaluationContext ConversionIif::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    const auto result = mCriterion->eval(context).collection.boolean();

    if (result && result->asBool())
    {
        return mTrueResult ? mTrueResult->eval(context) : context();
    }
    return mFalseResult ? mFalseResult->eval(context) : context();
}

EvaluationContext ConversionToInteger::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    if (context.collection.empty())
    {
        return context();
    }
    const auto& element = context.collection.single();
    try
    {
        return context.makeIntegerElement(element->asInt());
    }
    catch (const std::exception&)
    {
        return context();
    }
}

EvaluationContext ConversionToString::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    if (context.collection.empty())
    {
        return context();
    }
    const auto& element = context.collection.single();
    try
    {
        return context.makeStringElement(element->asString());
    }
    catch (const std::exception&)
    {
        return context.makeBoolElement(false);
    }
}
}
