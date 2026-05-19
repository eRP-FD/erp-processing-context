/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/expression/StringManipulation.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/ExpressionTrace.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"
#include "fhirtools/util/Gsl.hxx"

#include <regex>

namespace fhirtools
{
EvaluationContext StringManipIndexOf::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    if (context.collection.empty() || ! mArg)
    {
        return context();
    }
    const auto substrResult = mArg->eval(context);
    if (substrResult.collection.empty())
    {
        return context();
    }
    FPExpect(substrResult.collection.singleOrEmpty()->type() == Element::Type::String,
             "substring must be single and of type string");
    FPExpect(context.collection.singleOrEmpty()->type() == Element::Type::String,
             "collection must be single and of type string");
    if (auto idx = context.collection.singleOrEmpty()->asString().find(substrResult.collection.singleOrEmpty()->asString());
        idx != std::string::npos)
    {
        return context.makeIntegerElement(static_cast<int>(idx));
    }
    return context.makeIntegerElement(-1);
}

EvaluationContext StringManipSubstring::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    if (context.collection.empty() || ! mLhs)
    {
        return context();
    }
    const auto startResult = mLhs->eval(context);
    if (startResult.collection.empty())
    {
        return context();
    }
    if (!context.collection.single()->hasValue())
    {
        return context();
    }
    const auto start = gsl::narrow<size_t>(startResult.collection.single()->asInt());
    const auto& asString = context.collection.single()->asString();
    if (start >= asString.size())
    {
        return context();
    }
    const size_t length = mRhs ? gsl::narrow<size_t>(mRhs->eval(context).collection.single()->asInt()) : std::string::npos;

    return context.makeStringElement(asString.substr(start, length));
}

EvaluationContext StringManipStartsWith::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    if (context.collection.empty() || ! mArg)
    {
        return context();
    }
    if (! mArg)
    {
        return context.makeBoolElement(true);
    }
    const auto prefixResult = mArg->eval(context);
    if (prefixResult.collection.empty())
    {
        return context.makeBoolElement(true);
    }
    if (!context.collection.single()->hasValue())
    {
        return context();
    }
    const auto prefix = prefixResult.collection.single()->asString();
    const auto& asString = context.collection.single()->asString();
    return context.makeBoolElement(asString.starts_with(prefix));
}

EvaluationContext StringManipContains::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    if (context.collection.empty())
    {
        return context();
    }
    if (! mArg)
    {
        return context.makeBoolElement(true);
    }
    const auto substringResult = mArg->eval(context);
    if (substringResult.collection.empty())
    {
        return context.makeBoolElement(true);
    }
    if (!context.collection.single()->hasValue())
    {
        return context();
    }
    const auto substring = substringResult.collection.single()->asString();
    const auto& asString = context.collection.single()->asString();
    return context.makeBoolElement(asString.find(substring) != std::string::npos);
}

EvaluationContext StringManipMatches::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    if (! mArg || context.collection.empty())
    {
        return context();
    }
    const auto regexResult = mArg->eval(context);
    if (regexResult.collection.empty())
    {
        return context();
    }
    if (!context.collection.single()->hasValue())
    {
        return context();
    }
    const auto regex = regexResult.collection.single()->asString();
    const auto& asString = context.collection.single()->asString();
    std::regex reg{regex};
    return context.makeBoolElement(std::regex_match(asString, reg));
}

EvaluationContext StringManipReplaceMatches::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    if (! mLhs || ! mRhs || context.collection.empty())
    {
        return context();
    }
    const auto regexResult = mLhs->eval(context);
    const auto replacementResult = mRhs->eval(context);
    if (regexResult.collection.empty() || replacementResult.collection.empty())
    {
        return context();
    }
    if (!context.collection.single()->hasValue())
    {
        return context();
    }
    const auto regex = regexResult.collection.single()->asString();
    const auto replacement = replacementResult.collection.single()->asString();
    const auto& asString = context.collection.single()->asString();
    std::regex reg{regex, std::regex::extended};
    return context.makeStringElement(std::regex_replace(asString, reg, replacement));
}

