/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "CommunicationDeleteHandler.hxx"

#include "erp/database/Database.hxx"
#include "erp/model/Communication.hxx"
#include "erp/model/Task.hxx"
#include "erp/server/context/ServiceContext.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "erp/ErpRequirements.hxx"

#include "erp/common/MimeType.hxx"


using namespace model;


CommunicationDeleteHandler::CommunicationDeleteHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ErpRequestHandler(Operation::DELETE_Communication_id, allowedProfessionOiDs)
{
}

void CommunicationDeleteHandler::handleRequest (PcSessionContext& session)
{
    Header header = session.request.header();

    TVLOG(1) << name() << ": processing request to " << header.target();

    A_20258.start("E-prescription service - delete communication based on sender ID");
    A_20260.start("Use case delete message by insured person");
    A_20776.start("E-prescription service - delete message by deliverer");

    std::optional<std::string> sender = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    ErpExpect(sender.has_value(), HttpStatus::BadRequest, "JWT does not contain a claim for the sender");

    std::optional<std::string> communicationIdString = session.request.getPathParameter("id");

    ErpExpect(communicationIdString.has_value(), HttpStatus::BadRequest, "Missing id of communication to be deleted");

    Uuid communicationId(communicationIdString.value());
    ErpExpect(communicationId.isValidIheUuid(), HttpStatus::BadRequest, "Invalid format of communication id");

    auto databaseHandle = session.database();

    auto [id, received] = databaseHandle->deleteCommunication(communicationId, sender.value());

    if (id.has_value())
    {
        A_20259.start("E-prescription service - delete communication with warning if the recipient has already accessed it");
        if (received.has_value())
        {
            session.response.setHeader(Header::Warning, "Deleted message delivered at " + received.value().toXsDateTime());
        }
        A_20259.finish();
    }
    else if (databaseHandle->existCommunication(communicationId))
    {
        ErpFail(HttpStatus::Unauthorized,
            sender.value() + " is not allowed to delete communication for id " + communicationId.toString());
    }
    else
    {
        ErpFail(HttpStatus::NotFound, "No communication found for id " + communicationId.toString());
    }

    A_20258.finish();
    A_20260.finish();
    A_20776.finish();

    session.response.setStatus(HttpStatus::NoContent);
}
