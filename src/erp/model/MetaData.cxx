/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/MetaData.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/model/ResourceVersion.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/RapidjsonDocument.hxx"

#include <mutex>// for call_once


namespace model
{

namespace
{
using namespace std::string_literals;

const std::string metadata_template = R"--(
{
  "resourceType": "CapabilityStatement",
  "name": "Gem_erxCapabilityStatement",
  "title": "E-Rezept Workflow CapabilityStatement",
  "status": "draft",
  "date": "",
  "kind": "instance",
  "software": {
    "name": "DEIBM-ERP-FD",
    "version": "",
    "releaseDate": ""
  },
  "implementation": {
    "description": "E-Rezept Fachdienst Server"
  },
  "fhirVersion": "4.0.1",
  "format": [
    "xml",
    "json"
  ],
  "rest": [
    {
      "mode": "server",
      "resource": [
        {
          "type": "Task",
          "profile": "",
          "interaction": [
            {
              "code": "create"
            },
            {
              "code": "read"
            }
          ],
          "searchParam": [
            {
              "name": "status",
              "type": "token"
            },
            {
              "name": "authored-on",
              "type": "date"
            },
            {
              "name": "modified",
              "type": "date"
            }
          ],
          "operation": [
            {
              "name": "create",
              "definition": ""
            },
            {
              "name": "activate",
              "definition": ""
            },
            {
              "name": "accept",
              "definition": ""
            },
            {
              "name": "reject",
              "definition": ""
            },
            {
              "name": "close",
              "definition": ""
            },
            {
              "name": "abort",
              "definition": ""
            }
          ]
        },
        {
          "type": "Communication",
          "profile": "",
          "supportedProfile": [],
          "interaction": [
            {
              "code": "create"
            },
            {
              "code": "read"
            },
            {
              "code": "delete"
            }
          ],
          "searchParam": [
            {
              "name": "sent",
              "type": "date"
            },
            {
              "name": "received",
              "type": "date"
            },
            {
              "name": "sender",
              "type": "string"
            },
            {
              "name": "recipient",
              "type": "string"
            }
          ]
        },
        {
          "type": "MedicationDispense",
          "profile": "",
          "interaction": [
            {
              "code": "read"
            }
          ],
          "searchParam": [
            {
              "name": "whenhandedover",
              "type": "date"
            },
            {
              "name": "whenprepared",
              "type": "date"
            },
            {
              "name": "performer",
              "type": "string"
            }
          ]
        },
        {
          "type": "AuditEvent",
          "profile": "",
          "interaction": [
            {
              "code": "read"
            }
          ],
          "searchParam": [
            {
              "name": "date",
              "type": "date"
            },
            {
              "name": "subtype",
              "type": "token"
            }
          ]
        },
        {
          "type": "Device",
          "profile": "",
          "interaction": [
            {
              "code": "read"
            }
          ]
        }
      ]
    }
  ]
}
)--";
constexpr std::string_view chargeitem_resource_template = R"--(
        {
          "type": "ChargeItem",
          "profile": "",
          "interaction": [
            {
              "code": "create"
            },
            {
              "code": "read"
            },
            {
              "code": "delete"
            }
          ],
          "searchParam": [
            {
              "name": "entered-date",
              "type": "date"
            },
            {
              "name": "_lastUpdated",
              "type": "date"
            }
          ]
        })--";
constexpr std::string_view consent_resource_template = R"--(
        {
          "type": "Consent",
          "profile": "",
          "interaction": [
            {
              "code": "create"
            },
            {
              "code": "read"
            },
            {
              "code": "delete"
            }
          ]
        })--";

std::once_flag onceFlag;
struct MetaDataTemplateMark;
RapidjsonNumberAsStringParserDocument<MetaDataTemplateMark> metaDataTemplate;
struct ConsentTemplateMark;
RapidjsonNumberAsStringParserDocument<ConsentTemplateMark> consentTemplate;
struct ChargeItemTemplateMark;
RapidjsonNumberAsStringParserDocument<ChargeItemTemplateMark> chargeItemTemplate;

void initTemplates()
{
    rapidjson::StringStream strm(metadata_template.data());
    metaDataTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(strm);
    ModelExpect(! metaDataTemplate->HasParseError(), "can not parse json MetaData template string");
    rapidjson::StringStream strmChargeItem(chargeitem_resource_template.data());
    chargeItemTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(strmChargeItem);
    ModelExpect(! metaDataTemplate->HasParseError(), "can not parse json ChargeItem template string");
    rapidjson::StringStream strmConsent(consent_resource_template.data());
    consentTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(strmConsent);
    ModelExpect(! metaDataTemplate->HasParseError(), "can not parse json Consent template string");
}

// definition of JSON pointers:
const rapidjson::Pointer datePointer("/date");
const rapidjson::Pointer versionPointer("/software/version");
const rapidjson::Pointer releaseDatePointer("/software/releaseDate");

const rapidjson::Pointer restResourceArrayPointer("/rest/0/resource");
const rapidjson::Pointer restResourceTaskOperationArrayPointer("/rest/0/resource/0/operation");
const rapidjson::Pointer restResourceCommSupportedProfileArrayPointer("/rest/0/resource/1/supportedProfile");
const rapidjson::Pointer restResourceTaskSupportedProfilePointer("/rest/0/resource/0/profile");

