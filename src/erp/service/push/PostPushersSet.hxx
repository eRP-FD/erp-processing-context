/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "PushersRequestHandler.hxx"


namespace model
{
class Kvnr;
class Pusher;
class DeletePusher;
}

class PostPushersSet : public PushersRequestHandler
{
public:
    Operation getOperation() const override;

protected:
    void handlePushersRequest(SessionContext& session) override;

private:
    static void registerFdv(SessionContext& session, const model::Kvnr& kvnr, const model::Pusher& pusher);
    static void unregisterFdv(SessionContext& session, const model::Kvnr& kvnr, const model::DeletePusher& deletePusher);
};
