/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_AUDITEVENTCREATOR_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_AUDITEVENTCREATOR_HXX

#include "erp/service/AuditEventTextTemplates.hxx"
#include "erp/model/AuditData.hxx"
#include "erp/model/AuditEvent.hxx"
#include "erp/model/ResourceVersion.hxx"

class JWT;

class AuditEventCreator
{
public:
    static model::AuditEvent fromAuditData(
        const model::AuditData& auditData,
        const std::string& language,
        const AuditEventTextTemplates& textResources,
        const JWT& accessToken,
        model::ResourceVersion::DeGematikErezeptWorkflowR4 profileVersion);

};


#endif
