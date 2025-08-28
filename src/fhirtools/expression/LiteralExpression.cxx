/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/expression/LiteralExpression.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/ExpressionTrace.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"

namespace fhirtools
{
LiteralExpression::LiteralExpression(
    std::shared_ptr<const fhirtools::FhirStructureRepositoryView> fhirStructureRepositoryView,
    std::shared_ptr<PrimitiveElement> value)
    : Expression(std::move(fhirStructureRepositoryView))
    , mValue(std::move(value))
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
    const std::shared_ptr<const fhirtools::FhirStructureRepositoryView>& fhirStructureRepositoryView, bool value)
    : LiteralExpression(fhirStructureRepositoryView,
                        std::make_shared<PrimitiveElement>(fhirStructureRepositoryView, Element::Type::Boolean, value))
{
}

LiteralStringExpression::LiteralStringExpression(
    const std::shared_ptr<const fhirtools::FhirStructureRepositoryView>& fhirStructureRepositoryView,
    const std::string_view& value)
    : LiteralExpression(
          fhirStructureRepositoryView,
          std::make_shared<PrimitiveElement>(fhirStructureRepositoryView, Element::Type::String, std::string(value)))
{
}

LiteralIntegerExpression::LiteralIntegerExpression(
    const std::shared_ptr<const fhirtools::FhirStructureRepositoryView>& fhirStructureRepositoryView, int32_t value)
    : LiteralExpression(fhirStructureRepositoryView,
                        std::make_shared<PrimitiveElement>(fhirStructureRepositoryView, Element::Type::Integer, value))
{
}

LiteralDecimalExpression::LiteralDecimalExpression(
    const std::shared_ptr<const fhirtools::FhirStructureRepositoryView>& fhirStructureRepositoryView, DecimalType value)
    : LiteralExpression(fhirStructureRepositoryView,
                        std::make_shared<PrimitiveElement>(fhirStructureRepositoryView, Element::Type::Decimal, value))
{
}

LiteralDateExpression::LiteralDateExpression(
    const std::shared_ptr<const fhirtools::FhirStructureRepositoryView>& fhirStructureRepositoryView,
    const std::string& value)
    : LiteralExpression(fhirStructureRepositoryView, std::make_shared<PrimitiveElement>(
                                                         fhirStructureRepositoryView, Element::Type::Date, Date(value)))
{
}

LiteralTimeExpression::LiteralTimeExpression(
    const std::shared_ptr<const fhirtools::FhirStructureRepositoryView>& fhirStructureRepositoryView,
    const std::string& value)
    : LiteralExpression(fhirStructureRepositoryView, std::make_shared<PrimitiveElement>(
                                                         fhirStructureRepositoryView, Element::Type::Time, Time(value)))
{
}

LiteralDateTimeExpression::LiteralDateTimeExpression(
    const std::shared_ptr<const fhirtools::FhirStructureRepositoryView>& fhirStructureRepositoryView,
    const std::string& value)
    : LiteralExpression(
          fhirStructureRepositoryView,
          std::make_shared<PrimitiveElement>(fhirStructureRepositoryView, Element::Type::DateTime, DateTime(value)))
{
}

LiteralQuantityExpression::LiteralQuantityExpression(
    const std::shared_ptr<const fhirtools::FhirStructureRepositoryView>& fhirStructureRepositoryView,
    const std::string& value)
    : LiteralExpression(fhirStructureRepositoryView,
                        std::make_shared<PrimitiveElement>(fhirStructureRepositoryView, Element::Type::Quantity,
                                                           Element::QuantityType(value)))
{
}
}
