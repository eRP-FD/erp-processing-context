/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
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
          "eventId": 19,
          "textTemplate": "{agentName} hat Abrechnungsinformationen zu dem Rezept mit der ID {prescriptionId} geändert."
        },
        {
          "eventId": 20,
          "textTemplate": "{self} hat die Einwilligung erteilt."
        },
        {
          "eventId": 21,
          "textTemplate": "{self} hat die Einwilligung widerrufen."
        },
        {
          "eventId": 22,
          "textTemplate": "Veraltete Abrechnungsinformationen wurden vom Fachdienst automatisch gelöscht."
        },
        {
          "eventId": 23,
          "textTemplate": "{self} hat Abrechnungsinformationen zu dem Rezept mit der ID {prescriptionId} gelesen."
        },
        {
          "eventId": 24,
          "textTemplate": "{agentName} hat Abrechnungsinformationen zu dem Rezept mit der ID {prescriptionId} gelesen."
        },
        {
          "eventId": 25,
          "textTemplate": "{self} hat Markierung zu Abrechnungsinformationen zu dem Rezept mit der ID {prescriptionId} geändert."
        },
        {
          "eventId": 26,
          "textTemplate": "{agentName} hat mit Ihrer eGK die Liste der offenen E-Rezepte abgerufen."
        },
        {
          "eventId": 28,
          "textTemplate": "{agentName} konnte aufgrund eines Fehlerfalls nicht die Liste der offenen E-Rezepte mit Ihrer eGK abrufen."
        },
        {
          "eventId": 29,
          "textTemplate": "Veraltete Nachrichten wurden vom Fachdienst automatisch gelöscht."
        },
        {
          "eventId": 30,
          "textTemplate": "{agentName} hat mit Ihrer eGK die Liste der offenen E-Rezepte abgerufen. (Offline-Check wurde akzeptiert)"
        },
        {
          "eventId": 31,
          "textTemplate": "{agentName} konnte aufgrund eines Fehlerfalls nicht die Liste der offenen E-Rezepte mit Ihrer eGK abrufen. (Offline-Check wurde nicht akzeptiert)"
        },
        {
          "eventId": 32,
          "textTemplate": "{agentName} hat das Rezept mit der ID {prescriptionId} beliefert."
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
          "eventId": 19,
          "textTemplate": "{agentName} changed charging information for a prescription {prescriptionId}."
        },
        {
          "eventId": 20,
          "textTemplate": "{self} gave the consent."
        },
        {
          "eventId": 21,
          "textTemplate": "{self} withdrew the consent."
        },
        {
          "eventId": 22,
          "textTemplate": "Outdated charging information was deleted automatically by the service."
        },
        {
          "eventId": 23,
          "textTemplate": "{self} read charging information for the prescription {prescriptionId}."
        },
        {
          "eventId": 24,
          "textTemplate": "{agentName} read charging information for the prescription {prescriptionId}."
        },
        {
          "eventId": 25,
          "textTemplate": "{self} changed marking in charging information for a prescription {prescriptionId}."
        },
        {
          "eventId": 26,
          "textTemplate": "{agentName} retrieved your dispensable e-prescriptions using your health card."
        },
        {
          "eventId": 28,
          "textTemplate": "{agentName} was not able to retrieve your e-prescriptions due to an error with your health card."
        },
        {
          "eventId": 29,
          "textTemplate": "Outdated communications were deleted automatically by the service."
        },
        {
          "eventId": 30,
          "textTemplate": "{agentName} retrieved your dispensable e-prescriptions using your health card. (Offline-Check get accepted)"
        },
        {
          "eventId": 31,
          "textTemplate": "{agentName} was not able to retrieve your e-prescriptions due to an error with your health card. (Offline-Check was not accepted)"
        },
        {
          "eventId": 32,
          "textTemplate": "{agentName} dispensed a prescription {prescriptionId}."
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


AuditEventTextTemplates::TextTemplate AuditEventTextTemplates::retrieveTextTemplate(
    const model::AuditEventId eventId,
    const std::string& requestedLanguage) const
{
    std::string language = requestedLanguage;
    try
    {
        const auto& resourcesForLanguage = getId2TextContainer(language);
        return { resourcesForLanguage.at(eventId), language };
    }
    catch(std::out_of_range& )
    {
        TLOG(WARNING) << "No audit event text resource found for language \"" << language << "\" and event id "
                      << static_cast<std::underlying_type<model::AuditEventId>::type>(eventId);
        Fail2("No audit event text resource found for event id / language combination", std::logic_error);
    }
}


const AuditEventTextTemplates::Id2TextContainer&
AuditEventTextTemplates::getId2TextContainer(std::string& inOutLanguage) const
{
    try
    {
        return mTextTemplates.at(inOutLanguage);
    }
    catch(std::out_of_range& )
    {
        TLOG(WARNING) << "Unsupported language " << inOutLanguage << ", using \"" << AuditEventTextTemplates::defaultLanguage << "\" instead";
        inOutLanguage = AuditEventTextTemplates::defaultLanguage;
        try
        {
            return mTextTemplates.at(inOutLanguage);
        }
        catch(std::out_of_range& )
        {
            TLOG(WARNING) << "No text templates for default language \"" << inOutLanguage << "\" found";
            Fail2("No text templates for default language found", std::logic_error);
        }
    }
}
