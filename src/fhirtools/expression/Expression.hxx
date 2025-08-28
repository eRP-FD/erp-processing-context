/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef FHIR_TOOLS_SRC_FHIR_PATH_EXPRESSION_EXPRESSION_HXX
#define FHIR_TOOLS_SRC_FHIR_PATH_EXPRESSION_EXPRESSION_HXX


#include "fhirtools/model/Element.hxx"
#include "fhirtools/expression/EvaluationContext.hxx"

#include <any>
#include <memory>

namespace fhirtools
{

class Expression
{
public:
    virtual ~Expression() = default;
    explicit Expression(std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView);
    [[nodiscard]] virtual EvaluationContext eval(const EvaluationContext&) const = 0;

    virtual std::string debugInfo() const
    {
        return "";
    }

protected:

    const std::shared_ptr<const fhirtools::FhirStructureRepositoryView>& fhirStructureRepository() const;

private:
    std::shared_ptr<const fhirtools::FhirStructureRepositoryView> mFhirStructureRepository;
};

using ExpressionPtr = std::shared_ptr<Expression>;

class UnaryExpression : public Expression
{
public:
    explicit UnaryExpression(std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView,
                             ExpressionPtr arg);

protected:
    ExpressionPtr mArg;
};

class BinaryExpression : public Expression
{
public:
    BinaryExpression(std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView,
                     ExpressionPtr lhs, ExpressionPtr rhs);

protected:
    ExpressionPtr mLhs;
    ExpressionPtr mRhs;
};

// expression '.' invocation
class InvocationExpression : public BinaryExpression
{
public:
    using BinaryExpression::BinaryExpression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#path-selection
class PathSelection : public Expression
{
public:
    explicit PathSelection(std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView,
                           const std::string& subElement);
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;

    std::string debugInfo() const override;

private:
    std::string mSubElement;
};

// '$this' #thisInvocation
class DollarThis : public Expression
{
public:
    using Expression::Expression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// '$index' #indexInvocation
class DollarIndex : public Expression
{
public:
};

// '$total' #totalInvocation
class DollarTotal : public Expression
{
public:
};

// external constant '%context'
class PercentContext : public Expression
{
public:
    using Expression::Expression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#children-collection
class TreeNavigationChildren : public Expression
{
public:
    static constexpr auto IDENTIFIER = "children";
    using Expression::Expression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#descendants-collection
class TreeNavigationDescendants : public Expression
{
public:
    static constexpr auto IDENTIFIER = "descendants";
    using Expression::Expression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;

private:
    void iterate(const Collection& collection, EvaluationContext& output) const;
};

// http://hl7.org/fhirpath/N1/#tracename-string-projection-expression-collection
class UtilityTrace : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "trace";
    using BinaryExpression::BinaryExpression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#now-datetime
class UtilityNow : public Expression
{
};

// http://hl7.org/fhirpath/N1/#timeofday-time
class UtilityTimeOfDay : public Expression
{
};

// http://hl7.org/fhirpath/N1/#today-date
class UtilityToday : public Expression
{
public:
    static constexpr auto IDENTIFIER = "today";
    using Expression::Expression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};


// http://hl7.org/fhirpath/N1/#is-type-specifier
class TypesIsOperator : public Expression
{
public:
    static constexpr auto IDENTIFIER = "is";
    explicit TypesIsOperator(std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView,
                             const ExpressionPtr expression, const std::string& type);
    explicit TypesIsOperator(std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView,
                             const ExpressionPtr typeExpression);
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;

private:
    ExpressionPtr mExpression;
    ExpressionPtr mTypeExpression;
    std::string mType;
};

// http://hl7.org/fhirpath/N1/#as-type-specifier
class TypeAsOperator : public Expression
{
public:
    static constexpr auto IDENTIFIER = "as";
    explicit TypeAsOperator(std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView,
                            ExpressionPtr expression, std::string type);
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;

protected:
    ExpressionPtr mExpression;
    std::string mType;
};

// http://hl7.org/fhirpath/N1/#astype-type-specifier
class TypeAsFunction : public TypeAsOperator
{
public:
    using TypeAsOperator::TypeAsOperator;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#in-membership
class CollectionsInOperator : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "in";
    using BinaryExpression::BinaryExpression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#contains-containership
class CollectionsContainsOperator : public CollectionsInOperator
{
public:
    static constexpr auto IDENTIFIER = "contains";
    CollectionsContainsOperator(
        std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView, ExpressionPtr lhs,
        ExpressionPtr rhs);
};
}
#endif//FHIR_TOOLS_SRC_FHIR_PATH_EXPRESSION_EXPRESSION_HXX
