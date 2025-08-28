/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/consent/ConsentPostHandler.hxx"
#include "erp/model/Consent.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "shared/util/TLog.hxx"

#include <pqxx/except>


ConsentPostHandler::ConsentPostHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ErpRequestHandler(Operation::POST_Consent, allowedProfessionOiDs)
{
}

void ConsentPostHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();
    TVLOG(2) << name() << ": request body: " << session.request.getBody();

    A_22351.start("FHIR validation of input Consent resource");
    auto consent = parseAndValidateRequestBody<model::Consent>(
        session, {model::ProfileType::GEM_ERPCHRG_PR_Consent, model::ProfileType::GEM_ERPEU_PR_Consent});
    A_22351.finish();

    auto category = checkConsentType(consent);

    A_22289.start("Assure identical kvnr of access token and Consent patient identifier");
    const auto kvnrClaim = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    Expect(kvnrClaim.has_value(), "JWT does not contain Kvnr");
    const model::Kvnr kvnr{*kvnrClaim, model::Kvnr::Type::unspecified};
    try
    {
        ErpExpect(consent.patientKvnr() == kvnr, HttpStatus::Forbidden,
                  "Kvnr mismatch between access token and Consent resource patient identifier");
    }
    catch (const model::ModelException& exc)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Missing patient identifier in Consent resource", exc.what());
    }
    A_22289.finish();

    A_22162_01.start("Only single consent for the same Kvnr");
    A_22350.start("Persist consent");
    try
    {
        session.database()->storeConsent(consent);
    }
    catch (const pqxx::unique_violation&)
    {
        ErpFail(HttpStatus::Conflict,
                std::string{magic_enum::enum_name(category)} + " consent already exists for this kvnr");
    }
    A_22162_01.finish();
    A_22350.finish();

    consent.fillId(category, kvnr);
    session.response.setHeader(Header::Location, getLinkBase() + "/Consent/" + std::string(consent.id().value()));

    makeResponse(session, HttpStatus::Created, &consent);

    // Collect Audit data
    const auto getAuditEventId = [](model::ConsentType category) {
        switch (category)
        {
            case model::ConsentType::EUDISPCONS:
                return model::AuditEventId::POST_EU_Consent;
            case model::ConsentType::CHARGCONS:
                break;
        }
        return model::AuditEventId::POST_Consent;
    };

    session.auditDataCollector()
        .setEventId(getAuditEventId(category))
        .setInsurantKvnr(kvnr)
        .setAction(model::AuditEvent::Action::create)
        .setConsentId(consent.id().value_or(""));
}

model::ConsentType ConsentPostHandler::checkConsentType(const model::Consent& consent)
{
    try
    {
        auto category = consent.consentCategory();
        switch (category)
        {
            case model::ConsentType::CHARGCONS:
                break;
            case model::ConsentType::EUDISPCONS:
                ErpExpect(Configuration::instance().getBoolValue(ConfigurationKey::FEATURE_EU),
                          HttpStatus::MethodNotAllowed, "EU feature is disabled in configuration ERP_FEATURE_EU");
                break;
        }
        return category;
    }
    catch (const model::ModelException& exc)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Invalid consent category", exc.what());
    }
}
