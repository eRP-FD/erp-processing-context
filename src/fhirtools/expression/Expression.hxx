/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef FHIR_TOOLS_SRC_FHIR_PATH_EXPRESSION_EXPRESSION_HXX
#define FHIR_TOOLS_SRC_FHIR_PATH_EXPRESSION_EXPRESSION_HXX


#include "fhirtools/model/Collection.hxx"
#include "fhirtools/model/Element.hxx"

#include <any>
#include <memory>

namespace fhirtools
{

class Expression
{
public:
    virtual ~Expression() = default;
    explicit Expression(const FhirStructureRepository* fhirStructureRepository);
    virtual Collection eval(const Collection&) const = 0;

    virtual std::string debugInfo() const
    {
        return "";
    }

protected:
    std::shared_ptr<PrimitiveElement> makeIntegerElement(int32_t value) const;
    std::shared_ptr<PrimitiveElement> makeIntegerElement(size_t value) const;
    std::shared_ptr<PrimitiveElement> makeDecimalElement(Element::DecimalType value) const;
    std::shared_ptr<PrimitiveElement> makeStringElement(const std::string_view& value) const;
    std::shared_ptr<PrimitiveElement> makeBoolElement(bool value) const;
    std::shared_ptr<PrimitiveElement> makeDateElement(const Timestamp& value) const;
    std::shared_ptr<PrimitiveElement> makeDateTimeElement(const Timestamp& value) const;
    std::shared_ptr<PrimitiveElement> makeTimeElement(const Timestamp& value) const;
    std::shared_ptr<PrimitiveElement> makeQuantityElement(const Element::QuantityType& value) const;

    const FhirStructureRepository* mFhirStructureRepository;
};
using ExpressionPtr = std::shared_ptr<Expression>;

class UnaryExpression : public Expression
{
public:
    explicit UnaryExpression(const FhirStructureRepository* fhirStructureRepository, ExpressionPtr arg);

protected:
    ExpressionPtr mArg;
};

class BinaryExpression : public Expression
{
public:
    BinaryExpression(const FhirStructureRepository* fhirStructureRepository, ExpressionPtr lhs, ExpressionPtr rhs);

protected:
    ExpressionPtr mLhs;
    ExpressionPtr mRhs;
};

// expression '.' invocation
class InvocationExpression : public BinaryExpression
{
public:
    using BinaryExpression::BinaryExpression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#path-selection
class PathSelection : public Expression
{
public:
    explicit PathSelection(const FhirStructureRepository* fhirStructureRepository, const std::string& subElement);
    Collection eval(const Collection& collection) const override;

    std::string debugInfo() const override;

private:
    std::string mSubElement;
};

// '$this' #thisInvocation
class DollarThis : public Expression
{
public:
    using Expression::Expression;
    Collection eval(const Collection& collection) const override;
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
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#children-collection
class TreeNavigationChildren : public Expression
{
public:
    static constexpr auto IDENTIFIER = "children";
    using Expression::Expression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#descendants-collection
class TreeNavigationDescendants : public Expression
{
public:
    static constexpr auto IDENTIFIER = "descendants";
    using Expression::Expression;
    Collection eval(const Collection& collection) const override;

private:
    void iterate(const Collection& collection, Collection& output) const;
};

// http://hl7.org/fhirpath/N1/#tracename-string-projection-expression-collection
class UtilityTrace : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "trace";
    using BinaryExpression::BinaryExpression;
    Collection eval(const Collection& collection) const override;
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
};



// http://hl7.org/fhirpath/N1/#is-type-specifier
class TypesIsOperator : public Expression
{
public:
    static constexpr auto IDENTIFIER = "is";
    explicit TypesIsOperator(const FhirStructureRepository* fhirStructureRepository, const ExpressionPtr expression,
                             const std::string& type);
    explicit TypesIsOperator(const FhirStructureRepository* fhirStructureRepository,
                             const ExpressionPtr typeExpression);
    Collection eval(const Collection& collection) const override;

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
    explicit TypeAsOperator(const FhirStructureRepository* fhirStructureRepository, const ExpressionPtr expression,
                            const std::string& type);
    Collection eval(const Collection& collection) const override;

protected:
    ExpressionPtr mExpression;
    std::string mType;
};

// http://hl7.org/fhirpath/N1/#astype-type-specifier
class TypeAsFunction : public TypeAsOperator
{
public:
    using TypeAsOperator::TypeAsOperator;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#in-membership
class CollectionsInOperator : public BinaryExpression
{
public:
    static constexpr auto IDENTIFIER = "in";
    using BinaryExpression::BinaryExpression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#contains-containership
class CollectionsContainsOperator : public CollectionsInOperator
{
public:
    static constexpr auto IDENTIFIER = "contains";
    CollectionsContainsOperator(const FhirStructureRepository* fhirStructureRepository, ExpressionPtr lhs,
                                ExpressionPtr rhs);
};
}
#endif//FHIR_TOOLS_SRC_FHIR_PATH_EXPRESSION_EXPRESSION_HXX