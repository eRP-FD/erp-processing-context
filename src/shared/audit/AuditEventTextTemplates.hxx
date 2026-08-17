/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_AUDITEVENTEXTRESOURCES_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_AUDITEVENTEXTRESOURCES_HXX

#include "shared/ErpRequirements.hxx"
#include "shared/model/AuditData.hxx"

#include <unordered_map>

class AuditEventTextTemplates
{
public:
    static constexpr auto implementsA_19284 = A_19284_14.implements("Versichertenprotokoll zu Operationen");
    AuditEventTextTemplates();

    struct TextTemplate
    {
        std::string text;
        std::string language;
    };
    TextTemplate retrieveTextTemplate(
        const model::AuditEventId eventId,
        const std::string& requestedLanguage) const;

    static std::string proofMethodString(const std::string& proofMethodStr, const std::string& language);

    static constexpr std::string_view defaultLanguage = "en";

    static constexpr std::string_view selfVariableName = "{self}";
    static constexpr std::string_view agentNameVariableName = "{agentName}";
    static constexpr std::string_view prescriptionIdVariableName = "{prescriptionId}";
    static constexpr std::string_view countryCodeVariableName = "{countryCode}";
    static constexpr std::string_view proofMethodVariableName = "{proofMethod}";
    static constexpr std::string_view proofMethodVariableNameRaw = "proofMethod";
    static constexpr std::string_view pushDeviceVariableName = "{device_display_name}";
    static constexpr std::string_view pushDeviceVariableNameRaw = "device_display_name";
private:
    using Id2TextContainer = std::unordered_map<model::AuditEventId, std::string>;
    // Language -> (event-id -> text-template)
    std::unordered_map<std::string, Id2TextContainer> mTextTemplates;

    const Id2TextContainer& getId2TextContainer(std::string& inOutLanguage) const;
};


#endif
