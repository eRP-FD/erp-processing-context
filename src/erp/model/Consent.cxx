/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Consent.hxx"
#include "shared/model/RapidjsonDocument.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/util/Expect.hxx"

#include <magic_enum/magic_enum.hpp>
#include <rapidjson/pointer.h>
#include <mutex>


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
          "system": "",
          "code": "",
          "display": ""
        }
      ]
    }
  ],
  "patient": {
    "identifier": {
      "system": "http://fhir.de/sid/gkv/kvid-10",
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
const rapidjson::Pointer patientKvnrValuePointer(ElementName::path(elements::patient, elements::identifier,
                                                                   elements::value));
const rapidjson::Pointer patientKvnrSystemPointer(ElementName::path(elements::patient, elements::identifier,
                                                                    elements::system));
const rapidjson::Pointer dateTimePointer(ElementName::path(elements::dateTime));

const rapidjson::Pointer categoryPointer(ElementName::path(elements::category));
const rapidjson::Pointer codingPointer(ElementName::path(elements::coding));
const rapidjson::Pointer systemPointer(ElementName::path(elements::system));
const rapidjson::Pointer codePointer(ElementName::path(elements::code));

const auto consentIdSeparator = '-';

}// anonymous namespace

ProfileType Consent::profileType() const
{
    const auto profileName = getProfileName();
    ModelExpect(profileName.has_value(), "Profile missing in Consent resource.");
    const auto profileType = findProfileType(*profileName);
    ModelExpect(profileType.has_value(), "Unsupported profile type: " + std::string{*profileName});
    return *profileType;
}

ProfileType Consent::profileType(ConsentType type)
{
    switch (type)
    {
        case ConsentType::CHARGCONS:
            return ProfileType::GEM_ERPCHRG_PR_Consent;
        case ConsentType::EUDISPCONS:
            return ProfileType::GEM_ERPEU_PR_Consent;
    }
    ModelFail("Invalid Consent type: " + std::to_string(static_cast<uintmax_t>(type)));
}

// static
std::string Consent::createIdString(ConsentType type, const Kvnr& kvnr)
{
    return std::string(magic_enum::enum_name(type)) + consentIdSeparator + kvnr.id();
}

std::string Consent::createIdString(model::AuditEventId auditEventId, const model::Kvnr& kvnr)
{
    if (auditEventId == model::AuditEventId::POST_Consent || auditEventId == model::AuditEventId::DELETE_Consent)
    {
        return createIdString(model::ConsentType::CHARGCONS, kvnr);
    }
    if (auditEventId == model::AuditEventId::POST_EU_Consent || auditEventId == model::AuditEventId::DELETE_EU_Consent)
    {
        return createIdString(model::ConsentType::EUDISPCONS, kvnr);
    }
    ModelFail("invalid audit id for consent: " + std::string{magic_enum::enum_name(auditEventId)});
}

// static
std::pair<ConsentType, std::string> Consent::splitIdString(const std::string_view& idStr)
{
    const auto sepPos = idStr.find(consentIdSeparator);
    ModelExpect(sepPos != std::string::npos, "Invalid Consent id " + std::string(idStr));
    const auto type = magic_enum::enum_cast<ConsentType>(idStr.substr(0, sepPos));
    ModelExpect(type.has_value(), "Invalid Consent type in id " + std::string(idStr));
    const auto kvnr = idStr.substr(sepPos + 1);
    return std::make_pair(*type, std::string(kvnr));
}


Consent::Consent(ConsentType category, const Kvnr& kvnr, const model::Timestamp& dateTime)
    : Resource(profileType(category),
               []() {
                   std::call_once(onceFlag, initTemplates);
                   return ConsentTemplate;
               }()
                   .instance())
{
    setPatientKvnr(kvnr);
    setDateTime(dateTime);
    fillCategory(category);
    fillId(category, kvnr);
}

Consent::Consent(NumberAsStringParserDocument&& jsonTree)
    : Resource(std::move(jsonTree))
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
    try
    {
        switch (consentCategory())
        {
            case ConsentType::CHARGCONS:
                return true;
            case ConsentType::EUDISPCONS:
                break;
        }
    }
    catch (const ModelException&)
    {
        // not handled, false is returned.
    }
    return false;
}

model::Timestamp Consent::dateTime() const
{
    return model::Timestamp::fromFhirDateTime(std::string(getStringValue(dateTimePointer)));
}

ConsentType Consent::consentCategory() const
{
    const auto* categoryArray = getValue(categoryPointer);
    ModelExpect(categoryArray && categoryArray->IsArray() && categoryArray->GetArray().Size() == 1,
                ResourceBase::pointerToString(categoryPointer) + ": element must be array of size 1");
    const auto* codingArray = getValue(categoryArray->GetArray()[0], codingPointer);
    ModelExpect(codingArray && codingArray->IsArray() && codingArray->GetArray().Size() == 1,
                ResourceBase::pointerToString(codingPointer) + ": element must be array of size 1");
    const auto system = getOptionalStringValue(codingArray->GetArray()[0], systemPointer);
    const auto code = getOptionalStringValue(codingArray->GetArray()[0], codePointer);
    ModelExpect(system.has_value() && code.has_value(),
                ResourceBase::pointerToString(codingPointer) + ": system and code must be present");
    const auto enumOpt = magic_enum::enum_cast<ConsentType>(code.value());
    ModelExpect(enumOpt.has_value(),
                "Unsupported Consent category: " + std::string{system.value()} + "/" + std::string{code.value()});
    // the system is already checked by fhir validator
    return enumOpt.value();
}

void Consent::fillId(ConsentType category, const Kvnr& kvnr)
{
    setValue(idPointer, createIdString(category, kvnr));
}

void Consent::fillCategory(ConsentType category)
{
    static const rapidjson::Pointer systemPointer(ElementName::path(elements::category, 0, elements::coding, 0, elements::system));
    static const rapidjson::Pointer codePointer(ElementName::path(elements::category, 0, elements::coding, 0, elements::code));
    static const rapidjson::Pointer displayPointer(ElementName::path(elements::category, 0, elements::coding, 0, elements::display));
    switch (category)
    {
        case ConsentType::CHARGCONS:
            setValue(systemPointer, "https://gematik.de/fhir/erpchrg/CodeSystem/GEM_ERPCHRG_CS_ConsentType");
            setValue(codePointer, "CHARGCONS");
            setValue(displayPointer, "Consent for saving electronic charge item");
            break;
        case ConsentType::EUDISPCONS:
            setValue(systemPointer, "https://gematik.de/fhir/erp-eu/CodeSystem/GEM_ERPEU_CS_ConsentType");
            setValue(codePointer, "EUDISPCONS");
            setValue(displayPointer, "Consent for redeeming e-prescriptions in EU countries");
            break;
    }
}

void Consent::setPatientKvnr(const Kvnr& kvnr)
{
    setValue(patientKvnrValuePointer, kvnr.id());
}

void Consent::setDateTime(const model::Timestamp& dateTime)
{
    setValue(dateTimePointer, dateTime.toXsDateTime());
}


std::optional<model::Timestamp> Consent::getValidationReferenceTimestamp() const
{
    return Timestamp::now();
}
}// namespace model
