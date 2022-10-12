/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/consent/ConsentDeleteHandler.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/model/Consent.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/util/TLog.hxx"


ConsentDeleteHandler::ConsentDeleteHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ErpRequestHandler(Operation::DELETE_Consent, allowedProfessionOiDs)
{
}

void ConsentDeleteHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    A_22154.start("Category must be specified");
    const auto category = session.request.getQueryParameter("category");
    ErpExpect(category.has_value(), HttpStatus::MethodNotAllowed, "Category must be specified");
    A_22154.finish();

    A_22874.start("Category must have value 'CHARGCONS'");
    ErpExpect(*category == "CHARGCONS", HttpStatus::BadRequest, "Category must be 'CHARGCONS'");
    A_22874.finish();

    const auto kvnr = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    Expect(kvnr.has_value(), "ACCESS_TOKEN does not contain KVNR");

    A_22158.start("Delete the consent matched by the KVNR from the ACCESS_TOKEN");
    ErpExpect(session.database()->clearConsent(*kvnr),
              HttpStatus::NotFound,
              "Could not find any consent from given KVNR ");
    A_22158.finish();

    A_22157.start("Delete all charge information for insurant matched by the KVNR from the ACCESS_TOKEN");
    session.database()->clearAllChargeInformation(*kvnr);
    A_22157.finish();

    A_22117_01.start("Delete all charge information & communications for insurant matched by the KVNR from the ACCESS_TOKEN");
    session.database()->clearAllChargeInformation(*kvnr);
    session.database()->clearAllChargeItemCommunications(*kvnr);
    A_22117_01.finish();

    // Collect Audit data
    session.auditDataCollector()
        .setEventId(model::AuditEventId::DELETE_Consent)
        .setInsurantKvnr(*kvnr)
        .setAction(model::AuditEvent::Action::del);

    session.response.setStatus(HttpStatus::NoContent);
}
