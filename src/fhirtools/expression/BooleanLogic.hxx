/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_BOOLEANLOGIC_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_BOOLEANLOGIC_HXX

#include "fhirtools/expression/Expression.hxx"

namespace fhirtools
{
// http://hl7.org/fhirpath/N1/#and
class BooleanAndOperator : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "and";
    using BinaryExpression::BinaryExpression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#or
class BooleanOrOperator : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "or";
    using BinaryExpression::BinaryExpression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#not-boolean
class BooleanNotOperator : public Expression
{
public:
    static constexpr auto IDENTIFIER = "not";
    using Expression::Expression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#xor
class BooleanXorOperator : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "xor";
    using BinaryExpression::BinaryExpression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#implies
class BooleanImpliesOperator : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "implies";
    using BinaryExpression::BinaryExpression;
    Collection eval(const Collection& collection) const override;
};
}

#endif//ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_BOOLEANLOGIC_HXX
