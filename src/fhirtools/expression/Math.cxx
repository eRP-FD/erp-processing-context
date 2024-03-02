/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/expression/Math.hxx"
#include "fhirtools/FPExpect.hxx"
namespace fhirtools
{
Collection MathModOperator::eval(const Collection& collection) const
{
    FPExpect(mLhs && mRhs, "Missing argument for mod operator");
    const auto lhs = mLhs->eval(collection).single();
    const auto rhs = mRhs->eval(collection).single();
    if (isImplicitConvertible(lhs->type(), rhs->type()))
    {
        return mod(*lhs, *rhs, rhs->type());
    }
    else if (isImplicitConvertible(rhs->type(), lhs->type()))
    {
        return mod(*lhs, *rhs, lhs->type());
    }
    FPFail("incompatible operand types for mod operator");
}

Collection MathModOperator::mod(const Element& lhs, const Element& rhs, Element::Type type) const
{
    switch (type)
    {
        case Element::Type::Integer:
            if (rhs.asInt() == 0)
            {
                return {};
            }
            return {makeIntegerElement(lhs.asInt() % rhs.asInt())};
        case Element::Type::Decimal: {
            if (rhs.asDecimal() == 0)
            {
                return {};
            }
            return {makeDecimalElement(boost::multiprecision::fmod(lhs.asDecimal(), rhs.asDecimal()))};
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
