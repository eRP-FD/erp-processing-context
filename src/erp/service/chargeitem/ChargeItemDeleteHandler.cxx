/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/chargeitem/ChargeItemDeleteHandler.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/util/TLog.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/server/response/ServerResponse.hxx"


ChargeItemDeleteHandler::ChargeItemDeleteHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ErpRequestHandler(Operation::DELETE_ChargeItem_id, allowedProfessionOiDs)
{
}

void ChargeItemDeleteHandler::handleRequest (PcSessionContext& session)
{
    std::optional<model::PrescriptionId> prescriptionID;
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    A_22112.start("Deletion of chargeItem only by specific id");
    const auto idString = session.request.getPathParameter("id");
    ErpExpect(idString.has_value(), HttpStatus::Forbidden, "Id of chargeItem to be deleted must be specified");
    A_22112.finish();

    try
    {
        prescriptionID.emplace(model::PrescriptionId::fromString(idString.value()));
    }
    catch(model::ModelException& exc)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Invalid format of ChargeItem id", exc.what());
    }

    const auto [chargeInformation, blobId, salt] = session.database()->retrieveChargeInformationForUpdate(prescriptionID.value());

    const auto idNumberClaim = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    Expect(idNumberClaim.has_value(), "JWT does not contain idNumberClaim");

    const auto professionOIDClaim = session.request.getAccessToken().stringForClaim(JWT::professionOIDClaim);
    Expect(professionOIDClaim.has_value(), "JWT does not contain professionOIDClaim");

    A_22114.start("Assure identical kvnr of access token and ChargeItem resource");
    ErpExpect(chargeInformation.chargeItem.subjectKvnr() == idNumberClaim.value(), HttpStatus::Forbidden,
              "Kvnr in ChargeItem does not match the one from the access token");
    A_22114.finish();

    A_22117_01.start("Delete ChargeItem");
    session.database()->deleteChargeInformation(prescriptionID.value());
    A_22117_01.finish();

    session.response.setStatus(HttpStatus::NoContent);

    // Collect Audit data
    session.auditDataCollector()
        .setPrescriptionId(prescriptionID.value())
        .setEventId(model::AuditEventId::DELETE_ChargeItem_id_insurant)
        .setInsurantKvnr(chargeInformation.chargeItem.subjectKvnr().value())
        .setAction(model::AuditEvent::Action::del);
}