struct DeprecatedProfile
{
    static constexpr auto resourceStructDefs = {
        resource::structure_definition::deprecated::task,
        resource::structure_definition::communication,
        resource::structure_definition::deprecated::medicationDispense,
        resource::structure_definition::deprecated::auditEvent,
        resource::structure_definition::deprecated::device
    };
    static constexpr auto resourceCommStructDefs = {
        resource::structure_definition::deprecated::communicationInfoReq,
        resource::structure_definition::deprecated::communicationReply,
        resource::structure_definition::deprecated::communicationDispReq,
        resource::structure_definition::deprecated::communicationRepresentative
    };
    static constexpr auto taskOpStructDefs = {
        resource::operation_definition::deprecated::create,
        resource::operation_definition::deprecated::activate,
        resource::operation_definition::deprecated::accept,
        resource::operation_definition::deprecated::reject,
        resource::operation_definition::deprecated::close,
        resource::operation_definition::deprecated::abort
    };
}
deprecatedProfile;

struct CurrentProfile
{
    static constexpr auto resourceStructDefs = {
        resource::structure_definition::task,
        resource::structure_definition::communication,
        resource::structure_definition::medicationDispense,
        resource::structure_definition::auditEvent,
        resource::structure_definition::device,
        resource::structure_definition::chargeItem,
        resource::structure_definition::consent
    };
    static constexpr auto resourceCommStructDefs = {
        resource::structure_definition::communicationInfoReq,
        resource::structure_definition::communicationReply,
        resource::structure_definition::communicationDispReq,
        resource::structure_definition::communicationRepresentative,
        resource::structure_definition::communicationChargChangeReq,
        resource::structure_definition::communicationChargChangeReply
    };
    static constexpr auto taskOpStructDefs = {
        resource::operation_definition::create,
        resource::operation_definition::activate,
        resource::operation_definition::accept,
        resource::operation_definition::reject,
        resource::operation_definition::close,
        resource::operation_definition::abort
    };
}
currentProfile;

}// anonymous namespace


MetaData::MetaData(ResourceVersion::FhirProfileBundleVersion profileBundle)
    : Resource(ResourceBase::NoProfile,
                         []() {
                             std::call_once(onceFlag, initTemplates);
                             return metaDataTemplate;
                         }()
                             .instance())
{
    if(model::ResourceVersion::deprecatedBundle(profileBundle))
    {
        fillResource(deprecatedProfile, profileBundle);
    }
    else
    {
        addResourceTemplate(*chargeItemTemplate);
        addResourceTemplate(*consentTemplate);
        fillResource(currentProfile, profileBundle);
    }
    setVersion(ErpServerInfo::ReleaseVersion());
    model::Timestamp releaseDate = model::Timestamp::fromXsDateTime(std::string(ErpServerInfo::ReleaseDate()));
    setDate(releaseDate);
    setReleaseDate(releaseDate);
}

template<class ProfileDefinition>
void MetaData::fillResource(const ProfileDefinition& profileDefinition,
                            ResourceVersion::FhirProfileBundleVersion profileBundle)
{
    for (size_t index = 0; const auto structDef : profileDefinition.resourceStructDefs)
    {
        addMemberToArrayEntry(restResourceArrayPointer, index++, "profile",
                              ResourceVersion::versionizeProfile(structDef, {profileBundle}));
    }
    for (const auto structDef : profileDefinition.resourceCommStructDefs)
    {
        addToArray(restResourceCommSupportedProfileArrayPointer,
                   ResourceVersion::versionizeProfile(structDef, {profileBundle}));
    }
    for (size_t index = 0; const auto structDef : profileDefinition.taskOpStructDefs)
    {
        addMemberToArrayEntry(restResourceTaskOperationArrayPointer, index++, "definition", structDef);
    }
}

template<class TemplateDocument>
void MetaData::addResourceTemplate(const TemplateDocument& templateDocument)
{
    auto newEntry = copyValue(templateDocument);
    addToArray(restResourceArrayPointer, std::move(newEntry));
}

MetaData::MetaData(NumberAsStringParserDocument&& jsonTree)
    : Resource(std::move(jsonTree))
{
    std::call_once(onceFlag, initTemplates);
}

model::Timestamp MetaData::date() const
{
    return model::Timestamp::fromXsDateTime(std::string(getStringValue(datePointer)));
}

std::string_view MetaData::version() const
{
    return getStringValue(versionPointer);
}

model::Timestamp MetaData::releaseDate() const
{
    return model::Timestamp::fromXsDateTime(std::string(getStringValue(releaseDatePointer)));
}

void MetaData::setDate(const model::Timestamp& date)
{
    setValue(datePointer, date.toXsDateTime());
}

void MetaData::setVersion(const std::string_view& version)
{
    setValue(versionPointer, version);
}

void MetaData::setReleaseDate(const model::Timestamp& releaseDate)
{
    setValue(releaseDatePointer, releaseDate.toXsDateTime());
}

ResourceVersion::DeGematikErezeptWorkflowR4 MetaData::taskProfileVersion()
{
    using namespace std::string_literals;
    auto profileName = getStringValue(restResourceTaskSupportedProfilePointer);
    const auto* profInfo = model::ResourceVersion::profileInfoFromProfileName(profileName);
    ModelExpect(profInfo != nullptr, "Unsupported profile: "s.append(profileName));
    return std::get<ResourceVersion::DeGematikErezeptWorkflowR4>(profInfo->version);
}

}// namespace model
