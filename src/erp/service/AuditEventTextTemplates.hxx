#ifndef ERP_PROCESSING_CONTEXT_SERVICE_AUDITEVENTEXTRESOURCES_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_AUDITEVENTEXTRESOURCES_HXX

#include "erp/model/AuditData.hxx"

#include <unordered_map>

class AuditEventTextTemplates
{
public:
    AuditEventTextTemplates();

    std::string retrieveTextTemplate(
        const model::AuditEventId eventId,
        const std::string& language) const;

    static constexpr std::string_view selfVariableName = "{self}";
    static constexpr std::string_view agentNameVariableName = "{agentName}";
    static constexpr std::string_view prescriptionIdVariableName = "{prescriptionId}";

private:
    // Language -> (event-id -> text-template)
    std::unordered_map<std::string, std::unordered_map<model::AuditEventId, std::string>> mTextTemplates;

};


#endif
