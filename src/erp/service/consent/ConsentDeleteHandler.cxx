/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/consent/ConsentDeleteHandler.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/util/TLog.hxx"


ConsentDeleteHandler::ConsentDeleteHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ErpRequestHandler(Operation::DELETE_Consent_id, allowedProfessionOiDs)
{
}

void ConsentDeleteHandler::handleRequest (PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    ErpFail(HttpStatus::InternalServerError, "Not yet implemented");
}

