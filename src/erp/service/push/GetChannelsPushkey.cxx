/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "erp/service/push/GetChannelsPushkey.hxx"
#include "erp/database/push/PushErpDatabase.hxx"
#include "erp/model/push/Channels.hxx"
#include "erp/model/push/ErrorResponse.hxx"
#include "erp/server/context/SessionContext.hxx"

Operation GetChannelsPushkey::getOperation() const
{
    return Operation::GET_CHANNELS_PUSHKEY;
}

void GetChannelsPushkey::handlePushersRequest(SessionContext& session)
{
    const model::Kvnr kvnr{session.kvnrFromAccessToken()};
    const auto pushkey = session.request.getPathParameter("pushkey");
    ErpExpectWithDiagnostics(pushkey.has_value(), HttpStatus::BadRequest,
                             std::string{magic_enum::enum_name(model::ErrorResponse::ErrorCode::missingParameter)},
                             "Missing pushkey path parameter");

    const auto [_, channels] = session.pushDatabase()->getChannels(kvnr, model::PushKey{*pushkey});
    ErpExpect(channels.has_value(), HttpStatus::NotFound, "push registration not found");
    makeResponse(session, HttpStatus::OK, channels->serializeToJsonString());
}
