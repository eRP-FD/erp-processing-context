/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "LocationDirectory.hxx"
#include "shared/util/Uuid.hxx"


namespace
{
using namespace model::resource;

const rapidjson::Pointer urlPtr(ElementName::path(elements::url));
const rapidjson::Pointer valueCodePtr(ElementName::path(elements::valueCode));
const rapidjson::Pointer addressTextPtr{ElementName::path(elements::address, elements::text)};
const rapidjson::Pointer addressPtr{ElementName::path(elements::address)};
}

namespace model
{

LocationDirectory::LocationDirectory()
    : Resource<LocationDirectory>(profileType)
{
    setResourceType(resourceTypeName);
    setId(Uuid{}.toString());
}

std::optional<Timestamp> LocationDirectory::getValidationReferenceTimestamp() const
{
    return model::Timestamp::now();
}

LocationDirectory::LocationDirectory(NumberAsStringParserDocument&& jsonTree)
    : Resource<LocationDirectory>(std::move(jsonTree))
{
}

[[nodiscard]] std::optional<std::string_view> LocationDirectory::getAddress() const
{
    return getOptionalStringValue(addressTextPtr);
}

const rapidjson::Value* LocationDirectory::addressRaw() const
{
    return getValue(addressPtr);
}

}
