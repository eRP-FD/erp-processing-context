/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/KbvOrganization.hxx"
#include "shared/model/ResourceNames.hxx"

namespace model
{
KbvOrganization::KbvOrganization(NumberAsStringParserDocument&& document)
    : Resource<KbvOrganization>(std::move(document))
{
}


bool KbvOrganization::identifierExists() const
{
    static const rapidjson::Pointer identifierPointer(resource::ElementName::path(resource::elements::identifier));
    const auto& identifierArray = getValue(identifierPointer);
    return identifierArray != nullptr;
}

std::optional<std::string_view> KbvOrganization::telecom(const std::string_view& system) const
{
    static const rapidjson::Pointer telecomPointer(resource::ElementName::path(resource::elements::telecom));
    static const rapidjson::Pointer systemPointer(resource::ElementName::path(resource::elements::system));
    static const rapidjson::Pointer valuePointer(resource::ElementName::path(resource::elements::value));
    return findStringInArray(telecomPointer, systemPointer, system, valuePointer);
}

std::optional<std::string_view> KbvOrganization::addressLine(size_t idx) const
{
    const rapidjson::Pointer addressLinePointer(
        resource::ElementName::path(resource::elements::address, "0", resource::elements::line, idx));
    return getOptionalStringValue(addressLinePointer);
}

std::optional<model::UnspecifiedResource> KbvOrganization::address_line(size_t index) const
{
    const rapidjson::Pointer addressLinePointer(resource::ElementName::path(
        resource::elements::address, "0", resource::elements::_line, std::to_string(index)));
    return extractUnspecifiedResource(addressLinePointer);
}

std::optional<std::string_view> KbvOrganization::addressType() const
{
    static const rapidjson::Pointer addressTypePointer(
        resource::ElementName::path(resource::elements::address, "0", resource::elements::type));
    return getOptionalStringValue(addressTypePointer);
}

std::optional<model::UnspecifiedResource> KbvOrganization::address() const
{
    static const rapidjson::Pointer addressPointer(resource::ElementName::path(resource::elements::address, "0"));
    return extractUnspecifiedResource(addressPointer);
}

}
