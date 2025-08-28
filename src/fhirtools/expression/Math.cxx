/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/expression/Math.hxx"
#include "ExpressionTrace.hxx"
#include "fhirtools/FPExpect.hxx"
namespace fhirtools
{

EvaluationContext MathMinusOperator::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    FPExpect(mLhs && mRhs, "Missing argument for minus operator");
    const auto lhs = mLhs->eval(context).collection.single();
    const auto rhs = mRhs->eval(context).collection.single();
    if (isImplicitConvertible(lhs->type(), rhs->type()))
    {
        return minus(context, *lhs, *rhs, rhs->type());
    }
    if (isImplicitConvertible(rhs->type(), lhs->type()))
    {
        return minus(context, *lhs, *rhs, lhs->type());
    }
    FPFail("incompatible operand types for minus operator");
}

EvaluationContext MathMinusOperator::minus(const EvaluationContext& context, const Element& lhs, const Element& rhs,
                                           Element::Type type)
{
    switch (type)
    {
        case Element::Type::Integer:
            return context.makeIntegerElement(lhs.asInt() - rhs.asInt());
        case Element::Type::Decimal:
            return context.makeDecimalElement(lhs.asDecimal() - rhs.asDecimal());
        case Element::Type::Quantity: {
            const auto lhsAsQuantity = lhs.asQuantity();
            const auto rhsAsQuantity = rhs.asQuantity();
            if (lhsAsQuantity.unit() == rhsAsQuantity.unit())
            {
                return context.makeQuantityElement({lhsAsQuantity.value() - rhsAsQuantity.value(), lhsAsQuantity.unit()});
            }
            break;
        }
        case Element::Type::String:
        case Element::Type::Boolean:
        case Element::Type::Date:
        case Element::Type::DateTime:
        case Element::Type::Time:
        case Element::Type::Structured:
            break;
    }
    FPFail("incompatible operand types for - operator");
}

EvaluationContext MathModOperator::eval(const EvaluationContext& context) const
{
    FPExpect(mLhs && mRhs, "Missing argument for mod operator");
    const auto lhs = mLhs->eval(context).collection.single();
    const auto rhs = mRhs->eval(context).collection.single();
    if (isImplicitConvertible(lhs->type(), rhs->type()))
    {
        return mod(context, *lhs, *rhs, rhs->type());
    }
    else if (isImplicitConvertible(rhs->type(), lhs->type()))
    {
        return mod(context, *lhs, *rhs, lhs->type());
    }
    FPFail("incompatible operand types for mod operator");
}

EvaluationContext MathModOperator::mod(const EvaluationContext& context, const Element& lhs, const Element& rhs,
                                       Element::Type type)
{
    switch (type)
    {
        case Element::Type::Integer:
            if (rhs.asInt() == 0)
            {
                return context();
            }
            return context.makeIntegerElement(lhs.asInt() % rhs.asInt());
        case Element::Type::Decimal: {
            if (rhs.asDecimal() == 0)
            {
                return context();
            }
            return context.makeDecimalElement(boost::multiprecision::fmod(lhs.asDecimal(), rhs.asDecimal()));
        }
        case Element::Type::String:
        case Element::Type::Boolean:
        case Element::Type::Date:
        case Element::Type::DateTime:
        case Element::Type::Time:
        case Element::Type::Quantity:
        case Element::Type::Structured:
            break;
    }
    FPFail("unsupported type for mod operator");
}

}
