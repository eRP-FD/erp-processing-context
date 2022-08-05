/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/AuditEventTextTemplates.hxx"

#include "erp/util/TLog.hxx"
#include "erp/util/Expect.hxx"


namespace
{

constexpr std::string_view auditEventTextTemplates = R"--(
{
  "languageTextTemplates": [
    {
      "language": "de",
      "eventTextTemplates": [
        {
          "eventId": 0,
          "textTemplate": "{self} hat eine Liste von E-Rezepten heruntergeladen."
        },
        {
          "eventId": 1,
          "textTemplate": "{self} hat das Rezept mit der ID {prescriptionId} heruntergeladen."
        },
        {
          "eventId": 2,
          "textTemplate": "{agentName} hat das Rezept mit der ID {prescriptionId} heruntergeladen."
        },
        {
          "eventId": 3,
          "textTemplate": "{agentName} hat das Rezept mit der ID {prescriptionId} heruntergeladen."
        },
        {
          "eventId": 4,
          "textTemplate": "{agentName} hat das Rezept mit der ID {prescriptionId} eingestellt."
        },
        {
          "eventId": 5,
          "textTemplate": "{agentName} hat das Rezept mit der ID {prescriptionId} angenommen."
        },
        {
          "eventId": 6,
          "textTemplate": "{agentName} hat das Rezept mit der ID {prescriptionId} zurückgegeben."
        },
        {
          "eventId": 7,
          "textTemplate": "{agentName} hat das Rezept mit der ID {prescriptionId} beliefert."
        },
        {
          "eventId": 8,
          "textTemplate": "{agentName} hat das Rezept mit der ID {prescriptionId} gelöscht."
        },
        {
          "eventId": 9,
          "textTemplate": "{self} hat das Rezept mit der ID {prescriptionId} gelöscht."
        },
        {
          "eventId": 10,
          "textTemplate": "{agentName} hat das Rezept mit der ID {prescriptionId} gelöscht."
        },
        {
          "eventId": 11,
          "textTemplate": "{agentName} hat das Rezept mit der ID {prescriptionId} gelöscht."
        },
        {
          "eventId": 12,
          "textTemplate": "{self} hat eine Liste mit Medikament-Informationen heruntergeladen."
        },
        {
          "eventId": 13,
          "textTemplate": "{self} hat Medikament-Informationen zu dem Rezept mit der ID {prescriptionId} heruntergeladen."
        },
        {
          "eventId": 14,
          "textTemplate": "Veraltete Rezepte wurden vom Fachdienst automatisch gelöscht."
        },
        {
          "eventId": 15,
          "textTemplate": "{self} hat Abrechnungsinformationen zu dem Rezept mit der ID {prescriptionId} gelöscht."
        },
        {
          "eventId": 16,
          "textTemplate": "{agentName} hat Abrechnungsinformationen zu dem Rezept mit der ID {prescriptionId} gelöscht."
        },
        {
          "eventId": 17,
          "textTemplate": "{agentName} hat Abrechnungsinformationen zu dem Rezept mit der ID {prescriptionId} gespeichert."
        },
        {
          "eventId": 18,
          "textTemplate": "{self} hat Markierung zu Abrechnungsinformationen zu dem Rezept mit der ID {prescriptionId} geändert."
        },
        {
          "eventId": 19,
          "textTemplate": "{agentName} hat Abrechnungsinformationen zu dem Rezept mit der ID {prescriptionId} geändert."
        },
        {
          "eventId": 20,
          "textTemplate": "{self} hat Markierung zu Abrechnungsinformation gespeichert."
        },
        {
          "eventId": 21,
          "textTemplate": "{self} hat die Einwilligung erteilt."
        },
        {
          "eventId": 22,
          "textTemplate": "{self} hat die Einwilligung widerrufen."
        },
        {
          "eventId": 23,
          "textTemplate": "Veraltete Abrechnungsinformationen wurden vom Fachdienst automatisch gelöscht."
        }
      ]
    },
    {
      "language": "en",
      "eventTextTemplates": [
        {
          "eventId": 0,
          "textTemplate": "{self} retrieved a list of prescriptions."
        },
        {
          "eventId": 1,
          "textTemplate": "{self} downloaded a prescription {prescriptionId}."
        },
        {
          "eventId": 2,
          "textTemplate": "{agentName} downloaded a prescription {prescriptionId}."
        },
        {
          "eventId": 3,
          "textTemplate": "{agentName} downloaded a prescription {prescriptionId}."
        },
        {
          "eventId": 4,
          "textTemplate": "{agentName} activated a prescription {prescriptionId}."
        },
        {
          "eventId": 5,
          "textTemplate": "{agentName} accepted a prescription {prescriptionId}."
        },
        {
          "eventId": 6,
          "textTemplate": "{agentName} rejected a prescription {prescriptionId}."
        },
        {
          "eventId": 7,
          "textTemplate": "{agentName} closed a prescription {prescriptionId}."
        },
        {
          "eventId": 8,
          "textTemplate": "{agentName} deleted a prescription {prescriptionId}."
        },
        {
          "eventId": 9,
          "textTemplate": "{self} deleted a prescription {prescriptionId}."
        },
        {
          "eventId": 10,
          "textTemplate": "{agentName} deleted a prescription {prescriptionId}."
        },
        {
          "eventId": 11,
          "textTemplate": "{agentName} deleted a prescription {prescriptionId}."
        },
        {
          "eventId": 12,
          "textTemplate": "{self} downloaded a medication dispense list."
        },
        {
          "eventId": 13,
          "textTemplate": "{self} downloaded the medication dispense for a prescription {prescriptionId}."
        },
        {
          "eventId": 14,
          "textTemplate": "Outdated prescriptions were deleted automatically by the service."
        },
        {
          "eventId": 15,
          "textTemplate": "{self} deleted charging information for a prescription {prescriptionId}."
        },
        {
          "eventId": 16,
          "textTemplate": "{agentName} deleted charging information for a prescription {prescriptionId}."
        },
        {
          "eventId": 17,
          "textTemplate": "{agentName} stored charging information for a prescription {prescriptionId}."
        },
        {
          "eventId": 18,
          "textTemplate": "{self} changed marking in charging information for a prescription {prescriptionId}."
        },
        {
          "eventId": 19,
          "textTemplate": "{agentName} changed charging information for a prescription {prescriptionId}"
        },
        {
          "eventId": 20,
          "textTemplate": "{self} saved billing information flag."
        },
        {
          "eventId": 21,
          "textTemplate": "{self} gave the consent."
        },
        {
          "eventId": 22,
          "textTemplate": "{self} withdrew the consent."
        },
        {
          "eventId": 23,
          "textTemplate": "Outdated charging information was deleted automatically by the service."
        }
      ]
    }
  ]
})--";

