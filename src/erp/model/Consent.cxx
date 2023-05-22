/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Consent.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/RapidjsonDocument.hxx"

#include <rapidjson/pointer.h>

#include <magic_enum.hpp>


namespace model
{

using namespace resource;

namespace
{

using namespace std::string_literals;

const std::string consent_template = R"--(
{
  "resourceType": "Consent",
  "id": "",
  "meta": {
    "profile": [
      ""
    ]
  },
  "status": "active",
  "scope": {
    "coding": [
      {
        "system": "http://terminology.hl7.org/CodeSystem/consentscope",
        "code": "patient-privacy",
        "display": "Privacy Consent"
      }
    ]
  },
  "category": [
    {
      "coding": [
        {
          "system": "https://gematik.de/fhir/erpchrg/CodeSystem/GEM_ERPCHRG_CS_ConsentType",
          "code": "CHARGCONS",
          "display": "Consent for saving electronic charge item"
        }
      ]
    }
  ],
  "patient": {
    "identifier": {
      "system": "http://fhir.de/sid/pkv/kvid-10",
      "value": ""
    }
  },
  "dateTime": "",
  "policyRule": {
    "coding": [
      {
        "system": "http://terminology.hl7.org/CodeSystem/v3-ActCode",
        "code": "OPTIN"
      }
    ]
  }
}
)--";

std::once_flag onceFlag;
struct ConsentTemplateMark;
RapidjsonNumberAsStringParserDocument<ConsentTemplateMark> ConsentTemplate;

void initTemplates()
{
    rapidjson::StringStream strm(consent_template.data());
    ConsentTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(strm);
}


const rapidjson::Pointer idPointer(ElementName::path(elements::id));
const rapidjson::Pointer patientKvnrValuePointer(ElementName::path(elements::patient, elements::identifier, elements::value));
const rapidjson::Pointer patientKvnrSystemPointer(ElementName::path(elements::patient, elements::identifier, elements::system));
const rapidjson::Pointer dateTimePointer(ElementName::path(elements::dateTime));

const rapidjson::Pointer categoryPointer(ElementName::path(elements::category));
const rapidjson::Pointer codingPointer(ElementName::path(elements::coding));
const rapidjson::Pointer systemPointer(ElementName::path(elements::system));
const rapidjson::Pointer codePointer(ElementName::path(elements::code));

const auto consentIdSeparator = '-';

}  // anonymous namespace


// static
std::string Consent::createIdString(Consent::Type type, const Kvnr& kvnr)
{
    return std::string(magic_enum::enum_name(type)) + consentIdSeparator + kvnr.id();
}

// static
std::pair<Consent::Type, std::string> Consent::splitIdString(const std::string_view& idStr)
{
    const auto sepPos = idStr.find(consentIdSeparator);
    ModelExpect(sepPos != std::string::npos, "Invalid Consent id " + std::string(idStr));
    const auto type = magic_enum::enum_cast<Consent::Type>(idStr.substr(0, sepPos));
    ModelExpect(type.has_value(), "Invalid Consent type in id " + std::string(idStr));
    const auto kvnr = idStr.substr(sepPos + 1);
    return std::make_pair(*type, std::string(kvnr));
}


Consent::Consent(const Kvnr& kvnr, const model::Timestamp& dateTime)
    : Resource<Consent, ResourceVersion::DeGematikErezeptPatientenrechnungR4>(resource::structure_definition::consent,
                                                                              []() {
                                                                                  std::call_once(onceFlag,
                                                                                                 initTemplates);
                                                                                  return ConsentTemplate;
                                                                              }()
                                                                                  .instance())
{
    setPatientKvnr(kvnr);
    setDateTime(dateTime);
    fillId();
}

Consent::Consent(NumberAsStringParserDocument&& jsonTree)
    : Resource<Consent, ResourceVersion::DeGematikErezeptPatientenrechnungR4>(std::move(jsonTree))
{
}

std::optional<std::string_view> Consent::id() const
{
    return getOptionalStringValue(idPointer);
}

Kvnr Consent::patientKvnr() const
{
    return Kvnr{getStringValue(patientKvnrValuePointer), getStringValue(patientKvnrSystemPointer)};
}

bool Consent::isChargingConsent() const
{
    const auto* categoryArray = getValue(categoryPointer);
    if(!categoryArray)
        return false;
    ModelExpect(categoryArray->IsArray(), ResourceBase::pointerToString(categoryPointer) + ": element must be array");
    for(auto categoryItem = categoryArray->Begin(), catEnd = categoryArray->End(); categoryItem != catEnd; ++categoryItem)
    {
        const auto* codingArray = getValue(*categoryItem, codingPointer);
        if(!codingArray)
            return false;
        ModelExpect(codingArray->IsArray(), ResourceBase::pointerToString(codingPointer) + ": element must be array");
        for(auto codingItem = codingArray->Begin(), codingEnd = codingArray->End(); codingItem != codingEnd; ++codingItem)
        {
            const auto system = getOptionalStringValue(*codingItem, systemPointer);
            const auto code = getOptionalStringValue(*codingItem, codePointer);
            if(system.has_value() && system.value() == code_system::consentType &&
               code.has_value() && code.value() == chargingConsentType)
            {
                return true;
            }
        }
    }
    return false;
}

model::Timestamp Consent::dateTime() const
{
    return model::Timestamp::fromFhirDateTime(std::string(getStringValue(dateTimePointer)));
}

void Consent::fillId()
{
    ModelExpect(isChargingConsent(), "Unsupported Consent type");
    setValue(idPointer, createIdString(Consent::Type::CHARGCONS, patientKvnr()));
}

void Consent::setId(const std::string_view& id)
{
    setValue(idPointer, id);
}

void Consent::setPatientKvnr(const Kvnr& kvnr)
{
    setValue(patientKvnrValuePointer, kvnr.id());
}

void Consent::setDateTime(const model::Timestamp& dateTime)
{
    setValue(dateTimePointer, dateTime.toXsDateTime());
}

} // namespace model
