/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/expression/Expression.hxx"

#include <boost/algorithm/string/case_conv.hpp>
#include <algorithm>
#include <source_location>
#include <utility>

#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/ExpressionTrace.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"

using namespace fhirtools;

Expression::Expression(const FhirStructureRepository* fhirStructureRepository)
    : mFhirStructureRepository(fhirStructureRepository)
{
}

std::shared_ptr<PrimitiveElement> Expression::makeBoolElement(bool value) const
{
    return std::make_shared<PrimitiveElement>(mFhirStructureRepository, Element::Type::Boolean, value);
}
std::shared_ptr<PrimitiveElement> Expression::makeIntegerElement(int32_t value) const
{
    return std::make_shared<PrimitiveElement>(mFhirStructureRepository, Element::Type::Integer, value);
}
std::shared_ptr<PrimitiveElement> Expression::makeIntegerElement(size_t value) const
{
    FPExpect(value < std::numeric_limits<int32_t>::max(), "value too large for int32");
    return std::make_shared<PrimitiveElement>(mFhirStructureRepository, Element::Type::Integer,
                                              static_cast<int32_t>(value));
}
std::shared_ptr<PrimitiveElement> Expression::makeDecimalElement(DecimalType value) const
{
    return std::make_shared<PrimitiveElement>(mFhirStructureRepository, Element::Type::Decimal, value);
}
std::shared_ptr<PrimitiveElement> Expression::makeStringElement(const std::string_view& value) const
{
    return std::make_shared<PrimitiveElement>(mFhirStructureRepository, Element::Type::String, std::string(value));
}
std::shared_ptr<PrimitiveElement> Expression::makeDateElement(const fhirtools::Date& value) const
{
    return std::make_shared<PrimitiveElement>(mFhirStructureRepository, Element::Type::Date, value);
}
std::shared_ptr<PrimitiveElement> Expression::makeDateTimeElement(const fhirtools::DateTime& value) const
{
    return std::make_shared<PrimitiveElement>(mFhirStructureRepository, Element::Type::DateTime, value);
}
std::shared_ptr<PrimitiveElement> Expression::makeTimeElement(const fhirtools::Time& value) const
{
    return std::make_shared<PrimitiveElement>(mFhirStructureRepository, Element::Type::Time, value);
}
std::shared_ptr<PrimitiveElement> Expression::makeQuantityElement(const Element::QuantityType& value) const
{
    return std::make_shared<PrimitiveElement>(mFhirStructureRepository, Element::Type::Quantity, value);
}

Collection InvocationExpression::eval(const Collection& collection) const
{
    EVAL_TRACE;
    return mRhs->eval(mLhs->eval(collection));
}

PathSelection::PathSelection(const FhirStructureRepository* fhirStructureRepository, const std::string& subElement)
    : Expression(fhirStructureRepository)
    , mSubElement(subElement)
{
}

Collection PathSelection::eval(const Collection& collection) const
{
    EVAL_TRACE;
    Collection retCollection;
    for (const auto& item : collection)
    {
        if (item->isResource() && item->resourceType() == mSubElement)
        {
            retCollection.emplace_back(item);
        }
        else
        {
            for (const auto& expanded : item->definitionPointer().expandedNames(mSubElement))
            {
                retCollection.append(item->subElements(expanded));
            }
        }
    }
    return retCollection;
}

std::string PathSelection::debugInfo() const
{
    return "path=" + mSubElement;
}

Collection DollarThis::eval(const Collection& collection) const
{
    EVAL_TRACE;
    return collection;
}

Collection PercentContext::eval(const Collection& collection) const
{
    EVAL_TRACE;
    auto element = collection.singleOrEmpty();
    while (element)
    {
        if (element->isContextElement() || ! element->parent())
        {
            return {element};
        }
        element = element->parent();
    }
    return {};
}

BinaryExpression::BinaryExpression(const FhirStructureRepository* fhirStructureRepository, ExpressionPtr lhs,
                                   ExpressionPtr rhs)
    : Expression(fhirStructureRepository)
    , mLhs(std::move(lhs))
    , mRhs(std::move(rhs))
{
}

