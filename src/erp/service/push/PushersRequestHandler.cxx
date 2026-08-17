/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/push/PushersRequestHandler.hxx"
#include "erp/model/push/ErrorResponse.hxx"
#include "erp/server/context/SessionContext.hxx"
#include "erp/service/ErpRequestHandler.hxx"
#include "shared/ErpRequirements.hxx"

void PushersRequestHandler::preHandleRequestHook(BaseSessionContext& baseSessionContext)
{
    ErpExpect(! baseSessionContext.request.header().header(Header::Accept) ||
              baseSessionContext.request.header().getAcceptMimeType(MimeType::json.getMimeType()).has_value(),
          HttpStatus::NotAcceptable, "Angefragter Mime-Type im Accept-Header kann nicht bedient werden");
}

void PushersRequestHandler::handleRequest(BaseSessionContext& session)
{
    TVLOG(1) << "processing request to " << session.request.header().target();
    TVLOG(2) << "request body: " << session.request.getBody();
    handlePushersRequest(dynamic_cast<SessionContext&>(session));
}

// GEMREQ-start A_28112
bool PushersRequestHandler::allowedForProfessionOID(std::string_view professionOid,
                                                    const std::optional<std::string>&) const
{
    A_28112.start("App-Registrierung - Rolle Versicherter");
    A_28113.start("App-Registrierungen Abrufen - Rolle Versicherter");
    A_28118.start("Channels abrufen - Rolle Versicherter");
    A_28119.start("Channels des Geräts abrufen - Rolle Versicherter");
    A_28120.start("Channels konfigurieren - Rolle Versicherter");
    return professionOid == profession_oid::oid_versicherter;
}
// GEMREQ-end A_28112

void PushersRequestHandler::makeResponse(BaseSessionContext& session, HttpStatus status, const std::string& body)
{
    session.response.setStatus(status);
    if (! body.empty())
    {
        session.response.setBody(body);
        session.response.setHeader(Header::ContentType, ContentMimeType::jsonUtf8);
    }
}