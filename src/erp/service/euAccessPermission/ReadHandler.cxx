// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/service/euAccessPermission/ReadHandler.hxx"
#include "erp/model/eu/EuAccessPermission.hxx"
#include "erp/model/eu/GemErpEuPrParAccessAuthorizationResponse.hxx"


namespace eu_access_permission
{

ReadHandler::ReadHandler(const std::initializer_list<std::string_view>& allowedProfessionOIDs)
    : ErpRequestHandler(Operation::GET_read_eu_access_permission, allowedProfessionOIDs)
{
}

void ReadHandler::handleRequest(PcSessionContext& session)
{
    const auto kvnrFromJwt = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    ErpExpect(kvnrFromJwt.has_value(), HttpStatus::BadRequest, "missing idNummer claim in jwt");
    const model::Kvnr kvnr{*kvnrFromJwt};
    ErpExpect(kvnr.validFormat(), HttpStatus::BadRequest, "invalid idNummer claim in jwt");
    auto* databaseTxn = session.database();
    A_27087.start("Rezept-Fachdienst - Zugriffsberechtigung lesen - Response");
    auto euAccessPermission = databaseTxn->retrieveEuAccessPermission(kvnr);
    ErpExpect(euAccessPermission.has_value(), HttpStatus::NotFound, "Zugriffsberechtigung nicht gefunden");
    ErpExpect(euAccessPermission->isValid(model::Timestamp::now()), HttpStatus::NotFound,
              "Zugriffsberechtigung nicht mehr gültig.");
    const model::GemErpEuPrParAccessAuthorizationResponse euAccessPermissionResponse{*euAccessPermission, kvnr};
    makeResponse(session, HttpStatus::OK, &euAccessPermissionResponse);
    A_27087.finish();

    // No AuditEvent for Read
}
}