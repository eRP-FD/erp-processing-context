// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "exporter/model/trezept/ErpTPrescriptionOrganization.hxx"
#include "shared/model/TelematikId.hxx"

namespace model
{

ErpTPrescriptionOrganization::ErpTPrescriptionOrganization()
    : Resource(profileType)
{
    setResourceType("Organization");
}

void ErpTPrescriptionOrganization::setTelematikId(const rapidjson::Value& telematikIdItem)
{
    addToArray(rapidjson::Pointer{resource::ElementName::path(resource::elements::identifier)},
               copyValue(telematikIdItem));
}
void ErpTPrescriptionOrganization::setTelematikId(const TelematikId& telematikId)
{
    rapidjson::Value telematikIdItem;
    telematikIdItem.SetObject();
    setKeyValue(telematikIdItem, rapidjson::Pointer{"/system"}, resource::naming_system::telematicID);
    setKeyValue(telematikIdItem, rapidjson::Pointer{"/value"}, telematikId.id());
    addToArray(rapidjson::Pointer{resource::ElementName::path(resource::elements::identifier)},
               std::move(telematikIdItem));
}

void ErpTPrescriptionOrganization::setName(std::string_view name)
{
    const rapidjson::Pointer namePtr{resource::ElementName::path(resource::elements::name)};
    setValue(namePtr, name);
}

void ErpTPrescriptionOrganization::setTelecom(const rapidjson::Value& telecom)
{
    const rapidjson::Pointer telecomPtr{resource::ElementName::path(resource::elements::telecom)};
    setValue(telecomPtr, telecom);
}

void ErpTPrescriptionOrganization::setAddress(const rapidjson::Value& address)
{
    const rapidjson::Pointer addressPtr{resource::ElementName::path(resource::elements::address)};
    addToArray(addressPtr, copyValue(address));
}

}
