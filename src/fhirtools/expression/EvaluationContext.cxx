/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/expression/EvaluationContext.hxx"
#include "fhirtools/model/Element.hxx"
#include "fhirtools/FPExpect.hxx"

#include <memory>

namespace fhirtools
{

EvaluationContext::EvaluationContext(std::shared_ptr<const FhirStructureRepositoryView> initView,
                                     Collection initCollection, std::shared_ptr<const Element> initContext)
    : collection{std::move(initCollection)}
    , context{std::move(initContext)}
    , view{std::move(initView)}
{
}

EvaluationContext::EvaluationContext(std::shared_ptr<const FhirStructureRepositoryView> initView,
                                     std::shared_ptr<const Element> element)
    : collection{element}
    , context{std::move(element)}
    , view{std::move(initView)}
{
}

EvaluationContext EvaluationContext::makeElement(std::shared_ptr<Element> element) const
{
    return EvaluationContext{view, {std::move(element)}, context};
}

template<typename ValueT>
EvaluationContext EvaluationContext::makeElement(Element::Type type, ValueT&& value) const
{
    return makeElement(std::make_shared<PrimitiveElement>(std::addressof(context->getFhirStructureRepository()), type,
                                                          std::forward<ValueT>(value)));
}

EvaluationContext EvaluationContext::makeBoolElement(bool value) const
{
    return makeElement(Element::Type::Boolean, value);
}

EvaluationContext EvaluationContext::makeIntegerElement(int32_t value) const
{
    return makeElement(Element::Type::Integer, value);
}

EvaluationContext EvaluationContext::makeIntegerElement(size_t value) const
{
    FPExpect(value < std::numeric_limits<int32_t>::max(), "value too large for int32");
    return makeElement(Element::Type::Integer, gsl::narrow<int32_t>(value));
}

EvaluationContext EvaluationContext::makeDecimalElement(DecimalType value) const
{
    return makeElement(Element::Type::Decimal, value);
}

EvaluationContext EvaluationContext::makeStringElement(const std::string_view& value) const
{
    return makeElement(Element::Type::String, std::string{value});
}

EvaluationContext EvaluationContext::makeDateElement(const Date& value) const
{
    return makeElement(Element::Type::Date, value);
}

EvaluationContext EvaluationContext::makeDateTimeElement(const DateTime& value) const
{
    return makeElement(Element::Type::DateTime, value);
}

EvaluationContext EvaluationContext::makeTimeElement(const Time& value) const
{
    return makeElement(Element::Type::Time, value);
}

EvaluationContext EvaluationContext::makeQuantityElement(const Element::QuantityType& value) const
{
    return makeElement(Element::Type::Quantity, value);
}

EvaluationContext EvaluationContext::operator()() const
{
    return {view, {}, context};
}

EvaluationContext EvaluationContext::operator()(Collection collection) const
{
    return {view, std::move(collection), context};
}

EvaluationContext EvaluationContext::operator()(std::shared_ptr<const Element> element) const
{
    FPExpect(element != nullptr, "element must not be nullptr.");
    return {view, {std::move(element)}, context};
}

}
