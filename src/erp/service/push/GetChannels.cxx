/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/push/GetChannels.hxx"
#include "erp/model/push/Channels.hxx"
#include "erp/model/push/ErrorResponse.hxx"
#include "erp/server/context/SessionContext.hxx"

Operation GetChannels::getOperation() const
{
    return Operation::GET_CHANNELS;
}

void GetChannels::handlePushersRequest(SessionContext& session)
{
    makeResponse(session, HttpStatus::OK, model::Channels{}.serializeToJsonString());
}
