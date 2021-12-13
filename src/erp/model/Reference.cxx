/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Reference.hxx"
#include "erp/model/ResourceNames.hxx"

namespace model
{

Reference::Reference(NumberAsStringParserDocument&& document)
    : Resource<Reference>(std::move(document))
{
}

std::optional<std::string_view> Reference::identifierUse() const
{
    static const rapidjson::Pointer identifierUsePointer(
        resource::ElementName::path(resource::elements::identifier, resource::elements::use));
    return getOptionalStringValue(identifierUsePointer);
}

std::optional<std::string_view> Reference::identifierTypeCodingCode() const
{
    static const rapidjson::Pointer identifierTypeCodingCodePointer(
        resource::ElementName::path(resource::elements::identifier, resource::elements::type,
                                    resource::elements::coding, "0", resource::elements::code));
    return getOptionalStringValue(identifierTypeCodingCodePointer);
}

std::optional<std::string_view> Reference::identifierTypeCodingSystem() const
{
    static const rapidjson::Pointer identifierTypeCodingSystemPointer(
        resource::ElementName::path(resource::elements::identifier, resource::elements::type,
                                    resource::elements::coding, "0", resource::elements::system));
    return getOptionalStringValue(identifierTypeCodingSystemPointer);
}

std::optional<std::string_view> Reference::identifierSystem() const
{
    static const rapidjson::Pointer identifierSystemPointer(
        resource::ElementName::path(resource::elements::identifier, resource::elements::system));
    return getOptionalStringValue(identifierSystemPointer);
}

std::optional<std::string_view> Reference::identifierValue() const
{
    static const rapidjson::Pointer identifierValuePointer(
        resource::ElementName::path(resource::elements::identifier, resource::elements::value));
    return getOptionalStringValue(identifierValuePointer);
}

}
