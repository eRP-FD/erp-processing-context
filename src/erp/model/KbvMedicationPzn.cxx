/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/KbvMedicationPzn.hxx"
#include "erp/model/ResourceNames.hxx"

namespace model
{
KbvMedicationPzn::KbvMedicationPzn(NumberAsStringParserDocument&& document)
    : KbvMedicationBase<KbvMedicationPzn>(std::move(document))
{
}

std::optional<std::string_view> KbvMedicationPzn::amountNumeratorValueAsString() const
{
    static const rapidjson::Pointer amountNumeratorValuePointer(resource::ElementName::path(
        resource::elements::amount, resource::elements::numerator, resource::elements::value));
    return getNumericAsString(amountNumeratorValuePointer);
}

std::optional<std::string_view> KbvMedicationPzn::amountNumeratorSystem() const
{
    static const rapidjson::Pointer amountNumeratorSystemPointer(resource::ElementName::path(
        resource::elements::amount, resource::elements::numerator, resource::elements::system));
    return getOptionalStringValue(amountNumeratorSystemPointer);
}

std::optional<std::string_view> KbvMedicationPzn::amountNumeratorCode() const
{
    static const rapidjson::Pointer amountNumeratorCodePointer(resource::ElementName::path(
        resource::elements::amount, resource::elements::numerator, resource::elements::code));
    return getOptionalStringValue(amountNumeratorCodePointer);
}

model::Pzn KbvMedicationPzn::pzn() const
{
    static const rapidjson::Pointer pznPointer(resource::ElementName::path(
        resource::elements::code, resource::elements::coding, 0, resource::elements::code));
    return model::Pzn{getStringValue(pznPointer)};
}

} // namespace model
