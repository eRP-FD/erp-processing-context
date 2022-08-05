/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "fhirtools/expression/Conversion.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/ExpressionTrace.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"

namespace fhirtools
{
ConversionIif::ConversionIif(const FhirStructureRepository* fhirStructureRepository, ExpressionPtr criterion,
                             ExpressionPtr trueResult, ExpressionPtr falseResult)
    : Expression(fhirStructureRepository)
    , mCriterion(std::move(criterion))
    , mTrueResult(std::move(trueResult))
    , mFalseResult(std::move(falseResult))
{
    FPExpect(mCriterion, "Criterion is mandatory");
}

Collection ConversionIif::eval(const Collection& collection) const
{
    EVAL_TRACE;
    const auto result = mCriterion->eval(collection).boolean();

    if (result && result->asBool())
    {
        return mTrueResult ? mTrueResult->eval({collection}) : Collection{};
    }
    return mFalseResult ? mFalseResult->eval({collection}) : Collection{};
}

Collection ConversionToInteger::eval(const Collection& collection) const
{
    EVAL_TRACE;
    if (collection.empty())
    {
        return {};
    }
    const auto& element = collection.single();
    try
    {
        return {makeIntegerElement(element->asInt())};
    }
    catch (const std::exception&)
    {
        return {};
    }
}

Collection ConversionToString::eval(const Collection& collection) const
{
    EVAL_TRACE;
    if (collection.empty())
    {
        return {};
    }
    const auto& element = collection.single();
    try
    {
        return {makeStringElement(element->asString())};
    }
    catch (const std::exception&)
    {
        return {makeBoolElement(false)};
    }
}
}
