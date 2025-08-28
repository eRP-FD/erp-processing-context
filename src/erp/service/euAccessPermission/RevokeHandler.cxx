// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/service/euAccessPermission/RevokeHandler.hxx"
#include "erp/model/eu/EuAccessPermission.hxx"

namespace eu_access_permission
{
RevokeHandler::RevokeHandler(const std::initializer_list<std::string_view>& allowedProfessionOIDs)
    : ErpRequestHandler(Operation::DELETE_revoke_eu_access_permission, allowedProfessionOIDs)
{
}

void RevokeHandler::handleRequest(PcSessionContext& session)
{
    A_27085.start("Zugriffsberechtigung löschen - Löschen");
    const auto kvnrFromJwt = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    ErpExpect(kvnrFromJwt.has_value(), HttpStatus::BadRequest, "missing idNummer claim in jwt");
    const model::Kvnr kvnr{*kvnrFromJwt};
    ErpExpect(kvnr.validFormat(), HttpStatus::BadRequest, "invalid idNummer claim in jwt");
    auto* databaseTxn = session.database();
    if (const auto euAccessPermission = databaseTxn->retrieveEuAccessPermission(kvnr))
    {
        const auto countryCode = euAccessPermission->getCountryCode();
        databaseTxn->deleteEuAccessPermission(kvnr);
        // Collect Audit data
        session.auditDataCollector()
            .setEventId(model::AuditEventId::DELETE_REVOKE_EU_ACCESS_PERMISSION)
            .setInsurantKvnr(kvnr)
            .setAction(model::AuditEvent::Action::del)
            .setCountryCode(countryCode);
    }
    session.response.setStatus(HttpStatus::NoContent);
    A_27085.finish();
}
}