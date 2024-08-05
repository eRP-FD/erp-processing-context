/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_AUDITEVENTCREATOR_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_AUDITEVENTCREATOR_HXX

#include <string>

namespace model
{
class AuditData;
class AuditEvent;
};

class JWT;
class AuditEventTextTemplates;

class AuditEventCreator
{
public:
    static model::AuditEvent fromAuditData(const model::AuditData& auditData, const std::string& language,
                                           const AuditEventTextTemplates& textResources, const JWT& accessToken);
};


#endif
