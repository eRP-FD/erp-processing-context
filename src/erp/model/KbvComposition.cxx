/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/KbvComposition.hxx"
#include "erp/model/ResourceNames.hxx"

namespace model
{
KbvComposition::KbvComposition(NumberAsStringParserDocument&& document)
    : Resource<KbvComposition, ResourceVersion::KbvItaErp>(std::move(document))
{
}


std::optional<std::string_view> KbvComposition::sectionEntry(const std::string_view& sectionCode) const
{
    static const rapidjson::Pointer sectionArrayPointer(resource::ElementName::path("section"));
    static const rapidjson::Pointer codeCodingCodePointer(resource::ElementName::path(
        resource::elements::code, resource::elements::coding, "0", resource::elements::code));
    static const rapidjson::Pointer entryReferencePointer(
        resource::ElementName::path(resource::elements::entry, "0", resource::elements::reference));
    return findStringInArray(sectionArrayPointer, codeCodingCodePointer, sectionCode, entryReferencePointer);
}

std::optional<std::string_view> KbvComposition::subject() const
{
    static const rapidjson::Pointer subjectReferencePointer(
        resource::ElementName::path(resource::elements::subject, resource::elements::reference));
    return getOptionalStringValue(subjectReferencePointer);
}

std::optional<std::string_view> KbvComposition::authorType(size_t idx) const
{
    const rapidjson::Pointer authorTypePointer(
        resource::ElementName::path(resource::elements::author, std::to_string(idx), resource::elements::type));
    return getOptionalStringValue(authorTypePointer);
}

std::optional<std::string_view> KbvComposition::authorReference(size_t idx) const
{
    const rapidjson::Pointer authorReferencePointer(
        resource::ElementName::path(resource::elements::author, std::to_string(idx), resource::elements::reference));
    return getOptionalStringValue(authorReferencePointer);
}

std::optional<std::string_view> KbvComposition::authorIdentifierSystem(size_t idx) const
{
    const rapidjson::Pointer authorIdentifierSystemPointer(resource::ElementName::path(
        resource::elements::author, std::to_string(idx), resource::elements::identifier, resource::elements::system));
    return getOptionalStringValue(authorIdentifierSystemPointer);
}

std::optional<std::string_view> KbvComposition::authorIdentifierValue(size_t idx) const
{
    const rapidjson::Pointer authorIdentifierValuePointer(resource::ElementName::path(
        resource::elements::author, std::to_string(idx), resource::elements::identifier, resource::elements::value));
    return getOptionalStringValue(authorIdentifierValuePointer);
}
}
