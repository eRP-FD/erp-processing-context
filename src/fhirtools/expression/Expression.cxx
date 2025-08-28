/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/expression/Expression.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/ExpressionTrace.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"

#include <boost/algorithm/string/case_conv.hpp>
#include <date/tz.h>
#include <algorithm>
#include <source_location>
#include <utility>

using namespace fhirtools;

Expression::Expression(std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView)
    : mFhirStructureRepository(std::move(fhirStructureRepositoryView))
{
}

const std::shared_ptr<const fhirtools::FhirStructureRepositoryView>&
fhirtools::Expression::fhirStructureRepository() const
{
    return mFhirStructureRepository;
}

EvaluationContext InvocationExpression::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    return mRhs->eval(mLhs->eval(context));
}

PathSelection::PathSelection(std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView,
                             const std::string& subElement)
    : Expression(std::move(fhirStructureRepositoryView))
    , mSubElement(subElement)
{
}

EvaluationContext PathSelection::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    auto ret  = context();
    for (const auto& item : context.collection)
    {
        if (item->isResource() && item->resourceType() == mSubElement)
        {
            ret.collection.emplace_back(item);
        }
        else
        {
            for (const auto& expanded : item->definitionPointer().expandedNames(mSubElement))
            {
                ret.collection.append(item->subElements(expanded));
            }
        }
    }
    return ret;
}

std::string PathSelection::debugInfo() const
{
    return "path=" + mSubElement;
}

EvaluationContext DollarThis::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    return context;
}

EvaluationContext PercentContext::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    return context(context.context);
}

BinaryExpression::BinaryExpression(
    std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView, ExpressionPtr lhs,
    ExpressionPtr rhs)
    : Expression(std::move(fhirStructureRepositoryView))
    , mLhs(std::move(lhs))
    , mRhs(std::move(rhs))
{
}

EvaluationContext TreeNavigationChildren::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    auto ret = context();
    for (const auto& item : context.collection)
    {
        for (const auto& name : item->subElementNames())
        {
            ret.collection.append(item->subElements(name));
        }
    }
    return ret;
}

EvaluationContext TreeNavigationDescendants::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    auto ret = context();
    iterate(context.collection, ret);
    return ret;
}

//NOLINTNEXTLINE(misc-no-recursion)
void TreeNavigationDescendants::iterate(const Collection& collection, EvaluationContext& output) const
{
    Collection ret;
    for (const auto& item : collection)
    {
        for (const auto& name : item->subElementNames())
        {
            ret.append(item->subElements(name));
        }
    }
    if (! ret.empty())
    {
        iterate(ret, output);
        output.collection.append(std::move(ret));
    }
}

EvaluationContext UtilityTrace::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    const std::string name = mLhs ? mLhs->eval(context).collection.single()->asString() : "";
    std::ostringstream oss;
    oss << name << ": ";
    if (mRhs)
    {
        auto projectionResult = mRhs->eval(context);
        oss << projectionResult.collection;
    }
    else
    {
        oss << context.collection;
    }
    TVLOG(3) << oss.str();
    return context;
}

EvaluationContext fhirtools::UtilityToday::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    auto germanTimestamp = date::make_zoned("Europe/Berlin", std::chrono::system_clock::now());
    auto germanDay = date::floor<date::days>(germanTimestamp.get_local_time());
    Date germanDate{date::year_month_day{germanDay}, Date::Precision::day};
    return context.makeDateElement(germanDate);
}


TypesIsOperator::TypesIsOperator(
    std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView, ExpressionPtr expression,
    const std::string& type)
    : Expression(std::move(fhirStructureRepositoryView))
    , mExpression(std::move(expression))
    , mType(type)
{
}

TypesIsOperator::TypesIsOperator(
    std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView,
    ExpressionPtr typeExpression)
    : Expression(std::move(fhirStructureRepositoryView))
    , mTypeExpression(std::move(typeExpression))
{
}

EvaluationContext TypesIsOperator::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    const auto& lhs = mExpression ? mExpression->eval(context) : context;
    auto type = mTypeExpression ? mTypeExpression->eval(context).collection.single()->asString() : mType;

    if (const auto* def = fhirStructureRepository()->findTypeById(type))
    {
        type = def->url();
    }

    const auto& element = lhs.collection.singleOrEmpty();
    return context.
        makeBoolElement(element && element->getStructureDefinition()->isDerivedFrom(*fhirStructureRepository(), type));
}

TypeAsOperator::TypeAsOperator(
    std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView, ExpressionPtr expression,
    std::string type)
    : Expression(std::move(fhirStructureRepositoryView))
    , mExpression(std::move(expression))
    , mType(std::move(type))
{
    if (const auto* def = fhirStructureRepository()->findTypeById(mType))
    {
        mType = def->url();
    }
}

EvaluationContext TypeAsOperator::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    const auto& lhs = mExpression ? mExpression->eval(context) : context;
    const auto& element = lhs.collection.singleOrEmpty();
    if (element && element->getStructureDefinition()->isDerivedFrom(*fhirStructureRepository(), mType))
    {
        return lhs;
    }
    return context();
}

EvaluationContext TypeAsFunction::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    // the implemented behaviour is not specified, but is used in existing expressions and therefor needed.
    const auto& lhs = mExpression ? mExpression->eval(context) : context;
    auto ret = context();
    for (const auto& element : lhs.collection)
    {
        if (element && element->getStructureDefinition()->isDerivedFrom(*fhirStructureRepository(), mType))
        {
            ret.collection.push_back(element);
        }
    }
    return ret;
}

EvaluationContext CollectionsInOperator::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    if (! mLhs)
    {
        return context();
    }
    if (! mRhs)
    {
        return context.makeBoolElement(false);
    }
    const auto lhs = mLhs->eval(context);
    const auto rhs = mRhs->eval(context);
    if (lhs.collection.empty())
    {
        return context();
    }
    if (rhs.collection.empty())
    {
        return context.makeBoolElement(false);
    }
    return context.makeBoolElement(rhs.collection.contains(lhs.collection.single()));
}

CollectionsContainsOperator::CollectionsContainsOperator(
    std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView, ExpressionPtr lhs,
    ExpressionPtr rhs)
    : CollectionsInOperator(std::move(fhirStructureRepositoryView), std::move(rhs), std::move(lhs))
{
}
