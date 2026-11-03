/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/expression/Functions.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/ExpressionTrace.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"

namespace fhirtools
{
EvaluationContext ExistenceEmpty::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    return context.makeBoolElement(context.collection.empty());
}

//NOLINTNEXTLINE(misc-no-recursion)
EvaluationContext ExistenceExists::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    if (! mArg)
    {
        return context.makeBoolElement(!context.collection.empty());
    }
    return ExistenceExists(nullptr).eval(FilteringWhere(mArg).eval(context));
}

EvaluationContext ExistenceAll::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    FPExpect(mArg, "no criterion argument given to all(...)");
    for (const auto& item : context.collection)
    {
        auto result = mArg->eval(context(item)).collection.boolean();
        if (! result || ! result->asBool())
        {
            return context.makeBoolElement(false);
        }
    }
    return context.makeBoolElement(true);
}

EvaluationContext ExistenceAllTrue::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    for (const auto& item : context.collection)
    {
        if (! item->asBool())
        {
            return context.makeBoolElement(false);
        }
    }
    return context.makeBoolElement(true);
}

EvaluationContext ExistenceAnyTrue::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    for (const auto& item : context.collection)
    {
        if (item->asBool())
        {
            return context.makeBoolElement(true);
        }
    }
    return context.makeBoolElement(false);
}

EvaluationContext ExistenceAllFalse::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    for (const auto& item : context.collection)
    {
        if (item->asBool())
        {
            return context.makeBoolElement(false);
        }
    }
    return context.makeBoolElement(true);
}

EvaluationContext ExistenceAnyFalse::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    for (const auto& item : context.collection)
    {
        if (! item->asBool())
        {
            return context.makeBoolElement(true);
        }
    }
    return context.makeBoolElement(false);
}

EvaluationContext ExistenceCount::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    return context.makeIntegerElement(context.collection.size());
}

EvaluationContext ExistenceDistinct::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    auto ret = context();
    std::ranges::copy_if(context.collection, std::back_insert_iterator(ret.collection),
                         [&ret](const Collection::value_type& val) {
                             return ! ret.collection.contains(val);
                         });
    return ret;
}

EvaluationContext ExistenceIsDistinct::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    return context.makeBoolElement(ExistenceDistinct{}.eval(context).collection.size() == context.collection.size());
}

EvaluationContext FilteringWhere::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    FPExpect(mArg, "no criterion argument given to where(...)");
    auto ret = context();
    for (const auto& item : context.collection)
    {
        const auto result = mArg->eval(context(item)).collection.boolean();
        if (result && result->asBool())
        {
            ret.collection.emplace_back(item);
        }
    }
    return ret;
}

EvaluationContext FilteringSelect::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    FPExpect(mArg, "no projection argument given to select(...)");
    auto ret = context();
    for (const auto& item : context.collection)
    {
        ret.collection.append(mArg->eval(context(item)).collection);
    }
    return ret;
}

EvaluationContext FilteringOfType::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    FPExpect(mArg, "type specifier not specified");
    const auto arg = mArg->eval(context).collection.singleOrEmpty();
    FPExpect(arg, "type not specified");
    const auto* type = context.view->findTypeById(arg->asString());
    FPExpect(type, "could not resolve type " + arg->asString());
    auto ret = context();
    for (const auto& item : context.collection)
    {
        if (item->getStructureDefinition()->isSystemType() &&
            type->kind() == FhirStructureDefinition::Kind::primitiveType)
        {
            if (item->getStructureDefinition()->isDerivedFrom(type->primitiveToSystemType().url()))
            {
                ret.collection.emplace_back(item);
            }
        }
        else
        {
            if (item->getStructureDefinition()->isDerivedFrom(type->url()))
            {
                ret.collection.emplace_back(item);
            }
        }
    }
    return ret;
}

SubsettingIndexer::SubsettingIndexer(ExpressionPtr lhs, ExpressionPtr rhs)
    : BinaryExpression(std::move(lhs), std::move(rhs))
{
    FPExpect(mLhs && mRhs, "missing mandatory argument");
}

EvaluationContext SubsettingIndexer::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    auto lhs = mLhs->eval(context);
    auto index = static_cast<size_t>(mRhs->eval(context).collection.single()->asInt());
    if (lhs.collection.size() <= index)
    {
        return context();
    }
    return context(lhs.collection[index]);
}

EvaluationContext SubsettingFirst::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    return context.collection.empty() ? context() : context(context.collection[0]);
}

EvaluationContext SubsettingLast::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    return context.collection.empty() ? context() : context(context.collection.back());
}

EvaluationContext SubsettingTail::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    return context.collection.empty() ? context() : context({context.collection.begin() + 1, context.collection.end()});
}

SubsettingIntersect::SubsettingIntersect(ExpressionPtr arg)
    : UnaryExpression(std::move(arg))
{
    FPExpect(mArg, "missing mandatory argument");
}

EvaluationContext SubsettingIntersect::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    auto other = mArg->eval(context);
    auto ret = context();
    for (const auto& item : context.collection)
    {
        if (other.collection.contains(item) && ! ret.collection.contains(item))
        {
            ret.collection.push_back(item);
        }
    }
    return ret;
}

CombiningUnion::CombiningUnion(ExpressionPtr lhs, ExpressionPtr rhs)
    : BinaryExpression(std::move(lhs), std::move(rhs))
{
    FPExpect(mLhs && mRhs, "missing mandatory argument");
}

EvaluationContext CombiningUnion::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    // according to http://hl7.org/fhirpath/N1/#unionother-collection
    // "There is no expectation of order in the resulting collection."
    static constexpr auto quickOrder = [](const auto& l, const auto& r) {
        return is_lt(l->quickCompareTo(*r));
    };
    static constexpr auto equals = [](const auto& l, const auto& r) {
        return l->type() == r->type() && l->equals(*r).value_or(false);
    };

    auto lhs = mLhs->eval(context);
    if (! mLhs->returnsSorted())
    {
        std::ranges::sort(lhs.collection, quickOrder);
    }
    auto rhs = mRhs->eval(context);
    if (! mRhs->returnsSorted())
    {
        std::ranges::sort(rhs.collection, quickOrder);
    }

    auto ret = context();
    auto lhsIt = lhs.collection.begin();
    auto rhsIt = rhs.collection.begin();
    ret.collection.reserve(lhs.collection.size() + rhs.collection.size());
    auto appendUniue = [&](auto& it) {
        if (ret.collection.empty() || ! equals(ret.collection.back(), *it))
        {
            ret.collection.emplace_back(std::move(*it));
        }
    };

    // both collections / lhs and rhs are now sorted
    // merge both as done by merge-sort thereby skip any duplicates:
    while (lhsIt != lhs.collection.end() || rhsIt != rhs.collection.end())
    {
        if (rhsIt == rhs.collection.end() || (lhsIt != lhs.collection.end() && quickOrder(*lhsIt, *rhsIt)))
        {
            appendUniue(lhsIt);
            ++lhsIt;
        }
        else
        {
            appendUniue(rhsIt);
            ++rhsIt;
        }
    }
    return ret;
}

bool fhirtools::CombiningUnion::returnsSorted() const
{
    // returned result will always be sorted by quickCompareTo
    return true;
}

EvaluationContext CombiningCombine::eval(const EvaluationContext& context) const
{
    auto ret = context;
    ret.collection.append(mArg->eval(context).collection);
    return ret;
}
}
