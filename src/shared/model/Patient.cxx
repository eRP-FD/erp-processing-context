/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/Kvnr.hxx"
#include "shared/model/Reference.hxx"
#include "shared/model/Patient.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"

#include <rapidjson/pointer.h>
#include <utility>

namespace model
{

namespace
{
rapidjson::Pointer identifierPointer("/identifier");
rapidjson::Pointer typePointer("/system");
rapidjson::Pointer kvnrPointer("/value");
rapidjson::Pointer postalCodePointer("/address/0/postalCode");
}

Patient::Patient(NumberAsStringParserDocument&& document)
    : Resource<Patient>(std::move(document))
{
}

Kvnr Patient::kvnr() const
{
    const auto* pointerValue = getValue(identifierPointer);
    ModelExpect(pointerValue && pointerValue->IsArray(), "identifier not present or not an array.");
    const auto array = pointerValue->GetArray();
    ModelExpect(array.Size() < 2, "identifier array has cardinality 0..1");
    for (auto item = array.Begin(), end = array.End(); item != end; ++item)
    {
        const auto* typePointerValue = typePointer.Get(*item);
        ModelExpect(typePointerValue && typePointerValue->IsString(),
                    "missing identifier/system in identifier array entry");
        const auto kvnrTypeString = NumberAsStringParserDocument::getStringValueFromValue(typePointerValue);
        // for gkv we have the old and the new naming system, so compare with the pkv value only
        const auto kvnrType = kvnrTypeString == resource::naming_system::pkvKvid10 ? model::Kvnr::Type::pkv : model::Kvnr::Type::gkv;
        const auto* kvnrPointerValue = kvnrPointer.Get(*item);
        ModelExpect(kvnrPointerValue && kvnrPointerValue->IsString(),
                    "missing identifier/value in identifier array entry");
        return Kvnr{NumberAsStringParserDocument::getStringValueFromValue(kvnrPointerValue), kvnrType};
    }
    ModelFail("KVNR not found in identifier array.");
}

std::optional<std::string_view> Patient::postalCode() const
{
    return getOptionalStringValue(postalCodePointer);
}

std::optional<UnspecifiedResource> Patient::birthDate() const
{
    static const rapidjson::Pointer birthDatePointer(resource::ElementName::path("birthDate"));
    return extractUnspecifiedResource(birthDatePointer);
}

std::optional<UnspecifiedResource> Patient::name_family() const
{
    static const rapidjson::Pointer nameFamilyPointer(
        resource::ElementName::path(resource::elements::name, "0", resource::elements::_family));
    return extractUnspecifiedResource(nameFamilyPointer);
}

std::optional<UnspecifiedResource> Patient::name_prefix() const
{
    static const rapidjson::Pointer namePrefixPointer(
        resource::ElementName::path(resource::elements::name, "0", resource::elements::_prefix, "0"));
    return extractUnspecifiedResource(namePrefixPointer);
}

std::optional<std::string_view> Patient::nameFamily() const
{
    static const rapidjson::Pointer nameFamilyPointer(
        resource::ElementName::path(resource::elements::name, "0", resource::elements::family));
    return getOptionalStringValue(nameFamilyPointer);
}

std::optional<std::string_view> Patient::namePrefix() const
{
    static const rapidjson::Pointer namePrefixPointer(
        resource::ElementName::path(resource::elements::name, "0", resource::elements::prefix, "0"));
    return getOptionalStringValue(namePrefixPointer);
}

std::optional<std::string_view> Patient::addressType() const
{
    static const rapidjson::Pointer addressTypePointer(
        resource::ElementName::path(resource::elements::address, "0", resource::elements::type));
    return getOptionalStringValue(addressTypePointer);
}

std::optional<std::string_view> Patient::addressLine(size_t idx) const
{
    const rapidjson::Pointer addressLinePointer(
        resource::ElementName::path(resource::elements::address, "0", resource::elements::line, std::to_string(idx)));
    return getOptionalStringValue(addressLinePointer);
}

std::optional<UnspecifiedResource> Patient::address_line(size_t idx) const
{
    const rapidjson::Pointer addressLinePointer(
        resource::ElementName::path(resource::elements::address, "0", resource::elements::_line, std::to_string(idx)));
    return extractUnspecifiedResource(addressLinePointer);
}

model::UnspecifiedResource Patient::address() const
{
    static const rapidjson::Pointer addressPointer(resource::ElementName::path(resource::elements::address, "0"));
    const auto* entry = getValue(addressPointer);
    ModelExpect(entry, "missing mandatory patient.address");
    return UnspecifiedResource::fromJson(*entry);
}

std::optional<std::string_view> Patient::identifierTypeCodingCode() const
{
    static const rapidjson::Pointer identifierTypeCodingCodePointer(
        resource::ElementName::path(resource::elements::identifier, "0", resource::elements::type,
                                    resource::elements::coding, "0", resource::elements::code));
    return getOptionalStringValue(identifierTypeCodingCodePointer);
}

size_t Patient::identifierSize() const
{
    static const rapidjson::Pointer identifiersPointer(resource::ElementName::path(resource::elements::identifier));
    const auto* identifiers = getValue(identifiersPointer);
    if (identifiers == nullptr)
    {
        return 0;
    }
    ModelExpect(identifiers->IsArray(), "Patient.identifier must be array.");
    return identifiers->Size();
}

std::optional<std::string_view> model::Patient::identifierSystem(size_t idx) const
{
    const rapidjson::Pointer identifierSystemPointer(
        resource::ElementName::path(resource::elements::identifier, idx, resource::elements::system));
    return getOptionalStringValue(identifierSystemPointer);
}

}
