/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "fhirtools/expression/LiteralExpression.hxx"

#include "fhirtools/FPExpect.hxx"
#include "fhirtools/expression/ExpressionTrace.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"

namespace fhirtools
{
LiteralExpression::LiteralExpression(const FhirStructureRepository* fhirStructureRepository,
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

LiteralBooleanExpression::LiteralBooleanExpression(const FhirStructureRepository* fhirStructureRepository, bool value)
    : LiteralExpression(fhirStructureRepository,
                        std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::Boolean, value))
{
}

LiteralStringExpression::LiteralStringExpression(const FhirStructureRepository* fhirStructureRepository,
                                                 const std::string_view& value)
    : LiteralExpression(
          fhirStructureRepository,
          std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::String, std::string(value)))
{
}

LiteralIntegerExpression::LiteralIntegerExpression(const FhirStructureRepository* fhirStructureRepository,
                                                   int32_t value)
    : LiteralExpression(fhirStructureRepository,
                        std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::Integer, value))
{
}

LiteralDecimalExpression::LiteralDecimalExpression(const FhirStructureRepository* fhirStructureRepository,
                                                   Element::DecimalType value)
    : LiteralExpression(fhirStructureRepository,
                        std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::Decimal, value))
{
}

LiteralDateExpression::LiteralDateExpression(const FhirStructureRepository* fhirStructureRepository,
                                             const std::string& value)
    : LiteralExpression(fhirStructureRepository,
                        std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::Date,
                                                           Timestamp::fromFhirDateTime(value)))
{
}

LiteralTimeExpression::LiteralTimeExpression(const FhirStructureRepository* fhirStructureRepository,
                                             const std::string& value)
    : LiteralExpression(fhirStructureRepository,
                        std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::Time,
                                                           Timestamp::fromFhirPathTimeLiteral(value)))
{
}

LiteralDateTimeExpression::LiteralDateTimeExpression(const FhirStructureRepository* fhirStructureRepository,
                                                     const std::string& value)
    : LiteralExpression(fhirStructureRepository,
                        std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::DateTime,
                                                           Timestamp::fromFhirPathDateTimeLiteral(value)))
{
}

LiteralQuantityExpression::LiteralQuantityExpression(const FhirStructureRepository* fhirStructureRepository,
                                                     const std::string& value)
    : LiteralExpression(fhirStructureRepository,
                        std::make_shared<PrimitiveElement>(fhirStructureRepository, Element::Type::Quantity,
                                                           Element::QuantityType(value)))
{
}
}
