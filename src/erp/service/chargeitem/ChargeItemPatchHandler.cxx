/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/ErpRequirements.hxx"
#include "erp/service/chargeitem/ChargeItemPatchHandler.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Parameters.hxx"

#include <string_view>


namespace
{
    constexpr auto* idParameterName = "id";
}


ChargeItemPatchHandler::ChargeItemPatchHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ChargeItemBodyHandlerBase(Operation::PATCH_ChargeItem_id, allowedProfessionOiDs)
{

}


void ChargeItemPatchHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    const auto idParameter = session.request.getPathParameter(idParameterName);
    Expect3(idParameter.has_value(), "ID not provided", std::logic_error);

    const auto idClaim = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    Expect3(idClaim.has_value(), "JWT does not contain Id",
            std::logic_error);// should not happen because of JWT verification;

    auto* databaseHandle = session.database();
    const auto prescriptionId = parseIdFromPath(session.request, session.accessLog);
    auto existingChargeInformation = databaseHandle->retrieveChargeInformationForUpdate(prescriptionId);

    A_22877.start("Only the beneficiary insured person as the authorized person changes the billing information");
    ErpExpect(existingChargeInformation.chargeItem.subjectKvnr().value() == idClaim.value(),
              HttpStatus::Forbidden,
              "Kvnr in ChargeItem does not match the one from the access token");
    A_22877.finish();

    const auto parametersResource = parseAndValidateRequestBody<model::Parameters>(session, SchemaType::fhir);
    const auto markingFlag = parametersResource.getChargeItemMarkingFlag();
    ErpExpect(markingFlag.has_value(),
              HttpStatus::BadRequest,
              "Could not get charge item marking flag from resource");

    existingChargeInformation.chargeItem.setMarkingFlag(*markingFlag);
    session.database()->updateChargeInformation(existingChargeInformation);

    makeResponse(session, HttpStatus::OK, &existingChargeInformation.chargeItem);

    // Collect Audit data
    session.auditDataCollector()
        .setPrescriptionId(prescriptionId)
        .setEventId(model::AuditEventId::PATCH_ChargeItem_id)
        .setInsurantKvnr(idClaim.value())
        .setAction(model::AuditEvent::Action::update);
}
