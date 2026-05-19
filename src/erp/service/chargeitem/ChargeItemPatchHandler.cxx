/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/ChargeItem.hxx"
#include "erp/model/GemErpChrgPrParPatchChargeItemInput.hxx"
#include "erp/model/WorkflowParameters.hxx"
#include "erp/service/chargeitem/ChargeItemPatchHandler.hxx"
#include "shared/ErpRequirements.hxx"

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

    const auto markingExtension = getMarkingExtension(session);

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

    existingChargeInformation.chargeItem.setMarkingFlags(markingExtension);

    session.database()->updateChargeInformation(existingChargeInformation, blobId, salt);

    makeResponse(session, HttpStatus::OK, &existingChargeInformation.chargeItem);

    // Collect Audit data
    session.auditDataCollector()
        .setPrescriptionId(prescriptionId)
        .setEventId(model::AuditEventId::PATCH_ChargeItem_id)
        .setInsurantKvnr(model::Kvnr{*idClaim})
        .setAction(model::AuditEvent::Action::update);
}

model::ChargeItemMarkingFlags ChargeItemPatchHandler::getMarkingExtension(PcSessionContext& session)
{
    TVLOG(1) << "Detecting input type..";
    auto unspec = createResourceFactory<model::UnspecifiedResource>(session);
    auto profileName = unspec.getProfileName();
    // GEMREQ-start A_22878
    A_22878.start("Validate 'Parameters' resource against generic FHIR profile and check validity of content");
    try
    {
        if (profileName && unspec.getProfile() == model::ProfileType::GEM_ERPCHRG_PR_PAR_Patch_ChargeItem_Input)
        {
            TVLOG(1) << "Detected GEM_ERPCHRG_PR_PAR_Patch_ChargeItem_Input";
            const auto parametersResource =
                parseAndValidateRequestBody<model::GemErpChrgPrParPatchChargeItemInput>(session);
            return parametersResource.createMarkingExtension();
        }
        TVLOG(1) << "Fallback to legacy parameters";
        const auto parametersResource = parseAndValidateRequestBody<model::PatchChargeItemParameters>(session);
        return parametersResource.getChargeItemMarkingFlags();
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Invalid 'Parameters' resource provided", ex.what());
    }
    A_22878.finish();
    // GEMREQ-end A_22878
    return {};
}
