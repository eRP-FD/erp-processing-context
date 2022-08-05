/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_COMPARISON_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_COMPARISON_HXX

#include "fhirtools/expression/Expression.hxx"

namespace fhirtools
{

// http://hl7.org/fhirpath/N1/#equals
class EqualityEqualsOperator : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "=";
    using BinaryExpression::BinaryExpression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#equivalent
class EqualityEquivalentOperator : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "~";
};

// http://hl7.org/fhirpath/N1/#not-equals
class EqualityNotEqualsOperator : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "!=";
    using BinaryExpression::BinaryExpression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#not-equivalent
class EqualityNotEquivalentOperator : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "!~";
};

// http://hl7.org/fhirpath/N1/#greater-than
class ComparisonOperatorGreaterThan : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = ">";
    using BinaryExpression::BinaryExpression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#greater-or-equal
class ComparisonOperatorGreaterOrEqual : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = ">=";
    using BinaryExpression::BinaryExpression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#less-than
class ComparisonOperatorLessThan : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "<";
    using BinaryExpression::BinaryExpression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#less-or-equal
class ComparisonOperatorLessOrEqual : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "<=";
    using BinaryExpression::BinaryExpression;
    Collection eval(const Collection& collection) const override;
};

}// fhirtools

#endif//ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_COMPARISON_HXX
