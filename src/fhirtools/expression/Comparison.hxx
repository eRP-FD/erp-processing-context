/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_COMPARISON_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_COMPARISON_HXX

#include "fhirtools/expression/Expression.hxx"

namespace fhirtools
{

class ComparisonOperator : public BinaryExpression
{
public:
    ComparisonOperator(const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository,
                       ExpressionPtr lhs, ExpressionPtr rhs);

protected:
    Collection comparison(const Element& lhs, const Element& rhs,
                          const std::vector<std::strong_ordering>& expected) const;
};

// http://hl7.org/fhirpath/N1/#equals
class EqualityEqualsOperator : public ComparisonOperator
{
public:
    static constexpr auto IDENTIFIER = "=";
    using ComparisonOperator::ComparisonOperator;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#equivalent
class EqualityEquivalentOperator : public ComparisonOperator
{
public:
    static constexpr auto IDENTIFIER = "~";
};

// http://hl7.org/fhirpath/N1/#not-equals
class EqualityNotEqualsOperator : public ComparisonOperator
{
public:
    static constexpr auto IDENTIFIER = "!=";
    using ComparisonOperator::ComparisonOperator;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#not-equivalent
class EqualityNotEquivalentOperator : public ComparisonOperator
{
public:
    static constexpr auto IDENTIFIER = "!~";
};

// http://hl7.org/fhirpath/N1/#greater-than
class ComparisonOperatorGreaterThan : public ComparisonOperator
{
public:
    static constexpr auto IDENTIFIER = ">";
    using ComparisonOperator::ComparisonOperator;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#greater-or-equal
class ComparisonOperatorGreaterOrEqual : public ComparisonOperator
{
public:
    static constexpr auto IDENTIFIER = ">=";
    using ComparisonOperator::ComparisonOperator;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#less-than
class ComparisonOperatorLessThan : public ComparisonOperator
{
public:
    static constexpr auto IDENTIFIER = "<";
    using ComparisonOperator::ComparisonOperator;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#less-or-equal
class ComparisonOperatorLessOrEqual : public ComparisonOperator
{
public:
    static constexpr auto IDENTIFIER = "<=";
    using ComparisonOperator::ComparisonOperator;
    Collection eval(const Collection& collection) const override;
};

}// fhirtools

#endif//ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_COMPARISON_HXX
