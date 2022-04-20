/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_HEALTHHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_HEALTHHANDLER_HXX

#include "erp/pc/PcServiceContext.hxx"
#include "erp/server/handler/RequestHandlerInterface.hxx"


class HealthHandler : public UnconstrainedRequestHandler
{
public:
    void handleRequest(SessionContext& session) override;
    Operation getOperation(void) const override;
};


#endif
