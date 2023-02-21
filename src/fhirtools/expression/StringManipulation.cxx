/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "fhirtools/expression/StringManipulation.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/ExpressionTrace.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/util/Gsl.hxx"

#include <regex>

namespace fhirtools
{
Collection StringManipIndexOf::eval(const Collection& collection) const
{
    EVAL_TRACE;
    if (collection.empty() || ! mArg)
    {
        return {};
    }
    const auto substrResult = mArg->eval(collection);
    if (substrResult.empty())
    {
        return {};
    }
    FPExpect(substrResult.singleOrEmpty()->type() == Element::Type::String,
             "substring must be single and of type string");
    FPExpect(collection.singleOrEmpty()->type() == Element::Type::String,
             "collection must be single and of type string");
    if (auto idx = collection.singleOrEmpty()->asString().find(substrResult.singleOrEmpty()->asString());
        idx != std::string::npos)
    {
        return {makeIntegerElement(static_cast<int>(idx))};
    }
    return {makeIntegerElement(-1)};
}

Collection StringManipSubstring::eval(const Collection& collection) const
{
    EVAL_TRACE;
    if (collection.empty() || ! mLhs)
    {
        return {};
    }
    const auto startResult = mLhs->eval(collection);
    if (startResult.empty())
    {
        return {};
    }
    const size_t start = gsl::narrow<size_t>(startResult.single()->asInt());
    const auto& asString = collection.single()->asString();
    if (start >= asString.size())
    {
        return {};
    }
    const size_t length = mRhs ? gsl::narrow<size_t>(mRhs->eval(collection).single()->asInt()) : std::string::npos;

    return {makeStringElement(asString.substr(start, length))};
}

Collection StringManipStartsWith::eval(const Collection& collection) const
{
    EVAL_TRACE;
    if (collection.empty() || ! mArg)
    {
        return {};
    }
    if (! mArg)
    {
        return {makeBoolElement(true)};
    }
    const auto prefixResult = mArg->eval(collection);
    if (prefixResult.empty())
    {
        return {makeBoolElement(true)};
    }
    const auto prefix = prefixResult.single()->asString();
    const auto& asString = collection.single()->asString();
    return {makeBoolElement(asString.find(prefix) == 0)};
}

Collection StringManipContains::eval(const Collection& collection) const
{
    EVAL_TRACE;
    if (collection.empty())
    {
        return {};
    }
    if (! mArg)
    {
        return {makeBoolElement(true)};
    }
    const auto substringResult = mArg->eval(collection);
    if (substringResult.empty())
    {
        return {makeBoolElement(true)};
    }
    const auto substring = substringResult.single()->asString();
    const auto& asString = collection.single()->asString();
    return {makeBoolElement(asString.find(substring) != std::string::npos)};
}

Collection StringManipMatches::eval(const Collection& collection) const
{
    EVAL_TRACE;
    if (! mArg || collection.empty())
    {
        return {};
    }
    const auto regexResult = mArg->eval(collection);
    if (regexResult.empty())
    {
        return {};
    }
    const auto regex = regexResult.single()->asString();
    const auto& asString = collection.single()->asString();
    std::regex reg{regex};
    return {makeBoolElement(std::regex_match(asString, reg))};
}

Collection StringManipReplaceMatches::eval(const Collection& collection) const
{
    EVAL_TRACE;
    if (! mLhs || ! mRhs || collection.empty())
    {
        return {};
    }
    const auto regexResult = mLhs->eval(collection);
    const auto replacementResult = mRhs->eval(collection);
    if (regexResult.empty() || replacementResult.empty())
    {
        return {};
    }
    const auto regex = regexResult.single()->asString();
    const auto replacement = replacementResult.single()->asString();
    const auto& asString = collection.single()->asString();
    std::regex reg{regex, std::regex::extended};
    return {makeStringElement(std::regex_replace(asString, reg, replacement))};
}

Collection StringManipLength::eval(const Collection& collection) const
{
    EVAL_TRACE;
    if (collection.empty())
    {
        return {};
    }
    return {makeIntegerElement(collection.single()->asString().length())};
}

Collection PlusOperator::eval(const Collection& collection) const
{
    EVAL_TRACE;
    const auto lhs = mLhs ? mLhs->eval(collection).singleOrEmpty() : nullptr;
    const auto rhs = mRhs ? mRhs->eval(collection).singleOrEmpty() : nullptr;
    if (! lhs || ! rhs)
    {
        return {};
    }
    if (lhs->type() == Element::Type::Integer && rhs->type() == Element::Type::Integer)
    {
        return {makeIntegerElement(lhs->asInt() + rhs->asInt())};
    }
    else if (lhs->type() == Element::Type::String && rhs->type() == Element::Type::String)
    {
        return {makeStringElement(lhs->asString() + rhs->asString())};
    }
    FPFail("operator+ not yet implemented for type " + std::string{magic_enum::enum_name(lhs->type())});
}

Collection AmpersandOperator::eval(const Collection& collection) const
{
    EVAL_TRACE;
    const auto lhs = mLhs ? mLhs->eval(collection).singleOrEmpty() : nullptr;
    const auto rhs = mRhs ? mRhs->eval(collection).singleOrEmpty() : nullptr;
    const auto lhsStr = lhs ? lhs->asString() : "";
    const auto rhsStr = rhs ? rhs->asString() : "";
    return {makeStringElement(lhsStr + rhsStr)};
}
}
