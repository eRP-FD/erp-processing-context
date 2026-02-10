/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "HealthcareServiceDirectory.hxx"
#include "shared/util/Uuid.hxx"


namespace
{
using namespace model::resource;

const rapidjson::Pointer providedByReferencePtr(ElementName::path(elements::providedBy, elements::reference));
const rapidjson::Pointer locationReferencePtr(ElementName::path(elements::location, 0, elements::reference));
const rapidjson::Pointer telecomPtr(ElementName::path(elements::telecom));
}

namespace model
{

HealthcareServiceDirectory::HealthcareServiceDirectory()
    : Resource<HealthcareServiceDirectory>(profileType)
{
    setResourceType(resourceTypeName);
    setId(Uuid{}.toString());
}

std::optional<Timestamp> HealthcareServiceDirectory::getValidationReferenceTimestamp() const
{
    return model::Timestamp::now();
}

HealthcareServiceDirectory::HealthcareServiceDirectory(NumberAsStringParserDocument&& jsonTree)
    : Resource<HealthcareServiceDirectory>(std::move(jsonTree))
{
}

[[nodiscard]] std::string_view HealthcareServiceDirectory::getOrganizationReference() const
{
    return getStringValue(providedByReferencePtr);
}

[[nodiscard]] std::optional<std::string_view> HealthcareServiceDirectory::getLocationReference() const
{
    return getOptionalStringValue(locationReferencePtr);
}

const rapidjson::Value* HealthcareServiceDirectory::telecomRaw() const
{
    return getValue(telecomPtr);
}

}
