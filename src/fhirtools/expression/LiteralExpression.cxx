/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/expression/LiteralExpression.hxx"
#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/ExpressionTrace.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"

namespace fhirtools
{
LiteralExpression::LiteralExpression(
    const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository,
    std::shared_ptr<PrimitiveElement> value)
    : Expression(fhirStructureRepository)
    , mValue(std::move(value))
{
}

Collection LiteralNullExpression::eval([[maybe_unused]] const Collection& collection) const
{
    EVAL_TRACE;
    return {};
}

Collection LiteralExpression::eval([[maybe_unused]] const Collection& collection) const
{
    EVAL_TRACE;
    return {mValue};
}

LiteralBooleanExpression::LiteralBooleanExpression(
    const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository, bool value)
    : LiteralExpression(fhirStructureRepository,
                        std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::Boolean, value))
{
}

LiteralStringExpression::LiteralStringExpression(
    const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository,
    const std::string_view& value)
    : LiteralExpression(
          fhirStructureRepository,
          std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::String, std::string(value)))
{
}

LiteralIntegerExpression::LiteralIntegerExpression(
    const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository, int32_t value)
    : LiteralExpression(fhirStructureRepository,
                        std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::Integer, value))
{
}

LiteralDecimalExpression::LiteralDecimalExpression(
    const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository, DecimalType value)
    : LiteralExpression(fhirStructureRepository,
                        std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::Decimal, value))
{
}

LiteralDateExpression::LiteralDateExpression(
    const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository, const std::string& value)
    : LiteralExpression(fhirStructureRepository,
                        std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::Date, Date(value)))
{
}

LiteralTimeExpression::LiteralTimeExpression(
    const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository, const std::string& value)
    : LiteralExpression(fhirStructureRepository,
                        std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::Time, Time(value)))
{
}

LiteralDateTimeExpression::LiteralDateTimeExpression(
    const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository, const std::string& value)
    : LiteralExpression(fhirStructureRepository, std::make_shared<PrimitiveElement>(
                                                     fhirStructureRepository, Element::Type::DateTime, DateTime(value)))
{
}

LiteralQuantityExpression::LiteralQuantityExpression(
    const std::shared_ptr<const fhirtools::FhirStructureRepository>& fhirStructureRepository, const std::string& value)
    : LiteralExpression(fhirStructureRepository,
                        std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::Quantity,
                                                           Element::QuantityType(value)))
{
}
}
