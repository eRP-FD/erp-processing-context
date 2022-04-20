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
    : ErpRequestHandler(Operation::DELETE_Consent_id, allowedProfessionOiDs)
{
}

void ConsentDeleteHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    const auto kvnrClaim = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    Expect(kvnrClaim.has_value(), "JWT does not contain Kvnr");

    A_22154.start("Deletion of Consent only by specific id");
    const auto consentIdString = session.request.getPathParameter("id");
    ErpExpect(consentIdString.has_value(), HttpStatus::Forbidden, "Id of Consent to be deleted must be specified");
    A_22154.finish();

    try
    {
        A_22156.start("Assure identical kvnr of access token and id");
        const auto [consentType, consentIdKvnr] = model::Consent::splitIdString(consentIdString.value());
        ErpExpect(consentIdKvnr == kvnrClaim.value(), HttpStatus::Forbidden,
                  "Kvnr mismatch between access token and Consent id");
        A_22156.finish();
    }
    catch(model::ModelException& exc)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Invalid format of Consent id", exc.what());
    }

    A_22157.start("Delete all charging data for insurant");
    session.database()->clearAllChargeInformation(kvnrClaim.value());
    A_22157.finish();

    A_22158.start("Delete Consent");
    ErpExpect(session.database()->clearConsent(kvnrClaim.value()), HttpStatus::NotFound,
              "No Consent found for id " + consentIdString.has_value());
    A_22158.finish();

    session.response.setStatus(HttpStatus::NoContent);

    // Collect Audit data
    session.auditDataCollector()
        .setEventId(model::AuditEventId::DELETE_Consent_id)
        .setInsurantKvnr(*kvnrClaim)
        .setAction(model::AuditEvent::Action::del);
}

