/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
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

UnaryExpression::UnaryExpression(
    std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView, ExpressionPtr arg)
    : Expression(std::move(fhirStructureRepositoryView))
    , mArg(std::move(arg))
{
}

//NOLINTNEXTLINE(misc-no-recursion)
EvaluationContext ExistenceExists::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    if (! mArg)
    {
        return context.makeBoolElement(!context.collection.empty());
    }
    return ExistenceExists(fhirStructureRepository(), nullptr)
        .eval(FilteringWhere(fhirStructureRepository(), mArg).eval(context));
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
    return context.makeBoolElement(std::make_shared<ExistenceDistinct>(fhirStructureRepository())->eval(context).collection.size() ==
                            context.collection.size());
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
    const auto* type = fhirStructureRepository()->findTypeById(arg->asString());
    FPExpect(type, "could not resolve type " + arg->asString());
    auto ret = context();
    for (const auto& item : context.collection)
    {
        if (item->getStructureDefinition()->isSystemType() &&
            type->kind() == FhirStructureDefinition::Kind::primitiveType)
        {
            if (item->getStructureDefinition()->isDerivedFrom(
                    *fhirStructureRepository(), type->primitiveToSystemType(*fhirStructureRepository()).url()))
            {
                ret.collection.emplace_back(item);
            }
        }
        else
        {
            if (item->getStructureDefinition()->isDerivedFrom(*fhirStructureRepository(), type->url()))
            {
                ret.collection.emplace_back(item);
            }
        }
    }
    return ret;
}

SubsettingIndexer::SubsettingIndexer(
    std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView, ExpressionPtr lhs,
    ExpressionPtr rhs)
    : BinaryExpression(std::move(fhirStructureRepositoryView), std::move(lhs), std::move(rhs))
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

SubsettingIntersect::SubsettingIntersect(
    std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView, ExpressionPtr arg)
    : UnaryExpression(std::move(fhirStructureRepositoryView), std::move(arg))
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

CombiningUnion::CombiningUnion(
    std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView, ExpressionPtr lhs,
    ExpressionPtr rhs)
    : BinaryExpression(std::move(fhirStructureRepositoryView), std::move(lhs), std::move(rhs))
{
    FPExpect(mLhs && mRhs, "missing mandatory argument");
}

EvaluationContext CombiningUnion::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    auto lhs = mLhs->eval(context);
    auto rhs = mRhs->eval(context);
    auto ret = context();
    for (const auto& item : lhs.collection)
    {
        if (! ret.collection.contains(item))
        {
            ret.collection.push_back(item);
        }
    }
    for (const auto& item : rhs.collection)
    {
        if (! ret.collection.contains(item))
        {
            ret.collection.push_back(item);
        }
    }
    return ret;
}

EvaluationContext CombiningCombine::eval(const EvaluationContext& context) const
{
    auto ret = context;
    ret.collection.append(mArg->eval(context).collection);
    return ret;
}
}
