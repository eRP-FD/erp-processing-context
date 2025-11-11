/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

#include "fhirtools/model/Collection.hxx"
#include "fhirtools/model/DecimalType.hxx"
#include "fhirtools/model/Element.hxx"

#include <memory>

namespace fhirtools
{

class EvaluationContext {
public:
    // uses element to initialize context and collection
    EvaluationContext(std::shared_ptr<const FhirStructureRepositoryView> initView,
                      std::shared_ptr<const Element> element);

    EvaluationContext(std::shared_ptr<const FhirStructureRepositoryView> initView, Collection initCollection,
                      std::shared_ptr<const Element> initContext);

    [[nodiscard]] EvaluationContext makeElement(std::shared_ptr<fhirtools::Element> element) const;
    [[nodiscard]] EvaluationContext makeIntegerElement(int32_t value) const;
    [[nodiscard]] EvaluationContext makeIntegerElement(size_t value) const;
    [[nodiscard]] EvaluationContext makeDecimalElement(DecimalType value) const;
    [[nodiscard]] EvaluationContext makeStringElement(const std::string_view& value) const;
    [[nodiscard]] EvaluationContext makeBoolElement(bool value) const;
    [[nodiscard]] EvaluationContext makeDateElement(const Date& value) const;
    [[nodiscard]] EvaluationContext makeDateTimeElement(const DateTime& value) const;
    [[nodiscard]] EvaluationContext makeTimeElement(const Time& value) const;
    [[nodiscard]] EvaluationContext makeQuantityElement(const Element::QuantityType& value) const;

    // use element as collection in this context
    [[nodiscard]] EvaluationContext operator()(std::shared_ptr<const Element> element) const;

    // use collection in this context
    [[nodiscard]] EvaluationContext operator()(Collection collection) const;

    // empty collection in this context
    [[nodiscard]] EvaluationContext operator()() const;


    Collection collection;
    std::shared_ptr<const Element> context;
    std::shared_ptr<const FhirStructureRepositoryView> view;

private:
    template<typename ValueT>
    [[nodiscard]] EvaluationContext makeElement(Element::Type type, ValueT&& value) const;

};


}
