/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/audit/AuditEventTextTemplates.hxx"
#include "erp/pc/popp/PoPPToken.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/TLog.hxx"

#include <iostream>


namespace
{

constexpr const auto* auditEventTextTemplates = R"--(
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
          "textTemplate": "{self} hat die Einwilligung zur Speicherung der elektronischen Abrechnungsinformationen erteilt."
        },
        {
          "eventId": 21,
          "textTemplate": "{self} hat die Einwilligung zur Speicherung der elektronischen Abrechnungsinformationen widerrufen."
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
        },
        {
          "eventId": 33,
          "textTemplate": "Die Verordnung wurde in die Patientenakte übertragen."
        },
        {
          "eventId": 34,
          "textTemplate": "Die Verordnung konnte nicht in die Patientenakte übertragen werden."
        },
        {
          "eventId": 35,
          "textTemplate": "Die Medikamentenabgabe wurde in die Patientenakte übertragen."
        },
        {
          "eventId": 36,
          "textTemplate": "Die Medikamentenabgabe konnte nicht in die Patientenakte übertragen werden."
        },
        {
          "eventId": 37,
          "textTemplate": "Die Löschinformation zum E-Rezept wurde in die Patientenakte übermittelt."
        },
        {
          "eventId": 38,
          "textTemplate": "Die Löschinformation zum E-Rezept konnte nicht in die Patientenakte übermittelt werden."
        },
        {
          "eventId": 39,
          "textTemplate": "Die Löschinformation für die Medikamentenabgabe wurde in die Patientenakte übertragen."
        },
        {
          "eventId": 40,
          "textTemplate": "Die Löschinformation für die Medikamentenabgabe konnte nicht in die Patientenakte übertragen werden."
        },
        {
          "eventId": 41,
          "textTemplate": "Sie haben eine Zugriffsberechtigung zum Einlösen von E-Rezepten für das Land {countryCode} erteilt."
        },
        {
          "eventId": 42,
          "textTemplate": "Sie haben die Zugriffsberechtigung zum Einlösen von E-Rezepten für das Land {countryCode} gelöscht."
        },
        {
          "eventId": 43,
          "textTemplate": "{self} hat Markierung zu Einlösung im EU Ausland gespeichert."
        },
        {
          "eventId": 44,
          "textTemplate": "Der {agentName} hat in {hftpoc} in Land {countryCode} Ihre Patientendaten abgerufen."
        },
        {
          "eventId": 45,
          "textTemplate": "Der {agentName} hat in {hftpoc} in Land {countryCode} Ihre einlösbaren E-Rezepte abgerufen."
        },
        {
          "eventId": 46,
          "textTemplate": "Der {agentName} hat in {hftpoc} in Land {countryCode} Ihre einzulösenden E-Rezepte abgerufen."
        },
        {
          "eventId": 47,
          "textTemplate": "{self} hat die Einwilligung zur Einlösung von E-Rezepten in EU-Ländern erteilt."
        },
        {
          "eventId": 48,
          "textTemplate": "{self} hat die Einwilligung zur Einlösung von E-Rezepten in EU-Ländern widerrufen."
        },
        {
          "eventId": 49,
          "textTemplate": "Der {practitioner} hat in {facility} in Land {countryCode} Ihr E-Rezept eingelöst."
        },
        {
          "eventId": 50,
          "textTemplate": "{agentName} hat die Liste der einlösbaren E-Rezepte abgerufen durch Autorisierung mittels {proofMethod}."
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
          "textTemplate": "{self} gave the consent for saving electronic charge item."
        },
        {
          "eventId": 21,
          "textTemplate": "{self} withdrew the consent for saving electronic charge item."
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
        },
        {
          "eventId": 33,
          "textTemplate": "Prescription has been transferred into Patientenakte."
        },
        {
          "eventId": 34,
          "textTemplate": "Prescription could not be transferred into Patientenakte."
        },
        {
          "eventId": 35,
          "textTemplate": "Dispensation has been transferred into Patientenakte."
        },
        {
          "eventId": 36,
          "textTemplate": "Dispensation could not be transferred into Patientenakte."
        },
        {
          "eventId": 37,
          "textTemplate": "Deletion of prescription has been transferred into Patientenakte."
        },
        {
          "eventId": 38,
          "textTemplate": "Deletion of Prescription could not be transferred into Patientenakte."
        },
        {
          "eventId": 39,
          "textTemplate": "Deletion of Dispensation has been transferred into Patientenakte."
        },
        {
          "eventId": 40,
          "textTemplate": "Deletion of Dispensation could not be transferred into Patientenakte."
        },
        {
          "eventId": 41,
          "textTemplate": "You have granted authorization to redeem e-prescriptions for the country {countryCode}."
        },
        {
          "eventId": 42,
          "textTemplate": "You have revoked authorization to redeem e-prescriptions for the country {countryCode}."
        },
        {
          "eventId": 43,
          "textTemplate": "{self} has saved a marker for redemption in the EU."
        },
        {
          "eventId": 44,
          "textTemplate": "The {agentName} retrieved your patient data at {hftpoc} in country {countryCode}."
        },
        {
          "eventId": 45,
          "textTemplate": "The {agentName} retrieved a list of redeemable prescriptions at {hftpoc} in country {countryCode}."
        },
        {
          "eventId": 46,
          "textTemplate": "The {agentName} retrieved a list of prescriptions to be redeemed at {hftpoc} in country {countryCode}."
        },
        {
          "eventId": 47,
          "textTemplate": "{self} has given the consent for redeeming e-prescriptions in EU countries."
        },
        {
          "eventId": 48,
          "textTemplate": "{self} has withdrawn the consent for redeeming e-prescriptions in EU countries."
        },
        {
          "eventId": 49,
          "textTemplate": "{practitioner} at {facility} in country {countryCode} has redeemed your e-prescription."
        },
        {
          "eventId": 50,
          "textTemplate": "{agentName} retrieved a list of prescriptions to be redeemed by authorization with {proofMethod}."
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
    auditResourcesDocument.Parse(auditEventTextTemplates);
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
                      << static_cast<std::underlying_type_t<model::AuditEventId>>(eventId);
        Fail2("No audit event text resource found for event id / language combination", std::logic_error);
    }
}

std::string AuditEventTextTemplates::proofMethodString(const std::string& proofMethodStr, const std::string& language)
{
    using namespace std::string_literals;
    auto proofMethod = magic_enum::enum_cast<PoPPTokenProofMethodPrefix>(proofMethodStr);
    if (proofMethod)
    {
        switch (*proofMethod)
        {
            case PoPPTokenProofMethodPrefix::healthid:
                return "GesundheitsID"s;
            case PoPPTokenProofMethodPrefix::ehc_practitioner:
                return language == "de"s ? "Gesundheitskarte in der Apotheke" : "the health card at the pharmacy"s;
            case PoPPTokenProofMethodPrefix::ehc_provider:
                return language == "de"s ? "Gesundheitskarte über ein Endgerät des Versicherten"s
                                         : "the insured person’s health card on their device"s;
            case PoPPTokenProofMethodPrefix::unknown:
                break;
        }
    }
    return language == "de"s ? "eines Nachweises des Versorgungskontextes" : "a proof of presence";
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
