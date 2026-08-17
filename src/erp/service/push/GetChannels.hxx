/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "erp/service/push/PushersRequestHandler.hxx"


class GetChannels : public PushersRequestHandler
{
public:
    Operation getOperation() const override;

protected:
    void handlePushersRequest(SessionContext& session) override;
};
