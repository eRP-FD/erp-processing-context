/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/MetaData.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/fhir/Fhir.hxx"
#include "erp/model/Communication.hxx"
#include "erp/model/ResourceNames.hxx"
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
              "name": "accept-date",
              "type": "date"
            },
            {
              "name": "authored-on",
              "type": "date"
            },
            {
              "name": "expiry-date",
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
              "definition": "https://gematik.de/fhir/erp/OperationDefinition/CreateOperationDefinition"
            },
            {
              "name": "activate",
              "definition": "https://gematik.de/fhir/erp/OperationDefinition/ActivateOperationDefinition"
            },
            {
              "name": "accept",
              "definition": "https://gematik.de/fhir/erp/OperationDefinition/AcceptOperationDefinition"
            },
            {
              "name": "reject",
              "definition": "https://gematik.de/fhir/erp/OperationDefinition/RejectOperationDefinition"
            },
            {
              "name": "dispense",
              "definition": "https://gematik.de/fhir/erp/OperationDefinition/DispenseOperationDefinition"
            },
            {
              "name": "close",
              "definition": "https://gematik.de/fhir/erp/OperationDefinition/CloseOperationDefinition"
            },
            {
              "name": "abort",
              "definition": "https://gematik.de/fhir/erp/OperationDefinition/AbortOperationDefinition"
            }
          ]
        },
        {
          "type": "Communication",
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
            },
            {
              "name": "identifier",
              "type": "string"
            }
          ]
        },
        {
          "type": "MedicationDispense",
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
              "name": "entity",
              "type": "string"
            },
            {
              "name": "subtype",
              "type": "token"
            }
          ]
        },
        {
          "type": "Device",
          "interaction": [
            {
              "code": "read"
            }
          ]
        },
        {
          "type": "ChargeItem",
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
        },
        {
          "type": "Consent",
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
        }
      ]
    }
  ]
}
)--";

std::once_flag onceFlag;
struct MetaDataTemplateMark;
RapidjsonNumberAsStringParserDocument<MetaDataTemplateMark> metaDataTemplate;

void initTemplates()
{
    rapidjson::StringStream strm(metadata_template.data());
    metaDataTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(strm);
    ModelExpect(! metaDataTemplate->HasParseError(), "can not parse json MetaData template string");
}

// definition of JSON pointers:
const rapidjson::Pointer datePointer("/date");
const rapidjson::Pointer versionPointer("/software/version");
const rapidjson::Pointer releaseDatePointer("/software/releaseDate");

const rapidjson::Pointer restResourceArrayPointer("/rest/0/resource");

std::string baseType(const fhirtools::FhirStructureRepository& view, ProfileType profileType)
{
    auto prof = profile(profileType);
    Expect3(prof.has_value(), "no profile for: " + std::string{magic_enum::enum_name(profileType)}, std::logic_error);
    const auto* def = view.findStructure(fhirtools::DefinitionKey{std::string{*prof}, std::nullopt});
    Expect3(def != nullptr, "structure not found in view " + std::string{view.id()} + ": " + std::string{*prof},
            std::logic_error);
    const auto* typeDef = view.findTypeById(def->typeId());
    Expect3(typeDef != nullptr, "type not found in view " + std::string{view.id()} + ": " + def->typeId(),
            std::logic_error);
    return typeDef->urlAndVersion();
}


}// anonymous namespace

std::map<std::string, std::list<ProfileType>> supportedProfileTypes{
    {"Task", {ProfileType::Gem_erxTask}},
    {"Communication", model::Communication::acceptedCommunications},
    {"MedicationDispense", {ProfileType::Gem_erxMedicationDispense}},
    {"AuditEvent", {ProfileType::Gem_erxAuditEvent}},
    {"Device", {ProfileType::Gem_erxDevice}},
    {"ChargeItem", {ProfileType::Gem_erxChargeItem}},
    {"Consent", {ProfileType::Gem_erxConsent}},
};


MetaData::MetaData(const model::Timestamp& referenceTimestamp)
    : Resource(FhirResourceBase::NoProfile,
               []() {
                   std::call_once(onceFlag, initTemplates);
                   return metaDataTemplate;
               }()
                   .instance())
{
    const auto& fhirInstance = Fhir::instance();
    static constexpr auto valueIsString = &NumberAsStringParserDocument::valueIsString;
    static constexpr auto getStringValueFromValue = &NumberAsStringParserDocument::getStringValueFromValue;
    using namespace std::string_literals;
    setVersion(ErpServerInfo::ReleaseVersion());
    model::Timestamp releaseDate = model::Timestamp::fromXsDateTime(std::string(ErpServerInfo::ReleaseDate()));
    setDate(releaseDate);
    setReleaseDate(releaseDate);
    auto viewList = fhirInstance.structureRepository(referenceTimestamp);
    Expect3(! viewList.empty(), "no view for referenceTimestamp: " + referenceTimestamp.toXsDateTime(),
            std::logic_error);
    const auto& viewConfig = viewList.latest();
    gsl::not_null view = viewConfig.view(std::addressof(fhirInstance.backend()));
    auto* restResourceArray = getValue(restResourceArrayPointer);
    Expect3(restResourceArray && restResourceArray->IsArray(), "rest resource must be array.", std::logic_error);
    for (auto& resource : restResourceArray->GetArray())
    {
        Expect3(resource.IsObject(), "rest resource must be array of Object.", std::logic_error);
        const auto typeMember = resource.FindMember("type");
        Expect3(typeMember != resource.MemberEnd() && valueIsString(typeMember->value), "field type must be string",
                std::logic_error);
        const auto& typeProfiles = supportedProfileTypes.at(std::string{getStringValueFromValue(&typeMember->value)});
        Expect3(!typeProfiles.empty(),
                "no profile types defined for: " + std::string{getStringValueFromValue(&typeMember->value)},
                std::logic_error);
        if (typeProfiles.size() == 1)
        {
            auto key = profileWithVersion(typeProfiles.front(), *view);
            Expect3(key.has_value(),
                    "no profile found for "s.append(magic_enum::enum_name(typeProfiles.front()))
                        .append("in view ")
                        .append(view->id()),
                    std::logic_error);
            setKeyValue(resource, rapidjson::Pointer{"/profile"}, to_string(*key));
        }
        else
        {

            setKeyValue(resource, rapidjson::Pointer{"/profile"}, baseType(*view, typeProfiles.front()));
            rapidjson::Value supportedProfileArray(rapidjson::kArrayType);
            for (const auto& profileType : typeProfiles)
            {
                auto key = profileWithVersion(profileType, *view);
                Expect3(key.has_value(),
                        "no profile found for "s.append(magic_enum::enum_name(typeProfiles.front()))
                            .append("in view ")
                            .append(view->id()),
                        std::logic_error);
                addStringToArray(supportedProfileArray, to_string(*key));
            }
            setKeyValue(resource, rapidjson::Pointer{"/supportedProfile"}, supportedProfileArray);
        }
    }
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


std::optional<model::Timestamp> MetaData::getValidationReferenceTimestamp() const
{
    return Timestamp::now();
}
}// namespace model
