/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Device.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/RapidjsonDocument.hxx"
#include "erp/erp-serverinfo.hxx"

#include "magic_enum.hpp"

#include <mutex> // for call_once

namespace rj = rapidjson;

namespace model
{
using namespace resource;

namespace
{
constexpr std::string_view contact_template = R"(
{
  {
      "system":"COMMUNICATION_SYSTEM",
      "value":"CONTACT"
  }
})";

const std::string device_template = R"(
{
  "resourceType":"Device",
  "id":"DEVICE_ID",
  "meta":{
    "profile":[
      ""
    ]
  },
  "status":"DEVICE_STATUS",
  "deviceName":[
    {
      "name":"DEVICE_NAME",
      "type":"user-friendly-name"
    }
  ],
  "version":[
    {
      "value":"ERP_RELEASE_VERSION"
    }
  ],
  "serialNumber":"ERP_RELEASE_VERSION",
  "contact":[
    {
      "system":"email",
      "value":"EMAIL"
    }
  ]
})";

std::once_flag onceFlag;
struct ContactTemplateMark;
RapidjsonNumberAsStringParserDocument<ContactTemplateMark> contactTemplate;
struct DeviceTemplateMark;
RapidjsonNumberAsStringParserDocument<DeviceTemplateMark> deviceTemplate;

void initTemplates ()
{
    rj::StringStream s1(device_template.data());
    deviceTemplate->ParseStream<rj::kParseNumbersAsStringsFlag, rj::CustomUtf8>(s1);

    rapidjson::StringStream s2(contact_template.data());
    contactTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rj::CustomUtf8>(s2);
}

// definition of JSON pointers:
const rj::Pointer idPointer(ElementName::path(elements::id));
const rj::Pointer statusPointer(ElementName::path(elements::status));
const rj::Pointer versionValuePointer(ElementName::path(elements::version, 0, elements::value));
const rj::Pointer serialNumberPointer (ElementName::path(elements::serialNumber));
const rj::Pointer deviceNamePointer(ElementName::path(elements::deviceName, 0, elements::name));
const rj::Pointer contactPointer(ElementName::path(elements::contact));
const rj::Pointer systemPointer(ElementName::path(elements::system));
const rj::Pointer valuePointer(ElementName::path(elements::value));

}  // anonymous namespace


Device::Device()
    : Resource<Device>("https://gematik.de/fhir/StructureDefinition/ErxDevice",
                       []() {
                           std::call_once(onceFlag, initTemplates);
                           return deviceTemplate;
                       }()
                           .instance())
{
    setId(std::to_string(Id));
    setStatus(Status::active);
    setVersion(ErpServerInfo::ReleaseVersion);
    setSerialNumber(ErpServerInfo::ReleaseVersion);
    setName(Name);
    setContact(CommunicationSystem::email, Email);
}


Device::Device (NumberAsStringParserDocument&& jsonTree)
    : Resource<Device>(std::move(jsonTree))
{
    std::call_once(onceFlag, initTemplates);
}


std::string_view Device::id() const
{
    return getStringValue(idPointer);
}

Device::Status Device::status() const
{
    std::optional<Status> status = magic_enum::enum_cast<Status>(getStringValue(statusPointer));
    ModelExpect(status.has_value(), "Status not set in device resource");
    return status.value();
}

std::string_view Device::version() const
{
    return getStringValue(versionValuePointer);
}

std::string_view Device::serialNumber() const
{
    return getStringValue(serialNumberPointer);
}

std::string_view Device::name() const
{
    return getStringValue(deviceNamePointer);
}

std::optional<std::string_view> Device::contact(CommunicationSystem system) const
{
    return findStringInArray(contactPointer, systemPointer, magic_enum::enum_name(system), valuePointer);
}

void Device::setId(const std::string_view& id)
{
    setValue(idPointer, id);
}

void Device::setStatus(Status status)
{
    setValue(statusPointer, magic_enum::enum_name(status));
}

void Device::setVersion(const std::string_view& version)
{
    setValue(versionValuePointer, version);
}

void Device::setSerialNumber(const std::string_view& serialNumber)
{
    setValue(serialNumberPointer, serialNumber);
}

void Device::setName(const std::string_view& name)
{
    setValue(deviceNamePointer, name);
}

void Device::setContact(CommunicationSystem system, const std::string_view& contact)
{
    std::optional<std::tuple<const rapidjson::Value*, std::size_t>> memberAndPosition =
        findMemberInArray(contactPointer, systemPointer, magic_enum::enum_name(system), valuePointer);
    std::size_t contactIndex = 0;
    if (memberAndPosition.has_value())
    {
        ModelExpect(std::get<0>(memberAndPosition.value()) != nullptr, "Invalid Device format");
        contactIndex = std::get<1>(memberAndPosition.value());
    }
    else
    {
        // Array element does not yet exist. Create a new one.
        auto newValue = copyValue(*contactTemplate);
        setKeyValue(newValue, systemPointer, magic_enum::enum_name(system));
        contactIndex = addToArray(contactPointer, std::move(newValue));
    }
    rj::Pointer contactValuePointer(ElementName::path(elements::contact, contactIndex, elements::value));
    setValue(contactValuePointer, contact);
}

std::string Device::createReferenceString(const std::string& linkBase)
{
    return linkBase + "/Device/" + std::to_string(Id);
}

} // namespace model
