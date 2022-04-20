/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/KbvMedicationPzn.hxx"
#include "erp/model/ResourceNames.hxx"

namespace model
{
KbvMedicationPzn::KbvMedicationPzn(NumberAsStringParserDocument&& document)
    : KbvMedicationBase<KbvMedicationPzn, ResourceVersion::KbvItaErp>(std::move(document))
{
}

const std::optional<std::string_view> KbvMedicationPzn::amountNumeratorValueAsString() const
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
}
