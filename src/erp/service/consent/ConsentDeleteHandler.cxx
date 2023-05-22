/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
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

// GEMREQ-start A_22157
// GEMREQ-start A_22158
void ConsentDeleteHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    A_22154.start("Category must be specified");
    const auto category = session.request.getQueryParameter("category");
    ErpExpect(category.has_value(), HttpStatus::MethodNotAllowed, "Category must be specified");
    A_22154.finish();

    A_22874_01.start("Category must have value 'CHARGCONS'");
    ErpExpect(*category == model::Consent::chargingConsentType, HttpStatus::BadRequest,
              "Category must be '" + std::string(model::Consent::chargingConsentType) + "'");
    A_22874_01.finish();

    const auto kvnrClaim = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    Expect(kvnrClaim.has_value(), "ACCESS_TOKEN does not contain KVNR");
    const model::Kvnr kvnr{*kvnrClaim, model::Kvnr::Type::pkv};

    auto* databaseHandle = session.database();

    A_22158.start("Delete the consent matched by the KVNR from the ACCESS_TOKEN");
    ErpExpect(databaseHandle->clearConsent(kvnr),
              HttpStatus::NotFound,
              "Could not find any consent for given KVNR ");
    A_22158.finish();
// GEMREQ-end A_22158

    A_22157.start("Delete all charge information and related communication for insurant matched by the KVNR from the ACCESS_TOKEN");
    databaseHandle->clearAllChargeInformation(kvnr);
    databaseHandle->clearAllChargeItemCommunications(kvnr);
    A_22157.finish();
// GEMREQ-end A_22157

    // Collect Audit data
    session.auditDataCollector()
        .setEventId(model::AuditEventId::DELETE_Consent)
        .setInsurantKvnr(kvnr)
        .setAction(model::AuditEvent::Action::del);

    session.response.setStatus(HttpStatus::NoContent);
}
