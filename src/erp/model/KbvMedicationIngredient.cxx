/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/KbvMedicationIngredient.hxx"
#include "erp/model/ResourceNames.hxx"

namespace model
{
KbvMedicationIngredient::KbvMedicationIngredient(NumberAsStringParserDocument&& document)
    : KbvMedicationBase<KbvMedicationIngredient, ResourceVersion::KbvItaErp>(std::move(document))
{
}

std::optional<std::string_view> KbvMedicationIngredient::amountNumeratorValueAsString() const
{
    static const rapidjson::Pointer amountNumeratorPointer(resource::ElementName::path(
        resource::elements::amount, resource::elements::numerator, resource::elements::value));
    return getNumericAsString(amountNumeratorPointer);
}

std::optional<std::string_view> KbvMedicationIngredient::amountNumeratorSystem() const
{
    static const rapidjson::Pointer amountNumeratorSystemPointer(resource::ElementName::path(
        resource::elements::amount, resource::elements::numerator, resource::elements::system));
    return getOptionalStringValue(amountNumeratorSystemPointer);
}

std::optional<std::string_view> KbvMedicationIngredient::amountNumeratorCode() const
{
    static const rapidjson::Pointer amountNumeratorCodePointer(resource::ElementName::path(
        resource::elements::amount, resource::elements::numerator, resource::elements::code));
    return getOptionalStringValue(amountNumeratorCodePointer);
}

std::optional<std::string_view> KbvMedicationIngredient::ingredientStrengthNumeratorValueAsString() const
{
    static const rapidjson::Pointer ingredientStrengthNumeratorValuePointer(
        resource::ElementName::path(resource::elements::ingredient, 0, resource::elements::strength,
                                    resource::elements::numerator, resource::elements::value));
    return getNumericAsString(ingredientStrengthNumeratorValuePointer);
}
}
