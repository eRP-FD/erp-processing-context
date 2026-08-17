/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/push/PostChannels.hxx"
#include "erp/database/push/PushErpDatabase.hxx"
#include "erp/model/push/Channels.hxx"
#include "erp/model/push/ErrorResponse.hxx"
#include "erp/server/context/SessionContext.hxx"

Operation PostChannels::getOperation() const
{
    return Operation::POST_CHANNELS;
}

void PostChannels::handlePushersRequest(SessionContext& session)
{
    try
    {
        const model::Kvnr kvnr{session.kvnrFromAccessToken()};
        const auto pushkey = session.request.getPathParameter("pushkey");
        ErpExpectWithDiagnostics(pushkey.has_value(), HttpStatus::BadRequest,
                                 std::string{magic_enum::enum_name(model::ErrorResponse::ErrorCode::missingParameter)},
                                 "Missing pushkey path parameter");
        ErpExpectWithDiagnostics(! session.request.getBody().empty(), HttpStatus::BadRequest,
                                 std::string{magic_enum::enum_name(model::ErrorResponse::ErrorCode::missingParameter)},
                                 "Missing request body");

        auto [appIdHashedBytes, channels] = session.pushDatabase()->getChannels(kvnr, model::PushKey{*pushkey});
        ErpExpect(channels.has_value(), HttpStatus::NotFound, "Cannot update channels: push registration not found");
        channels->processUpdate(session.request.getBody());

        db_model::HashedId appIdHashed{std::move(appIdHashedBytes)};

        session.pushDatabase()->updateChannels(kvnr, model::PushKey{*pushkey}, appIdHashed, *channels);
        makeResponse(session, HttpStatus::OK, "{}");
    }
    catch (const model::MissingParameterException& mp)
    {
        session.accessLog.locationFromException(mp);
        makeResponse(
            session, HttpStatus::BadRequest,
            model::ErrorResponse{model::ErrorResponse::ErrorCode::missingParameter, mp.what()}.serializeToJsonString());
    }
    catch (const model::InvalidParameterException& ip)
    {
        session.accessLog.locationFromException(ip);
        makeResponse(
            session, HttpStatus::BadRequest,
            model::ErrorResponse{model::ErrorResponse::ErrorCode::invalidParameter, ip.what()}.serializeToJsonString());
    }
    catch (const model::ModelException& re)
    {
        session.accessLog.locationFromException(re);
        makeResponse(
            session, HttpStatus::BadRequest,
            model::ErrorResponse{model::ErrorResponse::ErrorCode::malformedRequest, re.what()}.serializeToJsonString());
    }
}
