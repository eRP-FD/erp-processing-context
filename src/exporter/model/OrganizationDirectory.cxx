/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/model/OrganizationDirectory.hxx"
#include "shared/model/Coding.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/model/TelematikId.hxx"
#include "shared/util/Uuid.hxx"

namespace model
{

namespace
{
const rapidjson::Pointer identifierSystemPtr{
    resource::ElementName::path(resource::elements::identifier, 0, resource::elements::system)};
const rapidjson::Pointer identifierValuePtr{
    resource::ElementName::path(resource::elements::identifier, 0, resource::elements::value)};
const rapidjson::Pointer typeCodingCodePtr{
    resource::ElementName::path(resource::elements::type, 0, resource::elements::coding, 0, resource::elements::code)};
const rapidjson::Pointer typeCodingSystemPtr{resource::ElementName::path(
    resource::elements::type, 0, resource::elements::coding, 0, resource::elements::system)};
const rapidjson::Pointer namePtr{resource::ElementName::path(resource::elements::name)};
const rapidjson::Pointer metaTagSystemPtr{
    resource::ElementName::path(resource::elements::meta, resource::elements::tag, 0, resource::elements::system)};
const rapidjson::Pointer metaTagCodePtr{
    resource::ElementName::path(resource::elements::meta, resource::elements::tag, 0, resource::elements::code)};
const rapidjson::Pointer type0Coding0Code{
    resource::ElementName::path(resource::elements::type, 0, resource::elements::coding, 0, resource::elements::code)};
const rapidjson::Pointer type0Coding0System{resource::ElementName::path(
    resource::elements::type, 0, resource::elements::coding, 0, resource::elements::system)};
}

std::optional<Timestamp> OrganizationDirectory::getValidationReferenceTimestamp() const
{
    return model::Timestamp::now();
}

OrganizationDirectory::OrganizationDirectory(const TelematikId& telematikId, const Coding& type,
                                             const std::string& name, const std::string& professionOid)
    : Resource<OrganizationDirectory>(profileType)
{
    setResourceType(resourceTypeName);
    setId(Uuid{}.toString());
    setValue(identifierSystemPtr, resource::naming_system::telematicID);
    setValue(identifierValuePtr, telematikId.id());
    if (type.getSystem())
    {
        setValue(typeCodingSystemPtr, *type.getSystem());
    }
    if (type.getCode())
    {
        setValue(typeCodingCodePtr, *type.getCode());
    }
    setValue(namePtr, name);
    setValue(metaTagSystemPtr, resource::code_system::origin);
    setValue(metaTagCodePtr, "ldap");
    setValue(type0Coding0System, "https://gematik.de/fhir/directory/CodeSystem/OrganizationProfessionOID");
    setValue(type0Coding0Code, professionOid);
}

OrganizationDirectory::OrganizationDirectory(NumberAsStringParserDocument&& jsonTree)
    : Resource<OrganizationDirectory>(std::move(jsonTree))
{
}

}
