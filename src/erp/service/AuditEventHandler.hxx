/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVICE_AUDITEVENTHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_SERVICE_AUDITEVENTHANDLER_HXX

#include "erp/service/ErpRequestHandler.hxx"
#include "shared/model/AuditData.hxx"


class GetAllAuditEventsHandler: public ErpRequestHandler
{
public:
    GetAllAuditEventsHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs);
    void handleRequest (PcSessionContext& session) override;

private:
    model::Bundle createBundle (
        const PcServiceContext& serviceContext,
        const ServerRequest& request,
        const std::vector<model::AuditData>& auditData) const;

};


class GetAuditEventHandler: public ErpRequestHandler
{
public:
    GetAuditEventHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs);
    void handleRequest (PcSessionContext& session) override;
};



#endif
