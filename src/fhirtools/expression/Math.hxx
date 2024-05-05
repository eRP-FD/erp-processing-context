/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_MATH_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_MATH_HXX

#include "fhirtools/expression/Expression.hxx"

namespace fhirtools
{
// http://hl7.org/fhirpath/N1/#addition
class MathPlusOperator : public BinaryExpression
{
};

// http://hl7.org/fhirpath/N1/#mod
class MathModOperator : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "mod";
    using BinaryExpression::BinaryExpression;
    Collection eval(const Collection& collection) const override;

private:
    Collection mod(const Element& lhs, const Element& rhs, Element::Type type) const;
};

// http://hl7.org/fhirpath/N1/#abs-integer-decimal-quantity
class MathAbs : public Expression
{
};

// http://hl7.org/fhirpath/N1/#ceiling-integer
class MathCeiling : public Expression
{
};

// http://hl7.org/fhirpath/N1/#exp-decimal
class MathExp : public Expression
{
};

// http://hl7.org/fhirpath/N1/#floor-integer
class MathFloor : public Expression
{
};

// http://hl7.org/fhirpath/N1/#ln-decimal
class MathLn : public Expression
{
};

// http://hl7.org/fhirpath/N1/#logbase-decimal-decimal
class MathLog : public UnaryExpression
{
};

// http://hl7.org/fhirpath/N1/#powerexponent-integer-decimal-integer-decimal
class MathPower : public UnaryExpression
{
};

// http://hl7.org/fhirpath/N1/#roundprecision-integer-decimal
class MathRound : public UnaryExpression
{
};

// http://hl7.org/fhirpath/N1/#sqrt-decimal
class MathSqrt : public Expression
{
};

// http://hl7.org/fhirpath/N1/#truncate-integer
class MathTruncate : public Expression
{
};
}// fhirtools

#endif//ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_MATH_HXX
