/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/expression/Functions.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/ExpressionTrace.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"

namespace fhirtools
{
Collection ExistenceEmpty::eval(const Collection& collection) const
{
    EVAL_TRACE;
    return {makeBoolElement(collection.empty())};
}

UnaryExpression::UnaryExpression(
    const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository, ExpressionPtr arg)
    : Expression(fhirStructureRepository)
    , mArg(std::move(arg))
{
}

//NOLINTNEXTLINE(misc-no-recursion)
Collection ExistenceExists::eval(const Collection& collection) const
{
    EVAL_TRACE;
    if (! mArg)
    {
        return {makeBoolElement(! collection.empty())};
    }
    return ExistenceExists(mFhirStructureRepository, nullptr)
        .eval(FilteringWhere(mFhirStructureRepository, mArg).eval(collection));
}

Collection ExistenceAll::eval(const Collection& collection) const
{
    EVAL_TRACE;
    FPExpect(mArg, "no criterion argument given to all(...)");
    for (const auto& item : collection)
    {
        auto result = mArg->eval({item}).boolean();
        if (! result || ! result->asBool())
        {
            return {makeBoolElement(false)};
        }
    }
    return {makeBoolElement(true)};
}

Collection ExistenceAllTrue::eval(const Collection& collection) const
{
    EVAL_TRACE;
    for (const auto& item : collection)
    {
        if (! item->asBool())
        {
            return {makeBoolElement(false)};
        }
    }
    return {makeBoolElement(true)};
}

Collection ExistenceAnyTrue::eval(const Collection& collection) const
{
    EVAL_TRACE;
    for (const auto& item : collection)
    {
        if (item->asBool())
        {
            return {makeBoolElement(true)};
        }
    }
    return {makeBoolElement(false)};
}

Collection ExistenceAllFalse::eval(const Collection& collection) const
{
    EVAL_TRACE;
    for (const auto& item : collection)
    {
        if (item->asBool())
        {
            return {makeBoolElement(false)};
        }
    }
    return {makeBoolElement(true)};
}

Collection ExistenceAnyFalse::eval(const Collection& collection) const
{
    EVAL_TRACE;
    for (const auto& item : collection)
    {
        if (! item->asBool())
        {
            return {makeBoolElement(true)};
        }
    }
    return {makeBoolElement(false)};
}

Collection ExistenceCount::eval(const Collection& collection) const
{
    EVAL_TRACE;
    return {makeIntegerElement(collection.size())};
}

Collection ExistenceDistinct::eval(const Collection& collection) const
{
    EVAL_TRACE;
    Collection ret;
    std::ranges::copy_if(collection, std::back_insert_iterator(ret), [&ret](const Collection::value_type& val) {
        return ! ret.contains(val);
    });
    return ret;
}

Collection ExistenceIsDistinct::eval(const Collection& collection) const
{
    EVAL_TRACE;
    return {makeBoolElement(std::make_shared<ExistenceDistinct>(mFhirStructureRepository)->eval(collection).size() ==
                            collection.size())};
}

Collection FilteringWhere::eval(const Collection& collection) const
{
    EVAL_TRACE;
    FPExpect(mArg, "no criterion argument given to where(...)");
    Collection ret;
    for (const auto& item : collection)
    {
        const auto result = mArg->eval({item}).boolean();
        if (result && result->asBool())
        {
            ret.emplace_back(item);
        }
    }
    return ret;
}

Collection FilteringSelect::eval(const Collection& collection) const
{
    EVAL_TRACE;
    FPExpect(mArg, "no projection argument given to select(...)");
    Collection ret;
    for (const auto& item : collection)
    {
        ret.append(mArg->eval({item}));
    }
    return ret;
}

Collection FilteringOfType::eval(const Collection& collection) const
{
    EVAL_TRACE;
    FPExpect(mArg, "type specifier not specified");
    const auto arg = mArg->eval(collection).singleOrEmpty();
    FPExpect(arg, "type not specified");
    const auto* type = mFhirStructureRepository->findTypeById(arg->asString());
    FPExpect(type, "could not resolve type " + arg->asString());
    Collection ret;
    for (const auto& item : collection)
    {
        if (item->getStructureDefinition()->isSystemType() &&
            type->kind() == FhirStructureDefinition::Kind::primitiveType)
        {
            if (item->getStructureDefinition()->isDerivedFrom(
                    *mFhirStructureRepository, type->primitiveToSystemType(*mFhirStructureRepository).url()))
            {
                ret.emplace_back(item);
            }
        }
        else
        {
            if (item->getStructureDefinition()->isDerivedFrom(*mFhirStructureRepository, type->url()))
            {
                ret.emplace_back(item);
            }
        }
    }
    return ret;
}

SubsettingIndexer::SubsettingIndexer(
    const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository, ExpressionPtr lhs,
    ExpressionPtr rhs)
    : BinaryExpression(fhirStructureRepository, std::move(lhs), std::move(rhs))
{
    FPExpect(mLhs && mRhs, "missing mandatory argument");
}

Collection SubsettingIndexer::eval(const Collection& collection) const
{
    EVAL_TRACE;
    auto lhs = mLhs->eval(collection);
    auto index = static_cast<size_t>(mRhs->eval(collection).single()->asInt());
    if (lhs.size() <= index)
    {
        return {};
    }
    return {lhs[index]};
}

Collection SubsettingFirst::eval(const Collection& collection) const
{
    EVAL_TRACE;
    return collection.empty() ? Collection{} : Collection{collection[0]};
}

Collection SubsettingLast::eval(const Collection& collection) const
{
    EVAL_TRACE;
    return collection.empty() ? Collection{} : Collection{collection.back()};
}

Collection SubsettingTail::eval(const Collection& collection) const
{
    EVAL_TRACE;
    return collection.empty() ? Collection{} : Collection{collection.begin() + 1, collection.end()};
}

SubsettingIntersect::SubsettingIntersect(
    const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository, ExpressionPtr arg)
    : UnaryExpression(fhirStructureRepository, std::move(arg))
{
    FPExpect(mArg, "missing mandatory argument");
}

Collection SubsettingIntersect::eval(const Collection& collection) const
{
    EVAL_TRACE;
    auto other = mArg->eval(collection);
    Collection ret;
    for (const auto& item : collection)
    {
        if (other.contains(item) && ! ret.contains(item))
        {
            ret.push_back(item);
        }
    }
    return ret;
}

CombiningUnion::CombiningUnion(const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository,
                               ExpressionPtr lhs, ExpressionPtr rhs)
    : BinaryExpression(fhirStructureRepository, std::move(lhs), std::move(rhs))
{
    FPExpect(mLhs && mRhs, "missing mandatory argument");
}

Collection CombiningUnion::eval(const Collection& collection) const
{
    EVAL_TRACE;
    auto lhs = mLhs->eval(collection);
    auto rhs = mRhs->eval(collection);
    Collection ret;
    for (const auto& item : lhs)
    {
        if (! ret.contains(item))
        {
            ret.push_back(item);
        }
    }
    for (const auto& item : rhs)
    {
        if (! ret.contains(item))
        {
            ret.push_back(item);
        }
    }
    return ret;
}

Collection CombiningCombine::eval(const Collection& collection) const
{
    Collection ret = collection;
    ret.append(mArg->eval(collection));
    return ret;
}
}
