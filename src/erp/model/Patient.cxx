/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Patient.hxx"
#include "erp/model/Reference.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Expect.hxx"

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
    : Resource<Patient, ResourceVersion::KbvItaErp>(std::move(document))
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

std::optional<std::string_view> Patient::identifierAssignerDisplay() const
{
    static const rapidjson::Pointer identifierAssignerDisplayPointer(resource::ElementName::path(
        resource::elements::identifier, "0", resource::elements::assigner, resource::elements::display));
    return getOptionalStringValue(identifierAssignerDisplayPointer);
}

std::optional<model::Reference> Patient::identifierAssigner() const
{
    static const rapidjson::Pointer identifierAssignerPointer(
        resource::ElementName::path(resource::elements::identifier, "0", resource::elements::assigner));
    const auto* entry = getValue(identifierAssignerPointer);
    if (entry)
    {
        return Reference::fromJson(*entry);
    }
    return std::nullopt;
}

bool Patient::hasIdentifier() const
{
    static const rapidjson::Pointer identifierPointer(resource::ElementName::path(resource::elements::identifier, "0"));
    return getValue(identifierPointer) != nullptr;
}

}
