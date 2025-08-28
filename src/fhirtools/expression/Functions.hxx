/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_FUNCTIONS_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_FUNCTIONS_HXX

#include "fhirtools/expression/Expression.hxx"

namespace fhirtools
{
// http://hl7.org/fhirpath/N1/#empty-boolean
class ExistenceEmpty : public Expression
{
public:
    static constexpr auto IDENTIFIER = "empty";
    using Expression::Expression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#existscriteria-expression-boolean
class ExistenceExists : public UnaryExpression
{
public:
    static constexpr auto IDENTIFIER = "exists";
    using UnaryExpression::UnaryExpression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#allcriteria-expression-boolean
class ExistenceAll : public UnaryExpression
{
public:
    static constexpr auto IDENTIFIER = "all";
    using UnaryExpression::UnaryExpression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#alltrue-boolean
class ExistenceAllTrue : public Expression
{
public:
    static constexpr auto IDENTIFIER = "allTrue";
    using Expression::Expression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#anytrue-boolean
class ExistenceAnyTrue : public Expression
{
public:
    static constexpr auto IDENTIFIER = "anyTrue";
    using Expression::Expression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#allfalse-boolean
class ExistenceAllFalse : public Expression
{
public:
    static constexpr auto IDENTIFIER = "allFalse";
    using Expression::Expression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#anyfalse-boolean
class ExistenceAnyFalse : public Expression
{
public:
    static constexpr auto IDENTIFIER = "anyFalse";
    using Expression::Expression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#subsetofother-collection-boolean
class ExistenceSubsetOf : public UnaryExpression
{
};

// http://hl7.org/fhirpath/N1/#supersetofother-collection-boolean
class ExistenceSupersetOf : public UnaryExpression
{
};

// http://hl7.org/fhirpath/N1/#count-integer
class ExistenceCount : public Expression
{
public:
    static constexpr auto IDENTIFIER = "count";
    using Expression::Expression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#distinct-collection
class ExistenceDistinct : public Expression
{
public:
    static constexpr auto IDENTIFIER = "distinct";
    using Expression::Expression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#isdistinct-boolean
class ExistenceIsDistinct : public Expression
{
public:
    static constexpr auto IDENTIFIER = "isDistinct";
    using Expression::Expression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#wherecriteria-expression-collection
class FilteringWhere : public UnaryExpression
{
public:
    static constexpr auto IDENTIFIER = "where";
    using UnaryExpression::UnaryExpression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#selectprojection-expression-collection
class FilteringSelect : public UnaryExpression
{
public:
    static constexpr auto IDENTIFIER = "select";
    using UnaryExpression::UnaryExpression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#repeatprojection-expression-collection
class FilteringRepeat : public UnaryExpression
{
};

// http://hl7.org/fhirpath/N1/#oftypetype-type-specifier-collection
class FilteringOfType : public UnaryExpression
{
public:
    static constexpr auto IDENTIFIER = "ofType";
    using UnaryExpression::UnaryExpression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#index-integer-collection
class SubsettingIndexer : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "indexer";
    SubsettingIndexer(std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView,
                      ExpressionPtr lhs, ExpressionPtr rhs);
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#single-collection
class SubsettingSingle : public Expression
{
};

// http://hl7.org/fhirpath/N1/#first-collection
class SubsettingFirst : public Expression
{
public:
    static constexpr auto IDENTIFIER = "first";
    using Expression::Expression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#last-collection
class SubsettingLast : public Expression
{
public:
    static constexpr auto IDENTIFIER = "last";
    using Expression::Expression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#tail-collection
class SubsettingTail : public Expression
{
public:
    static constexpr auto IDENTIFIER = "tail";
    using Expression::Expression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#skipnum-integer-collection
class SubsettingSkip : public UnaryExpression
{
};

// http://hl7.org/fhirpath/N1/#takenum-integer-collection
class SubsettingTake : public UnaryExpression
{
};

// http://hl7.org/fhirpath/N1/#intersectother-collection-collection
class SubsettingIntersect : public UnaryExpression
{
public:
    static constexpr auto IDENTIFIER = "intersect";
    explicit SubsettingIntersect(
        std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView, ExpressionPtr arg);
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#excludeother-collection-collection
class SubsettingExclude : public UnaryExpression
{
};

// http://hl7.org/fhirpath/N1/#unionother-collection
class CombiningUnion : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "union";
    CombiningUnion(std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView,
                   ExpressionPtr lhs, ExpressionPtr rhs);
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

using UnionOperator = CombiningUnion;

// http://hl7.org/fhirpath/N1/#combineother-collection-collection
class CombiningCombine : public UnaryExpression
{
public:
    static constexpr auto IDENTIFIER = "combine";
    using UnaryExpression::UnaryExpression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

}


#endif//ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_FUNCTIONS_HXX
