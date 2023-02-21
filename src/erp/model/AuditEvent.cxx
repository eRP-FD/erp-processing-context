/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/AuditEvent.hxx"
#include "erp/model/ResourceNames.hxx"

#include "erp/util/Expect.hxx"
#include "erp/util/RapidjsonDocument.hxx"

#include <rapidjson/pointer.h>
#include <mutex>// for call_once


namespace model
{
using namespace std::string_literals;

const std::string auditevent_template = R"--(
{
  "resourceType": "AuditEvent",
  "id": "",
  "meta": {
    "profile": [
      ""
    ]
  },
  "text": {
    "status": "generated",
    "div": ""
  },
  "language": "",
  "type": {
    "system": "http://terminology.hl7.org/CodeSystem/audit-event-type",
    "code": "rest"
  },
  "subtype": [
    {
      "system": "http://hl7.org/fhir/restful-interaction",
      "code": ""
    }
  ],
  "action": "",
  "recorded": "",
  "outcome": "",
  "agent": [
    {
      "type": {
        "coding": [
          {
            "system": "http://terminology.hl7.org/CodeSystem/extra-security-role-type",
            "code": "",
            "display": ""
          }
        ]
      },
      "name": "",
      "requestor": false
    }
  ],
  "source": {
    "site": "E-Rezept Fachdienst",
    "observer": {
      "reference": ""
    }
  },
  "entity": [
    {
      "what": {
        "reference": ""
      },
      "name": "",
      "description": ""
    }
  ]
}
)--";

const std::unordered_map<AuditEvent::SubType, std::string_view> AuditEvent::SubTypeNames = {
    { AuditEvent::SubType::create, "create" },
    { AuditEvent::SubType::read, "read" },
    { AuditEvent::SubType::update, "update" },
    { AuditEvent::SubType::del, "delete" } };

const std::unordered_map<std::string_view, AuditEvent::SubType> AuditEvent::SubTypeNamesReverse = {
    { "create", AuditEvent::SubType::create },
    { "read", AuditEvent::SubType::read },
    { "update", AuditEvent::SubType::update },
    { "delete", AuditEvent::SubType::del } };

const std::unordered_map<std::string_view, AuditEvent::Action> AuditEvent::ActionNamesReverse = {
    { "C", AuditEvent::Action::create },
    { "R", AuditEvent::Action::read },
    { "U", AuditEvent::Action::update },
    { "D", AuditEvent::Action::del } };

const std::unordered_map<std::string_view, AuditEvent::Outcome> AuditEvent::OutcomeNamesReverse = {
    { "0", AuditEvent::Outcome::success },
    { "4", AuditEvent::Outcome::minorFailure },
    { "8", AuditEvent::Outcome::seriousFailure } };

const std::unordered_map<AuditEvent::Action, AuditEvent::SubType> AuditEvent::ActionToSubType = {
    { AuditEvent::Action::create, AuditEvent::SubType::create },
    { AuditEvent::Action::read, AuditEvent::SubType::read },
    { AuditEvent::Action::update, AuditEvent::SubType::update },
    { AuditEvent::Action::del, AuditEvent::SubType::del } };

const std::unordered_map<std::string_view, std::string_view> AuditEvent::SubTypeNameToActionName = {
    { "create", "C" },
    { "read", "R" },
    { "update", "U" },
    { "delete", "D" } };

const std::unordered_map<AuditEvent::AgentType, std::pair<std::string_view,std::string_view>> AuditEvent::AgentTypeStrings = {
    { AuditEvent::AgentType::human, { "humanuser", "Human User" } },
    { AuditEvent::AgentType::machine, { "dataprocessor", "Data Processor" } } };

const std::unordered_map<std::string_view, AuditEvent::AgentType> AuditEvent::AgentTypeNamesReverse = {
    { "humanuser", AuditEvent::AgentType::human },
    { "dataprocessor", AuditEvent::AgentType::machine } };


namespace
{

std::once_flag onceFlag;
struct AuditEventTemplateMark;
RapidjsonNumberAsStringParserDocument<AuditEventTemplateMark> AuditEventTemplate;

void initTemplates()
{
    rapidjson::StringStream s1(auditevent_template.data());
    AuditEventTemplate->ParseStream<rapidjson::kParseNumbersAsStringsFlag, rapidjson::CustomUtf8>(s1);
}

// definition of JSON pointers:
const rapidjson::Pointer idPointer("/id");
const rapidjson::Pointer languagePointer("/language");
const rapidjson::Pointer textDivPointer("/text/div");
const rapidjson::Pointer subTypeCodePointer("/subtype/0/code");
const rapidjson::Pointer actionPointer("/action");
const rapidjson::Pointer recordedPointer("/recorded");
const rapidjson::Pointer outcomePointer("/outcome");

const rapidjson::Pointer agentWhoIdentifierSystemPointer("/agent/0/who/identifier/system");
const rapidjson::Pointer agentWhoIdentifierValuePointer("/agent/0/who/identifier/value");
const rapidjson::Pointer agentNamePointer("/agent/0/name");
const rapidjson::Pointer agentTypeCodePointer("/agent/0/type/coding/0/code");
const rapidjson::Pointer agentTypeDisplayPointer("/agent/0/type/coding/0/display");

const rapidjson::Pointer sourceObserverReferencePointer("/source/observer/reference");

const rapidjson::Pointer entityWhatIdentifierUsePointer("/entity/0/what/identifier/use");
const rapidjson::Pointer entityWhatIdentifierSystemPointer("/entity/0/what/identifier/system");
const rapidjson::Pointer entityWhatIdentifierValuePointer("/entity/0/what/identifier/value");
const rapidjson::Pointer entityWhatReferencePointer("/entity/0/what/reference");
const rapidjson::Pointer entityNamePointer("/entity/0/name");
const rapidjson::Pointer entityDescriptionPointer("/entity/0/description");

}  // anonymous namespace


