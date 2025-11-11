/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/expression/LiteralExpression.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/ExpressionTrace.hxx"

namespace fhirtools
{
LiteralExpression::LiteralExpression(std::shared_ptr<PrimitiveElement> value)
    : mValue(std::move(value))
{
}

EvaluationContext LiteralNullExpression::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    return context();
}

EvaluationContext LiteralExpression::eval(const EvaluationContext& context) const
{
    EVAL_TRACE;
    return context(mValue);
}

LiteralBooleanExpression::LiteralBooleanExpression(
    gsl::not_null<const fhirtools::FhirStructureRepositoryBackend*> fhirStructureRepository, bool value)
    : LiteralExpression(std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::Boolean, value))
{
}

LiteralStringExpression::LiteralStringExpression(
    gsl::not_null<const fhirtools::FhirStructureRepositoryBackend*> fhirStructureRepository,
    const std::string_view& value)
    : LiteralExpression(
          std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::String, std::string(value)))
{
}

LiteralIntegerExpression::LiteralIntegerExpression(
    gsl::not_null<const fhirtools::FhirStructureRepositoryBackend*> fhirStructureRepository, int32_t value)
    : LiteralExpression(std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::Integer, value))
{
}

LiteralDecimalExpression::LiteralDecimalExpression(
    gsl::not_null<const fhirtools::FhirStructureRepositoryBackend*> fhirStructureRepository, DecimalType value)
    : LiteralExpression(std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::Decimal, value))
{
}

LiteralDateExpression::LiteralDateExpression(
    gsl::not_null<const fhirtools::FhirStructureRepositoryBackend*> fhirStructureRepository, const std::string& value)
    : LiteralExpression(std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::Date, Date(value)))
{
}

LiteralTimeExpression::LiteralTimeExpression(
    gsl::not_null<const fhirtools::FhirStructureRepositoryBackend*> fhirStructureRepository, const std::string& value)
    : LiteralExpression(std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::Time, Time(value)))
{
}

LiteralDateTimeExpression::LiteralDateTimeExpression(
    gsl::not_null<const fhirtools::FhirStructureRepositoryBackend*> fhirStructureRepository, const std::string& value)
    : LiteralExpression(
          std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::DateTime, DateTime(value)))
{
}

LiteralQuantityExpression::LiteralQuantityExpression(
    gsl::not_null<const fhirtools::FhirStructureRepositoryBackend*> fhirStructureRepository, const std::string& value)
    : LiteralExpression(std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::Quantity,
                                                           Element::QuantityType(value)))
{
}
}