EvaluationContext StringManipLength::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    if (context.collection.empty())
    {
        return context();
    }
    if (!context.collection.single()->hasValue())
    {
        return context();
    }
    return context.makeIntegerElement(context.collection.single()->asString().length());
}

EvaluationContext PlusOperator::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    const auto lhs = mLhs ? mLhs->eval(context).collection.singleOrEmpty() : nullptr;
    const auto rhs = mRhs ? mRhs->eval(context).collection.singleOrEmpty() : nullptr;
    if (! lhs || ! rhs)
    {
        return context();
    }
    if (isImplicitConvertible(lhs->type(), rhs->type()))
    {
        return plus(context, *lhs, *rhs, rhs->type());
    }
    if (isImplicitConvertible(rhs->type(), lhs->type()))
    {
        return plus(context, *lhs, *rhs, lhs->type());
    }
    FPFail("incompatible operand types for plus operator");
}

EvaluationContext PlusOperator::plus(const EvaluationContext& context, const Element& lhs, const Element& rhs, Element::Type type)
{
    switch (type)
    {
        case Element::Type::Integer:
            return context.makeIntegerElement(lhs.asInt() + rhs.asInt());
        case Element::Type::Decimal:
            return context.makeDecimalElement(lhs.asDecimal() + rhs.asDecimal());
        case Element::Type::String:
            return context.makeStringElement(lhs.asString() + rhs.asString());
        case Element::Type::Quantity:{
            const auto lhsAsQuantity = lhs.asQuantity();
            const auto rhsAsQuantity = rhs.asQuantity();
            if (lhsAsQuantity.unit() == rhsAsQuantity.unit())
            {
                return context.makeQuantityElement({lhsAsQuantity.value() + rhsAsQuantity.value(), lhsAsQuantity.unit()});
            }
            break;
        }
        case Element::Type::Boolean:
        case Element::Type::Date:
        case Element::Type::DateTime:
        case Element::Type::Time:
        case Element::Type::Structured:
            break;
    }
    FPFail("incompatible operand types for + operator");
}

EvaluationContext AmpersandOperator::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    const auto lhs = mLhs ? mLhs->eval(context).collection.singleOrEmpty() : nullptr;
    const auto rhs = mRhs ? mRhs->eval(context).collection.singleOrEmpty() : nullptr;
    const auto lhsStr = lhs ? lhs->asString() : "";
    const auto rhsStr = rhs ? rhs->asString() : "";
    return context.makeStringElement(lhsStr + rhsStr);
}

EvaluationContext StringManipSplit::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    if (context.collection.empty())
    {
        return context();
    }
    if (! mArg)
    {
        // return context unchanged
        return context;
    }
    const auto delimiterResult = mArg->eval(context);
    if (delimiterResult.collection.empty())
    {
        return context();
    }
    const auto delimiter = delimiterResult.collection.front()->asString();
    auto result = context();
    for (const auto& item: context.collection)
    {
        if (item->type() == Element::Type::String)
        {
            split(result, item->asString(), delimiter);
        }
    }
    return result;
}

void fhirtools::StringManipSplit::split(EvaluationContext& result, std::string_view str, std::string_view delimiter) const
{
    const auto* repo = std::addressof(result.context->getFhirStructureRepository());
    if (str.empty())
    {
        result.collection.emplace_back(std::make_shared<PrimitiveElement>(repo, Element::Type::String, std::string{}));
        return;
    }
    if (delimiter.empty())
    {
        chars(result, str);
        return;
    }
    for (auto range = std::ranges::search(str, delimiter); range.begin() != str.end();
         range = std::ranges::search(str, delimiter))
    {
        result.collection.emplace_back(
            std::make_shared<PrimitiveElement>(repo, Element::Type::String, std::string{str.begin(), range.begin()}));
        str = {range.end(), str.end()};
    }
    result.collection.emplace_back(std::make_shared<PrimitiveElement>(repo, Element::Type::String, std::string{str}));
}

void fhirtools::StringManipSplit::chars(EvaluationContext& result, std::string_view str) const
{
    const auto* repo = std::addressof(result.context->getFhirStructureRepository());
    for (char c : str)
    {
        result.collection.emplace_back(
            std::make_shared<PrimitiveElement>(repo, Element::Type::String, std::string(1, c)));
    }
}

}
