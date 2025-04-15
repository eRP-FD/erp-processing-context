/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_HEALTHHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_HEALTHHANDLER_HXX

#include "erp/server/RequestHandler.hxx"


class HealthHandler : public UnconstrainedRequestHandler
{
public:
    void handleRequest(BaseSessionContext& session) override;
    Operation getOperation() const override;
    ~HealthHandler() override = default;
};


#endif
