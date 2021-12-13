/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/consent/ConsentGetHandler.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/util/TLog.hxx"


ConsentGetHandler::ConsentGetHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ErpRequestHandler(Operation::GET_Consent, allowedProfessionOiDs)
{
}

void ConsentGetHandler::handleRequest (PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    ErpFail(HttpStatus::InternalServerError, "Not yet implemented");
}