AuditEvent::AuditEvent(ResourceVersion::DeGematikErezeptWorkflowR4 profileVersion)
    : Resource<AuditEvent>(ResourceVersion::deprecatedProfile(profileVersion)
                               ? resource::structure_definition::deprecated::auditEvent
                               : resource::structure_definition::auditEvent,
                           []() {
                               std::call_once(onceFlag, initTemplates);
                               return AuditEventTemplate;
                           }()
                               .instance())
{
}

void AuditEvent::setId(const std::string_view& id)
{
    setValue(idPointer, id);
}

void AuditEvent::setLanguage(const std::string_view& language)
{
    setValue(languagePointer, language);
}

void AuditEvent::setTextDiv(const std::string_view& textDiv)
{
    setValue(textDivPointer, textDiv);
}

void AuditEvent::setSubTypeCode(const SubType subTypeCode)
{
    setValue(subTypeCodePointer, SubTypeNames.at(subTypeCode));
}

void AuditEvent::setAction(const Action action)
{
    setValue(actionPointer, std::string(1, static_cast<char>(action)));
}

void AuditEvent::setRecorded(const model::Timestamp& recorded)
{
    setValue(recordedPointer, recorded.toXsDateTime());
}

void AuditEvent::setOutcome(const Outcome outcome)
{
    setValue(outcomePointer, std::to_string(static_cast<int>(outcome)));
}

void AuditEvent::setAgentWho(
    const std::string_view& identifierSystem,
    const std::string_view& identifierValue)
{
    setValue(agentWhoIdentifierSystemPointer, identifierSystem);
    setValue(agentWhoIdentifierValuePointer, identifierValue);
}

void AuditEvent::setAgentName(const std::string_view& agentName)
{
    setValue(agentNamePointer, agentName);
}

void AuditEvent::setAgentType(const AgentType agentType)
{
    const auto [agentTypeCode, agentTypeDisplay] = AgentTypeStrings.at(agentType);
    setValue(agentTypeCodePointer, agentTypeCode);
    setValue(agentTypeDisplayPointer, agentTypeDisplay);
}

void AuditEvent::setSourceObserverReference(const std::string_view& reference)
{
    setValue(sourceObserverReferencePointer, reference);
}

void AuditEvent::setEntityWhatIdentifier(
    const std::optional<std::string_view>& identifierSystem,
    const std::string_view& identifierValue)
{
    if (identifierSystem.has_value())
    {
        setValue(entityWhatIdentifierUsePointer, "official");
        setValue(entityWhatIdentifierSystemPointer, *identifierSystem);
    }
    setValue(entityWhatIdentifierValuePointer, identifierValue);
}

void AuditEvent::setEntityWhatReference(const std::string_view& reference)
{
    setValue(entityWhatReferencePointer, reference);
}

void AuditEvent::setEntityName(const std::string_view& entityName)
{
    setValue(entityNamePointer, entityName);
}

void AuditEvent::setEntityDescription(const std::string_view& entityDescription)
{
    setValue(entityDescriptionPointer, entityDescription);
}

std::string_view AuditEvent::id() const
{
    return getStringValue(idPointer);
}

std::string_view AuditEvent::language() const
{
    return getStringValue(languagePointer);
}

std::string_view AuditEvent::textDiv() const
{
    return getStringValue(textDivPointer);
}

AuditEvent::SubType AuditEvent::subTypeCode() const
{
    return SubTypeNamesReverse.at(getStringValue(subTypeCodePointer));
}

AuditEvent::Action AuditEvent::action() const
{
    return ActionNamesReverse.at(getStringValue(actionPointer));
}

model::Timestamp AuditEvent::recorded() const
{
    return model::Timestamp::fromXsDateTime(std::string(getStringValue(recordedPointer)));
}

AuditEvent::Outcome AuditEvent::outcome() const
{
    return OutcomeNamesReverse.at(getStringValue(outcomePointer));
}

std::tuple<std::optional<std::string_view>, std::optional<std::string_view>> AuditEvent::agentWho() const
{
    return std::make_tuple(
        getOptionalStringValue(agentWhoIdentifierSystemPointer),
        getOptionalStringValue(agentWhoIdentifierValuePointer));
}

std::string_view AuditEvent::agentName() const
{
    return getStringValue(agentNamePointer);
}

AuditEvent::AgentType AuditEvent::agentType() const
{
    return AgentTypeNamesReverse.at(getStringValue(agentTypeCodePointer));
}

std::string_view AuditEvent::sourceObserverReference() const
{
    return getStringValue(sourceObserverReferencePointer);
}

std::tuple<std::optional<std::string_view>, std::optional<std::string_view>> AuditEvent::entityWhatIdentifier() const
{
    return std::make_tuple(
        getOptionalStringValue(entityWhatIdentifierSystemPointer),
        getOptionalStringValue(entityWhatIdentifierValuePointer));
}

std::string_view AuditEvent::entityWhatReference() const
{
    return getStringValue(entityWhatReferencePointer);
}

std::string_view AuditEvent::entityName() const
{
    return getStringValue(entityNamePointer);
}

std::string_view AuditEvent::entityDescription() const
{
    return getStringValue(entityDescriptionPointer);
}

AuditEvent::AuditEvent(NumberAsStringParserDocument&& jsonTree)
    : Resource<AuditEvent>(std::move(jsonTree))
{
    std::call_once(onceFlag, initTemplates);
}

}  // namespace model