Collection TreeNavigationChildren::eval(const Collection& collection) const
{
    EVAL_TRACE;
    Collection ret;
    for (const auto& item : collection)
    {
        for (const auto& name : item->subElementNames())
        {
            ret.append(item->subElements(name));
        }
    }
    return ret;
}

Collection TreeNavigationDescendants::eval(const Collection& collection) const
{
    EVAL_TRACE;
    Collection ret;
    iterate(collection, ret);
    return ret;
}

//NOLINTNEXTLINE(misc-no-recursion)
void TreeNavigationDescendants::iterate(const Collection& collection, Collection& output) const
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
        output.append(std::move(ret));
    }
}

Collection UtilityTrace::eval(const Collection& collection) const
{
    EVAL_TRACE;
    const std::string name = mLhs ? mLhs->eval(collection).single()->asString() : "";
    std::ostringstream oss;
    oss << name << ": ";
    if (mRhs)
    {
        auto projectionResult = mRhs->eval(collection);
        oss << projectionResult;
    }
    else
    {
        oss << collection;
    }
    TVLOG(3) << oss.str();
    return collection;
}



TypesIsOperator::TypesIsOperator(const FhirStructureRepository* fhirStructureRepository, ExpressionPtr expression,
                                 const std::string& type)
    : Expression(fhirStructureRepository)
    , mExpression(std::move(expression))
    , mType(type)
{
}

TypesIsOperator::TypesIsOperator(const FhirStructureRepository* fhirStructureRepository, ExpressionPtr typeExpression)
    : Expression(fhirStructureRepository)
    , mTypeExpression(std::move(typeExpression))
{
}

Collection TypesIsOperator::eval(const Collection& collection) const
{
    EVAL_TRACE;
    const auto& lhs = mExpression ? mExpression->eval(collection) : collection;
    auto type = mTypeExpression ? mTypeExpression->eval(collection).single()->asString() : mType;

    if (const auto* def = mFhirStructureRepository->findTypeById(type))
    {
        type = def->url();
    }


    const auto& element = lhs.singleOrEmpty();
    return {
        makeBoolElement(element && element->getStructureDefinition()->isDerivedFrom(*mFhirStructureRepository, type))};
}

TypeAsOperator::TypeAsOperator(const FhirStructureRepository* fhirStructureRepository, ExpressionPtr expression,
                               const std::string& type)
    : Expression(fhirStructureRepository)
    , mExpression(std::move(expression))
    , mType(type)
{
    if (const auto* def = mFhirStructureRepository->findTypeById(mType))
    {
        mType = def->url();
    }
}

Collection TypeAsOperator::eval(const Collection& collection) const
{
    EVAL_TRACE;
    const auto& lhs = mExpression ? mExpression->eval(collection) : collection;
    const auto& element = lhs.singleOrEmpty();
    if (element && element->getStructureDefinition()->isDerivedFrom(*mFhirStructureRepository, mType))
    {
        return lhs;
    }
    return {};
}

Collection TypeAsFunction::eval(const Collection& collection) const
{
    EVAL_TRACE;
    // the implemented behaviour is not specified, but is used in existing expressions and therefor needed.
    const auto& lhs = mExpression ? mExpression->eval(collection) : collection;
    Collection ret;
    for (const auto& element : lhs)
    {
        if (element && element->getStructureDefinition()->isDerivedFrom(*mFhirStructureRepository, mType))
        {
            ret.push_back(element);
        }
    }
    return ret;
}

Collection CollectionsInOperator::eval(const Collection& collection) const
{
    EVAL_TRACE;
    if (! mLhs)
    {
        return {};
    }
    if (! mRhs)
    {
        return {makeBoolElement(false)};
    }
    const auto lhs = mLhs->eval(collection);
    const auto rhs = mRhs->eval(collection);
    if (lhs.empty())
    {
        return {};
    }
    if (rhs.empty())
    {
        return {makeBoolElement(false)};
    }
    return {makeBoolElement(rhs.contains(lhs.single()))};
}

CollectionsContainsOperator::CollectionsContainsOperator(const FhirStructureRepository* fhirStructureRepository,
                                                         ExpressionPtr lhs, ExpressionPtr rhs)
    : CollectionsInOperator(fhirStructureRepository, std::move(rhs), std::move(lhs))
{
}
