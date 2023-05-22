/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/chargeitem/ChargeItemDeleteHandler.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/util/TLog.hxx"
#include "erp/server/response/ServerResponse.hxx"


ChargeItemDeleteHandler::ChargeItemDeleteHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ChargeItemHandlerBase(Operation::DELETE_ChargeItem_id, allowedProfessionOiDs)
{
}

// GEMREQ-start A_22114
// GEMREQ-start A_22117-01
void ChargeItemDeleteHandler::handleRequest (PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    const auto prescriptionID = parseIdFromPath(session.request, session.accessLog);

    auto* databaseHandle = session.database();
    const auto [chargeInformation, blobId, salt] = databaseHandle->retrieveChargeInformationForUpdate(prescriptionID);

    A_22114.start("Assure identical kvnr of access token and ChargeItem resource");
    const auto idNumberClaim = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    Expect3(idNumberClaim.has_value(), "JWT does not contain idNumberClaim",
            std::logic_error); // should not happen because of JWT verification;
    Expect(chargeInformation.chargeItem.subjectKvnr().has_value(), "Retrieved ChargeItem has no kvnr");
    ErpExpect(chargeInformation.chargeItem.subjectKvnr().value() == idNumberClaim.value(), HttpStatus::Forbidden,
              "Kvnr in ChargeItem does not match the one from the access token");
    A_22114.finish();
// GEMREQ-end A_22114

    A_22117_01.start("Delete charging information and related communication resources");
    databaseHandle->deleteChargeInformation(prescriptionID);
    databaseHandle->deleteCommunicationsForChargeItem(prescriptionID);
    A_22117_01.finish();
// GEMREQ-end A_22117-01

    session.response.setStatus(HttpStatus::NoContent);

    // Collect Audit data
    session.auditDataCollector()
        .setPrescriptionId(prescriptionID)
        .setEventId(model::AuditEventId::DELETE_ChargeItem_id_insurant)
        .setInsurantKvnr(chargeInformation.chargeItem.subjectKvnr().value())
        .setAction(model::AuditEvent::Action::del);
}
