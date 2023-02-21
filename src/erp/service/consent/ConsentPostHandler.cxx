/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/consent/ConsentPostHandler.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/model/Consent.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/util/TLog.hxx"

#include <pqxx/except>


ConsentPostHandler::ConsentPostHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ErpRequestHandler(Operation::POST_Consent, allowedProfessionOiDs)
{
}

void ConsentPostHandler::handleRequest (PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    A_22351.start("FHIR validation of input Consent resource");
    auto consent = parseAndValidateRequestBody<model::Consent>(session, SchemaType::fhir, std::nullopt);
    A_22351.finish();
    ErpExpect(consent.isChargingConsent(), HttpStatus::BadRequest, "Invalid consent type");

    A_22289.start("Assure identical kvnr of access token and Consent patient identifier");
    const auto kvnrClaim = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    Expect(kvnrClaim.has_value(), "JWT does not contain Kvnr");
    const model::Kvnr kvnr{*kvnrClaim, model::Kvnr::Type::pkv};
    try
    {
        ErpExpect(consent.patientKvnr() == kvnr, HttpStatus::Forbidden,
                  "Kvnr mismatch between access token and Consent resource patient identifier");
    }
    catch(model::ModelException& exc)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Missing patient identifier in Consent resource", exc.what());
    }
    A_22289.finish();

    A_22162.start("Only single consent for the same Kvnr");
    A_22350.start("Persist consent");
    try
    {
        session.database()->storeConsent(consent);
    }
    catch(const pqxx::unique_violation& exc)
    {
        ErpFail(HttpStatus::Conflict, "Charging consent already exists for this kvnr");
    }
    A_22162.finish();
    A_22350.finish();

    consent.fillId();
    session.response.setHeader(Header::Location, getLinkBase() + "/Consent/" + std::string(consent.id().value()));

    makeResponse(session, HttpStatus::Created, &consent);

    // Collect Audit data
    session.auditDataCollector()
        .setEventId(model::AuditEventId::POST_Consent)
        .setInsurantKvnr(kvnr)
        .setAction(model::AuditEvent::Action::create)
        .setConsentId(consent.id().value_or(""));
}
