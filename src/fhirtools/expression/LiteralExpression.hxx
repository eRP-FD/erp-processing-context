/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_LITERALEXPRESSION_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_LITERALEXPRESSION_HXX

#include "fhirtools/expression/Expression.hxx"

namespace fhirtools
{

// http://hl7.org/fhirpath/N1/#literals
class LiteralExpression : public Expression
{
public:
    LiteralExpression(std::shared_ptr<PrimitiveElement> value);
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;

protected:
    std::shared_ptr<PrimitiveElement> mValue;
};

// http://hl7.org/fhirpath/N1/#null-and-empty
class LiteralNullExpression : public Expression
{
public:
    using Expression::Expression;
    [[nodiscard]] EvaluationContext eval(const EvaluationContext& context) const override;
};

// http://hl7.org/fhirpath/N1/#boolean
class LiteralBooleanExpression : public LiteralExpression
{
public:
    explicit LiteralBooleanExpression(
        gsl::not_null<const fhirtools::FhirStructureRepositoryBackend*> fhirStructureRepository, bool value);
};

// http://hl7.org/fhirpath/N1/#string
class LiteralStringExpression : public LiteralExpression
{
public:
    explicit LiteralStringExpression(
        gsl::not_null<const fhirtools::FhirStructureRepositoryBackend*> fhirStructureRepository,
        const std::string_view& value);
};

// http://hl7.org/fhirpath/N1/#integer
class LiteralIntegerExpression : public LiteralExpression
{
public:
    explicit LiteralIntegerExpression(
        gsl::not_null<const fhirtools::FhirStructureRepositoryBackend*> fhirStructureRepository, int32_t value);
};

// http://hl7.org/fhirpath/N1/#decimal
class LiteralDecimalExpression : public LiteralExpression
{
public:
    explicit LiteralDecimalExpression(
        gsl::not_null<const fhirtools::FhirStructureRepositoryBackend*> fhirStructureRepository, DecimalType value);
};

// http://hl7.org/fhirpath/N1/#date
class LiteralDateExpression : public LiteralExpression
{
public:
    explicit LiteralDateExpression(
        gsl::not_null<const fhirtools::FhirStructureRepositoryBackend*> fhirStructureRepository,
        const std::string& value);
};

// http://hl7.org/fhirpath/N1/#time
class LiteralTimeExpression : public LiteralExpression
{
public:
    explicit LiteralTimeExpression(
        gsl::not_null<const fhirtools::FhirStructureRepositoryBackend*> fhirStructureRepository,
        const std::string& value);
};

// http://hl7.org/fhirpath/N1/#datetime
class LiteralDateTimeExpression : public LiteralExpression
{
public:
    explicit LiteralDateTimeExpression(
        gsl::not_null<const fhirtools::FhirStructureRepositoryBackend*> fhirStructureRepository,
        const std::string& value);
};

// http://hl7.org/fhirpath/N1/#quantity
class LiteralQuantityExpression : public LiteralExpression
{
public:
    explicit LiteralQuantityExpression(
        gsl::not_null<const fhirtools::FhirStructureRepositoryBackend*> fhirStructureRepository,
        const std::string& value);
};

}// fhirtools

#endif//ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_EXPRESSION_LITERALEXPRESSION_HXX
