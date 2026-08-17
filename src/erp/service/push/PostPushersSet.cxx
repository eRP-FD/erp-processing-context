/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/push/PostPushersSet.hxx"
#include "erp/database/push/PushErpDatabase.hxx"
#include "erp/model/push/ErrorResponse.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/model/push/Pusher.hxx"

void PostPushersSet::handlePushersRequest(SessionContext& session)
{
    try
    {
        const model::Kvnr kvnr{session.kvnrFromAccessToken()};
        ErpExpectWithDiagnostics(kvnr.validFormat(), HttpStatus::BadRequest,
                                 std::string{magic_enum::enum_name(model::ErrorResponse::ErrorCode::invalidParameter)},
                                 "Invalid Kvnr in ACCESS_TOKEN");
        rapidjson::Document doc;
        doc.Parse(session.request.getBody());
        ErpExpectWithDiagnostics(! doc.HasParseError() && doc.IsObject(), HttpStatus::BadRequest,
                                 std::string{magic_enum::enum_name(model::ErrorResponse::ErrorCode::invalidParameter)},
                                 "Request body is not a valid json object");
        switch (model::Pusher::parsePushKind(doc))
        {
            case model::PushKind::http:
                registerFdv(session, kvnr, model::Pusher::parse(doc));
                break;
            case model::PushKind::null:
                unregisterFdv(session, kvnr, model::DeletePusher::parse(doc));
                break;
        }
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

Operation PostPushersSet::getOperation() const
{
    return Operation::POST_PUSHERS_SET;
}

void PostPushersSet::registerFdv(SessionContext& session, const model::Kvnr& kvnr, const model::Pusher& pusher)
{
    session.auditDataCollector()
        .setAction(model::AuditEvent::Action::create)
        .setEventId(model::AuditEventId::POST_PUSHERS_SET_REGISTER)
        .setPushDeviceDisplayName(pusher.deviceDisplayName())
        .setInsurantKvnr(kvnr);
    A_27154.start("FdV-Instanz registrieren - App-Registrierung anlegen");
    A_27155.start("FdV-Instanz registrieren - App-Registrierung aktualisieren");
    A_27193.start("FdV-Instanz registrieren - Liste der channel_ids des Geräts anlegen");
    A_27193_02.start("neue Liste der channel_ids, mit den Status not_set für jede channel_id");
    // initial key derivation both on create and update
    session.pushDatabase()->createOrUpdateRegistration(kvnr, pusher);
}

void PostPushersSet::unregisterFdv(SessionContext& session, const model::Kvnr& kvnr,
                                   const model::DeletePusher& deletePusher)
{
    if (const auto registration =
            session.pushDatabase()->findRegistration(deletePusher.pushKey(), deletePusher.appId(), kvnr))
    {
        session.auditDataCollector()
            .setAction(model::AuditEvent::Action::del)
            .setEventId(model::AuditEventId::POST_PUSHERS_SET_UNREGISTER)
            .setPushDeviceDisplayName(registration->deviceDisplayName())
            .setInsurantKvnr(kvnr);
        A_27156.start("FdV-Instanz deregistrieren - App-Registrierung löschen");
        A_27197_01.start("FdV-Instanz deregistrieren - Liste der channel_ids des Geräts löschen");
        session.pushDatabase()->deleteRegistration(kvnr, deletePusher.pushKey(), deletePusher.appId());
    }
}
