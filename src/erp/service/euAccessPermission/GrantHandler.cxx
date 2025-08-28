// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/service/euAccessPermission/GrantHandler.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/eu/GemErpEuPrParAccessAuthorizationRequest.hxx"
#include "erp/model/eu/GemErpEuPrParAccessAuthorizationResponse.hxx"
#include "shared/model/CountryCode.hxx"

namespace eu_access_permission
{

GrantHandler::GrantHandler(const std::initializer_list<std::string_view>& allowedProfessionOIDs)
    : ErpRequestHandler(Operation::POST_grant_eu_access_permission, allowedProfessionOIDs)
{
}

void GrantHandler::handleRequest(PcSessionContext& session)
{
    try
    {
        A_27089.start("check EU-consent for KVNR from accesstoken");
        const auto kvnrFromJwt = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
        ErpExpect(kvnrFromJwt.has_value(), HttpStatus::BadRequest, "missing idNummer claim in jwt");
        const model::Kvnr kvnr{*kvnrFromJwt};
        ErpExpect(kvnr.validFormat(), HttpStatus::BadRequest, "invalid idNummer claim in jwt");

        auto* databaseTxn = session.database();
        auto consent = databaseTxn->retrieveConsent(kvnr, model::ConsentType::EUDISPCONS);
        ErpExpect(
            consent.has_value(), HttpStatus::Forbidden,
            "Das Erstellen einer Zugriffsberechtigung ist erst zulässig, wenn eine Einwilligung durch den Nutzer zum "
            "Einlösen von E-Rezepten im europäischen Ausland erteilt wurde.");
        A_27089.finish();

        const auto authorizationRequestParams =
            parseAndValidateRequestBody<model::GemErpEuPrParAccessAuthorizationRequest>(session);

        A_27090.start("check countryCode");
        const auto countryCode = authorizationRequestParams.getCountryCode();
        ErpExpect(databaseTxn->existsCountryCode(countryCode), HttpStatus::Conflict,
                  "Für das angefragte Land ist Einlösen von E-Rezepten nicht möglich.");
        A_27090.finish();

        A_27091.start("check access code format");
        // actually the code is already checked by fhir constraint workflow-eu-access-code-1
        const auto accessCode = authorizationRequestParams.getAccessCode();
        A_27091.finish();

        A_27092.start("delete existing access permissions");
        databaseTxn->deleteEuAccessPermission(kvnr);
        A_27092.finish();

        A_27093.start("create new access permission, valid 1h");
        const model::EuAccessPermission accessPermission{accessCode, countryCode};
        databaseTxn->createEuAccessPermission(kvnr, accessPermission);
        A_27093.finish();

        A_27094.start("send response");
        const model::GemErpEuPrParAccessAuthorizationResponse response{accessPermission, kvnr};
        makeResponse(session, HttpStatus::Created, &response);
        A_27094.finish();

        // Collect Audit data
        session.auditDataCollector()
            .setEventId(model::AuditEventId::POST_GRANT_EU_ACCESS_PERMISSION)
            .setInsurantKvnr(kvnr)
            .setAction(model::AuditEvent::Action::create)
            .setCountryCode(countryCode);
    }
    catch (const model::ModelException& exc)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Parsing/validation error", exc.what());
    }
}

}
