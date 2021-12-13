/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/MetaData.hxx"

#include "erp/erp-serverinfo.hxx"
#include "erp/util/RapidjsonDocument.hxx"
#include "erp/util/Expect.hxx"

#include <mutex> // for call_once


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
              "definition": "http://gematik.de/fhir/OperationDefinition/CreateOperationDefinition"
            },
            {
              "name": "activate",
              "definition": "http://gematik.de/fhir/OperationDefinition/ActivateOperationDefinition"
            },
            {
              "name": "accept",
              "definition": "http://gematik.de/fhir/OperationDefinition/AcceptOperationDefinition"
            },
            {
              "name": "reject",
              "definition": "http://gematik.de/fhir/OperationDefinition/RejectOperationDefinition"
            },
            {
              "name": "close",
              "definition": "http://gematik.de/fhir/OperationDefinition/CloseOperationDefinition"
            },
            {
              "name": "abort",
              "definition": "http://gematik.de/fhir/OperationDefinition/AbortOperationDefinition"
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

std::once_flag onceFlag;
struct MetaDataTemplateMark;
RapidjsonNumberAsStringParserDocument<MetaDataTemplateMark> metaDataTemplate;

void initTemplates ()
{
    rapidjson::StringStream strm(metadata_template.data());
    metaDataTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(strm);
    ModelExpect(!metaDataTemplate->HasParseError(), "can not parse json template string");
}

// definition of JSON pointers:
const rapidjson::Pointer datePointer ("/date");
const rapidjson::Pointer versionPointer ("/software/version");
const rapidjson::Pointer releaseDatePointer ("/software/releaseDate");

}  // anonymous namespace


MetaData::MetaData()
    : Resource<MetaData>(ResourceBase::NoProfile,
                         []() {
                             std::call_once(onceFlag, initTemplates);
                             return metaDataTemplate;
                         }()
                             .instance())
{
    addMemberToArrayEntry(
        ::rapidjson::Pointer{"/rest/0/resource"}, 0, "profile",
        ::model::ResourceVersion::versionizeProfile("https://gematik.de/fhir/StructureDefinition/ErxTask"));

    addMemberToArrayEntry(
        ::rapidjson::Pointer{"/rest/0/resource"}, 1, "profile",
        ::model::ResourceVersion::versionizeProfile("http://hl7.org/fhir/StructureDefinition/Communication"));

    addMemberToArrayEntry(::rapidjson::Pointer{"/rest/0/resource"}, 1, "supportedProfile",
                          {::model::ResourceVersion::versionizeProfile(
                               "https://gematik.de/fhir/StructureDefinition/ErxCommunicationInfoReq"),
                           ::model::ResourceVersion::versionizeProfile(
                               "https://gematik.de/fhir/StructureDefinition/ErxCommunicationReply"),
                           ::model::ResourceVersion::versionizeProfile(
                               "https://gematik.de/fhir/StructureDefinition/ErxCommunicationDispReq"),
                           ::model::ResourceVersion::versionizeProfile(
                               "https://gematik.de/fhir/StructureDefinition/ErxCommunicationRepresentative")});

    addMemberToArrayEntry(::rapidjson::Pointer{"/rest/0/resource"}, 2, "profile",
                          ::model::ResourceVersion::versionizeProfile(
                              "https://gematik.de/fhir/StructureDefinition/ErxMedicationDispense"));
    addMemberToArrayEntry(
        ::rapidjson::Pointer{"/rest/0/resource"}, 3, "profile",
        ::model::ResourceVersion::versionizeProfile("https://gematik.de/fhir/StructureDefinition/ErxAuditEvent"));
    addMemberToArrayEntry(
        ::rapidjson::Pointer{"/rest/0/resource"}, 4, "profile",
        ::model::ResourceVersion::versionizeProfile("https://gematik.de/fhir/StructureDefinition/ErxDevice"));

    setVersion(ErpServerInfo::ReleaseVersion);
    Timestamp releaseDate = Timestamp::fromXsDateTime(ErpServerInfo::ReleaseDate);
    setDate(releaseDate);
    setReleaseDate(releaseDate);
}

MetaData::MetaData(NumberAsStringParserDocument&& jsonTree)
    : Resource<MetaData>(std::move(jsonTree))
{
    std::call_once(onceFlag, initTemplates);
}

Timestamp MetaData::date() const
{
    return Timestamp::fromXsDateTime(std::string(getStringValue(datePointer)));
}

std::string_view MetaData::version() const
{
    return getStringValue(versionPointer);
}

Timestamp MetaData::releaseDate() const
{
    return Timestamp::fromXsDateTime(std::string(getStringValue(releaseDatePointer)));
}

void MetaData::setDate(const Timestamp& date)
{
    setValue(datePointer, date.toXsDateTime());
}

void MetaData::setVersion(const std::string_view& version)
{
    setValue(versionPointer, version);
}

void MetaData::setReleaseDate(const Timestamp& releaseDate)
{
    setValue(releaseDatePointer, releaseDate.toXsDateTime());
}

}  // namespace model
