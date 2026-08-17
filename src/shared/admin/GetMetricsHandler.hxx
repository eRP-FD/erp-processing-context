// (C) Copyright IBM Deutschland GmbH 2021, 2026
// (C) Copyright IBM Corp. 2021, 2026
// non-exclusively licensed to gematik GmbH

#pragma once
#include "shared/admin/AdminRequestHandler.hxx"

class GetMetricsHandler : public UnconstrainedRequestHandler
{
public:
    Operation getOperation() const override;
    void handleRequest(BaseSessionContext& session) override;
};
