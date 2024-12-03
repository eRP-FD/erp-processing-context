/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/chargeitem/ChargeItemPatchHandler.hxx"
#include "shared/ErpRequirements.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/WorkflowParameters.hxx"

#include <string_view>


ChargeItemPatchHandler::ChargeItemPatchHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ChargeItemHandlerBase(Operation::PATCH_ChargeItem_id, allowedProfessionOiDs)
{

}


// GEMREQ-start A_22877
void ChargeItemPatchHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    const auto idClaim = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    Expect3(idClaim.has_value(), "JWT does not contain Id",
            std::logic_error);// should not happen because of JWT verification;

    const auto prescriptionId = parseIdFromPath(session.request, session.accessLog);
    auto [existingChargeInformation, blobId, salt] =
        session.database()->retrieveChargeInformationForUpdate(prescriptionId);

    A_22877.start("Only the beneficiary insured person as the authorized person changes the billing information");
    Expect(existingChargeInformation.chargeItem.subjectKvnr().has_value(), "Retrieved ChargeItem has no kvnr");
    ErpExpect(existingChargeInformation.chargeItem.subjectKvnr().value() == idClaim.value(),
              HttpStatus::Forbidden,
              "Kvnr in ChargeItem does not match the one from the access token");
    A_22877.finish();
// GEMREQ-end A_22877

// GEMREQ-start A_22878
    A_22878.start("Validate 'Parameters' resource against generic FHIR profile and check validity of content");
    const auto parametersResource = parseAndValidateRequestBody<model::PatchChargeItemParameters>(session);
    std::optional<const model::ChargeItemMarkingFlags> markingFlags;
    try
    {
        markingFlags.emplace(parametersResource.getChargeItemMarkingFlags());
    }
    catch(const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Invalid 'Parameters' resource provided", ex.what());
    }
    A_22878.finish();

    existingChargeInformation.chargeItem.setMarkingFlags(markingFlags.value());
// GEMREQ-end A_22878

    session.database()->updateChargeInformation(existingChargeInformation, blobId, salt);

    makeResponse(session, HttpStatus::OK, &existingChargeInformation.chargeItem);

    // Collect Audit data
    session.auditDataCollector()
        .setPrescriptionId(prescriptionId)
        .setEventId(model::AuditEventId::PATCH_ChargeItem_id)
        .setInsurantKvnr(model::Kvnr{*idClaim, model::Kvnr::Type::pkv})
        .setAction(model::AuditEvent::Action::update);
}
