/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_CONVERSION_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_CONVERSION_HXX

#include "fhirtools/expression/Expression.hxx"

namespace fhirtools
{
// http://hl7.org/fhirpath/N1/#iifcriterion-expression-true-result-collection-otherwise-result-collection-collection
class ConversionIif : public Expression
{
public:
    static constexpr auto IDENTIFIER = "iif";
    ConversionIif(const FhirStructureRepository* fhirStructureRepository, ExpressionPtr criterion,
                  ExpressionPtr trueResult, ExpressionPtr falseResult);
    Collection eval(const Collection& collection) const override;

private:
    ExpressionPtr mCriterion;
    ExpressionPtr mTrueResult;
    ExpressionPtr mFalseResult;
};

// http://hl7.org/fhirpath/N1/#toboolean-boolean
class ConversionToBoolean : public Expression
{
};

// http://hl7.org/fhirpath/N1/#convertstoboolean-boolean
class ConversionConvertsToBoolean : public Expression
{
};

// http://hl7.org/fhirpath/N1/#tointeger-integer
class ConversionToInteger : public Expression
{
public:
    static constexpr auto IDENTIFIER = "toInteger";
    using Expression::Expression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#convertstointeger-boolean
class ConversionConvertsToInteger : public Expression
{
};

// http://hl7.org/fhirpath/N1/#todate-date
class ConversionToDate : public Expression
{
};

// http://hl7.org/fhirpath/N1/#convertstodate-boolean
class ConversionConvertsToDate : public Expression
{
};

// http://hl7.org/fhirpath/N1/#todatetime-datetime
class ConversionToDateTime : public Expression
{
};

// http://hl7.org/fhirpath/N1/#convertstodatetime-boolean
class ConversionConvertsToDateTime : public Expression
{
};

// http://hl7.org/fhirpath/N1/#todecimal-decimal
class ConversionToDecimal : public Expression
{
};

// http://hl7.org/fhirpath/N1/#convertstodecimal-boolean
class ConversionConvertsToDecimal : public Expression
{
};

// http://hl7.org/fhirpath/N1/#toquantityunit-string-quantity
class ConversionToQuantity : public Expression
{
};

// http://hl7.org/fhirpath/N1/#convertstoquantityunit-string-boolean
class ConversionConvertsToQuantity : public Expression
{
};

// http://hl7.org/fhirpath/N1/#tostring-string
class ConversionToString : public Expression
{
public:
    static constexpr auto IDENTIFIER = "toString";
    using Expression::Expression;
    Collection eval(const Collection& collection) const override;
};

// http://hl7.org/fhirpath/N1/#convertstostring-string
class ConversionConvertsToString : public Expression
{
};

// http://hl7.org/fhirpath/N1/#totime-time
class ConversionToTime : public Expression
{
};

// http://hl7.org/fhirpath/N1/#convertstotime-boolean
class ConversionConvertsToTime : public Expression
{
};

}// fhirtools

#endif//ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_CONVERSION_HXX
