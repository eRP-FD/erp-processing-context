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
          "textTemplate": "{self} hat ein E-Rezept {prescriptionId} heruntergeladen."
        },
        {
          "eventId": 2,
          "textTemplate": "{agentName} hat ein E-Rezept {prescriptionId} heruntergeladen."
        },
        {
          "eventId": 3,
          "textTemplate": "{agentName} hat ein E-Rezept {prescriptionId} heruntergeladen."
        },
        {
          "eventId": 4,
          "textTemplate": "{agentName} hat ein E-Rezept {prescriptionId} eingestellt."
        },
        {
          "eventId": 5,
          "textTemplate": "{agentName} hat ein E-Rezept {prescriptionId} angenommen."
        },
        {
          "eventId": 6,
          "textTemplate": "{agentName} hat ein E-Rezept {prescriptionId} zurückgegeben."
        },
        {
          "eventId": 7,
          "textTemplate": "{agentName} hat ein E-Rezept {prescriptionId} beliefert."
        },
        {
          "eventId": 8,
          "textTemplate": "{agentName} hat ein E-Rezept {prescriptionId} gelöscht."
        },
        {
          "eventId": 9,
          "textTemplate": "{self} hat ein E-Rezept {prescriptionId} gelöscht."
        },
        {
          "eventId": 10,
          "textTemplate": "{agentName} hat ein E-Rezept {prescriptionId} gelöscht."
        },
        {
          "eventId": 11,
          "textTemplate": "{agentName} hat ein E-Rezept {prescriptionId} gelöscht."
        },
        {
          "eventId": 12,
          "textTemplate": "{self} hat eine Liste von Medikament-Informationen heruntergeladen."
        },
        {
          "eventId": 13,
          "textTemplate": "{self} hat Medikament-Informationen zu einem E-Rezept {prescriptionId} heruntergeladen."
        },
        {
          "eventId": 14,
          "textTemplate": "Veraltete E-Rezepte wurden vom Fachdienst automatisch gelöscht."
        }
      ]
    },
    {
      "language": "en",
      "eventTextTemplates": [
        {
          "eventId": 0,
          "textTemplate": "{self} retrieved a list of e-prescriptions."
        },
        {
          "eventId": 1,
          "textTemplate": "{self} downloaded an e-prescription {prescriptionId}."
        },
        {
          "eventId": 2,
          "textTemplate": "{agentName} downloaded an e-prescription {prescriptionId}."
        },
        {
          "eventId": 3,
          "textTemplate": "{agentName} downloaded an e-prescription {prescriptionId}."
        },
        {
          "eventId": 4,
          "textTemplate": "{agentName} activated an e-prescription {prescriptionId}."
        },
        {
          "eventId": 5,
          "textTemplate": "{agentName} accepted an e-prescription {prescriptionId}."
        },
        {
          "eventId": 6,
          "textTemplate": "{agentName} rejected an e-prescription {prescriptionId}."
        },
        {
          "eventId": 7,
          "textTemplate": "{agentName} closed an e-prescription {prescriptionId}."
        },
        {
          "eventId": 8,
          "textTemplate": "{agentName} deleted an e-prescription {prescriptionId}."
        },
        {
          "eventId": 9,
          "textTemplate": "{self} deleted an e-prescription {prescriptionId}."
        },
        {
          "eventId": 10,
          "textTemplate": "{agentName} deleted an e-prescription {prescriptionId}."
        },
        {
          "eventId": 11,
          "textTemplate": "{agentName} deleted an e-prescription {prescriptionId}."
        },
        {
          "eventId": 12,
          "textTemplate": "{self} downloaded a medication dispense list."
        },
        {
          "eventId": 13,
          "textTemplate": "{self} downloaded the medication dispense for an e-prescription {prescriptionId}."
        },
        {
          "eventId": 14,
          "textTemplate": "Outdated e-prescriptions where deleted automatically by the service."
        }
      ]
    }
  ]
}
)--";

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