// definition of JSON pointers:
const rapidjson::Pointer languageTextTemplatesPointer("/languageTextTemplates");
const rapidjson::Pointer languagePointer("/language");
const rapidjson::Pointer eventTextTemplatesPointer("/eventTextTemplates");
const rapidjson::Pointer eventIdPointer("/eventId");
const rapidjson::Pointer textTemplatePointer("/textTemplate");

}  // anonymous namespace


AuditEventTextTemplates::AuditEventTextTemplates()
    : mTextTemplates(){
    using namespace model;

    rapidjson::Document auditResourcesDocument;
    auditResourcesDocument.Parse(auditEventTextTemplates.data());
    Expect(!auditResourcesDocument.HasParseError(), "Error parsing audit event text templates JSON");

    const auto* languageTextTemplateArray = languageTextTemplatesPointer.Get(auditResourcesDocument);
    Expect(languageTextTemplateArray && languageTextTemplateArray->IsArray(),
           ResourceBase::pointerToString(languageTextTemplatesPointer) + " is mandatory and must be array");

    for(const auto& languageTextTemplate : languageTextTemplateArray->GetArray())
    {
        const auto* language = languagePointer.Get(languageTextTemplate);
        Expect(language && language->IsString(),
               ResourceBase::pointerToString(languagePointer) + " is mandatory and must be string");
        const auto* eventTextTemplateArray = eventTextTemplatesPointer.Get(languageTextTemplate);
        Expect(eventTextTemplateArray && eventTextTemplateArray->IsArray(),
               ResourceBase::pointerToString(eventTextTemplatesPointer) + " is mandatory and must be array");
        for(const auto& eventTextTemplate : eventTextTemplateArray->GetArray())
        {
            const auto* eventId = eventIdPointer.Get(eventTextTemplate);
            Expect(eventId && eventId->IsInt(),
                   ResourceBase::pointerToString(eventIdPointer) + " is mandatory and must be integer");
            Expect(eventId->GetInt() >= 0 && eventId->GetInt() <= static_cast<int>(AuditEventId::MAX),
                   "Invalid event id value " + std::to_string(eventId->GetInt()));
            const auto* textTemplate = textTemplatePointer.Get(eventTextTemplate);
            Expect(textTemplate && textTemplate->IsString(),
                   ResourceBase::pointerToString(textTemplatePointer) + " is mandatory and must be string");
            Expect(mTextTemplates[language->GetString()].insert(
                       std::make_pair(static_cast<AuditEventId>(eventId->GetUint()), textTemplate->GetString())).second,
                   "Event id " + std::to_string(eventId->GetUint()) + " multiply used for language " + language->GetString());
        }
    }

}


std::string AuditEventTextTemplates::retrieveTextTemplate(
    const model::AuditEventId eventId,
    const std::string& language) const
{
    try
    {
        const auto& resourcesForLanguage = mTextTemplates.at(language);
        return resourcesForLanguage.at(eventId);
    }
    catch(std::out_of_range& )
    {
        TVLOG(1) << "No audit event text resource found for language " << language << " and event id "
                 << static_cast<std::underlying_type<model::AuditEventId>::type>(eventId);
        throw std::logic_error("No audit event text resource found for event id / language combination");
    }
}
