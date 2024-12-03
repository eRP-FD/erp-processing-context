/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_HEALTHHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_HEALTHHANDLER_HXX

#include "erp/server/RequestHandler.hxx"


class HealthHandler : public UnconstrainedRequestHandler
{
public:
    void handleRequest(SessionContext& session) override;
    Operation getOperation(void) const override;
};


#endif
