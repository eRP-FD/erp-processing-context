/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/GemErpEuPrPractitionerRole.hxx"

namespace model
{

namespace
{
    using namespace model::resource;
    const rapidjson::Pointer practitionerReferencePointer(ElementName::path(elements::practitioner, elements::reference));
    const rapidjson::Pointer organizationReferencePointer(ElementName::path(elements::organization, elements::reference));
}

GemErpEuPrPractitionerRole::GemErpEuPrPractitionerRole(NumberAsStringParserDocument&& jsonTree)
    : Resource(std::move(jsonTree))
{
}

std::string_view GemErpEuPrPractitionerRole::practitionerReference() const
{
    return getStringValue(practitionerReferencePointer);
}

std::string_view GemErpEuPrPractitionerRole::organizationReference() const
{
    return getStringValue(organizationReferencePointer);
}

void GemErpEuPrPractitionerRole::setPractitionerReference(std::string_view newReference)
{
    setValue(practitionerReferencePointer, newReference);
}

void GemErpEuPrPractitionerRole::setOrganizationReference(std::string_view newReference)
{
    setValue(organizationReferencePointer, newReference);
}

}
