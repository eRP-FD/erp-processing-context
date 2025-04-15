/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_AUDITEVENTEXTRESOURCES_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_AUDITEVENTEXTRESOURCES_HXX

#include "shared/model/AuditData.hxx"

#include <unordered_map>

class AuditEventTextTemplates
{
public:
    AuditEventTextTemplates();

    struct TextTemplate
    {
        std::string text;
        std::string language;
    };
    TextTemplate retrieveTextTemplate(
        const model::AuditEventId eventId,
        const std::string& requestedLanguage) const;

    static constexpr std::string_view defaultLanguage = "en";

    static constexpr std::string_view selfVariableName = "{self}";
    static constexpr std::string_view agentNameVariableName = "{agentName}";
    static constexpr std::string_view prescriptionIdVariableName = "{prescriptionId}";
private:
    using Id2TextContainer = std::unordered_map<model::AuditEventId, std::string>;
    // Language -> (event-id -> text-template)
    std::unordered_map<std::string, Id2TextContainer> mTextTemplates;

    const Id2TextContainer& getId2TextContainer(std::string& inOutLanguage) const;
};


#endif
