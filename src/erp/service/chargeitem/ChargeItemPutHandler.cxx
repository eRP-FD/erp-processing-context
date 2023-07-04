/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/chargeitem/ChargeItemPutHandler.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/util/TLog.hxx"

namespace
{

void checkChargeItemConsistency(
    const model::ChargeItem& existingChargeItem,
    const model::ChargeItem& newChargeItem)
{
    try
    {
        const auto id = newChargeItem.id();
        const auto prescriptionId = newChargeItem.prescriptionId();
        const auto entererTelematikId = newChargeItem.entererTelematikId();
        const auto subjectKvnr = newChargeItem.subjectKvnr();
        const auto enteredDate = newChargeItem.enteredDate();
        const auto receiptRef = newChargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::receiptBundle);
        const auto prescriptionRef = newChargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItemBundle);

        const auto newMarkingFlags = newChargeItem.markingFlags();
        const auto newMarkings = newMarkingFlags.has_value() ? newMarkingFlags->getAllMarkings()
                                                             : model::ChargeItemMarkingFlags::MarkingContainer();

        try
        {
            ErpExpect(id == existingChargeItem.id(), HttpStatus::BadRequest,
                      "Id of provided ChargeItem does not match the one from the ChargeItem to update");
            ErpExpect(prescriptionId == existingChargeItem.prescriptionId(), HttpStatus::BadRequest,
                      "Identifier of provided ChargeItem does not match the one from the ChargeItem to update");
            ErpExpect(entererTelematikId == existingChargeItem.entererTelematikId(), HttpStatus::BadRequest,
                      "TelematikId in provided ChargeItem does not match the one from the ChargeItem to update");
            ErpExpect(subjectKvnr == existingChargeItem.subjectKvnr(), HttpStatus::BadRequest,
                      "Kvnr in provided ChargeItem does not match the one from the ChargeItem to update");
            ErpExpect(enteredDate == existingChargeItem.enteredDate(), HttpStatus::BadRequest,
                      "Entered date in provided ChargeItem does not match the one from the ChargeItem to update");
            const auto existingReceiptBundleRef =
                existingChargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::receiptBundle);
            ErpExpect(receiptRef == existingReceiptBundleRef ||
                          (existingReceiptBundleRef.has_value() &&
                           ("Bundle/" + std::string{receiptRef->data()}) == existingReceiptBundleRef),
                      HttpStatus::BadRequest,
                      "Receipt reference in provided ChargeItem does not match the one from the ChargeItem to update");
            auto existingPrescriptionBundleRef = existingChargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItemBundle);
            ErpExpect(
                prescriptionRef == existingPrescriptionBundleRef ||
                    (existingPrescriptionBundleRef.has_value() &&
                     ("Bundle/" + std::string{prescriptionRef->data()}) == existingPrescriptionBundleRef),
                HttpStatus::BadRequest,
                "Prescription reference in provided ChargeItem does not match the one from the ChargeItem to update");

            const auto existingMarkingFlags = existingChargeItem.markingFlags();
            const auto existingMarkings = existingMarkingFlags.has_value()
                                              ? existingMarkingFlags->getAllMarkings()
                                              : model::ChargeItemMarkingFlags::MarkingContainer();
            ErpExpect(existingMarkings == newMarkings, HttpStatus::BadRequest,
                      "Markings in provided ChargeItem do not match the ones from the ChargeItem to update");
        }
        catch (const model::ModelException &exc)
        {
            Fail2("ChargeItem which shall be updated is invalid", std::logic_error);
        }
    }
    catch (const model::ModelException &exc)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Invalid charge item provided", exc.what());
    }
}

} // anonymous namespace


ChargeItemPutHandler::ChargeItemPutHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ChargeItemBodyHandlerBase(Operation::PUT_ChargeItem_id, allowedProfessionOiDs)
{
}


// GEMREQ-start A_22146
// GEMREQ-start A_22215
void ChargeItemPutHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    const auto prescriptionId = parseIdFromPath(session.request, session.accessLog);

    A_22152.start("Validate input ChargeItem");
    const auto newChargeItem = parseAndValidateRequestBody<model::ChargeItem>(session, SchemaType::Gem_erxChargeItem);
    A_22152.finish();

    auto* databaseHandle = session.database();
    auto [existingChargeInformation, blobId, salt] = databaseHandle->retrieveChargeInformationForUpdate(prescriptionId);

    A_22146.start("Pharmacy: validation of TelematikId");
    const auto idClaim = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    Expect3(idClaim.has_value(), "JWT does not contain Id",
            std::logic_error);// should not happen because of JWT verification;
    Expect(existingChargeInformation.chargeItem.entererTelematikId().has_value(), "Retrieved ChargeItem has no TelematikId");
    ErpExpect(existingChargeInformation.chargeItem.entererTelematikId().value() == idClaim, HttpStatus::Forbidden,
              "TelematikId in ChargeItem does not match the one from the access token");
    A_22146.finish();
// GEMREQ-end A_22146

    A_22215.start("Check consent");
    ErpExpect(newChargeItem.subjectKvnr(), ::HttpStatus::BadRequest, "KVNR is missing");
    const auto consent = databaseHandle->retrieveConsent(newChargeItem.subjectKvnr().value());
    ErpExpect(
        consent.has_value(),
        HttpStatus::Forbidden,
        "Die versicherte Person hat keine Einwilligung im E-Rezept Fachdienst hinterlegt. Abrechnungsinformation kann nicht gespeichert werden.");
    A_22215.finish();
// GEMREQ-end A_22215

    A_22152.start("Check that ChargeItem fields except dispense item are unchanged");
    checkChargeItemConsistency(existingChargeInformation.chargeItem, newChargeItem);
    A_22152.finish();

    // GEMREQ-start A_22137#getBinary
    auto rawContainedBinary = getDispenseItemBinary(newChargeItem);
    // GEMREQ-end A_22137#getBinary

    A_22616_03.start("verify pharmacy access code");
    ChargeItemHandlerBase::verifyPharmacyAccessCode(session.request, existingChargeInformation.chargeItem, true);
    A_22616_03.finish();

    // GEMREQ-start A_22615-02#setNewAccessCode
    A_22615_02.start("create access code for pharmacy");
    const auto pharmacyAccessCode = ChargeItemHandlerBase::createPharmacyAccessCode();
    existingChargeInformation.chargeItem.setAccessCode(pharmacyAccessCode);
    A_22615_02.finish();
    // GEMREQ-end A_22615-02#setNewAccessCode

    // GEMREQ-start A_22141#chargeItemCadesBes, A_22150, A_22151
    A_22149.start("Pharmacy: validate PKV dispense item");
    A_22151_01.start("Pharmacy: check signature certificate of PKV dispense bundle");
    auto [dispenseItemBundle, containedBinaryWithOcsp] = validatedBundleFromSigned(
        *rawContainedBinary,
        session.serviceContext,
        ::VauErrorCode::invalid_dispense);
    A_22151_01.finish();
    A_22149.finish();
    // GEMREQ-end A_22141#chargeItemCadesBes, A_22150, A_22151

    A_22148.start("Pharmacy: set PKV dispense item reference in ChargeItem");
    A_22152.start("Pharmacy may only update dispense item");
    // GEMREQ-start A_22148#addBundleRef
    existingChargeInformation.chargeItem.setSupportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle);
    // GEMREQ-end A_22148#addBundleRef
    model::Binary newContainedBinary{*rawContainedBinary->id(), containedBinaryWithOcsp};
    ::std::swap(existingChargeInformation.dispenseItem.value(), newContainedBinary);
    ::std::swap(existingChargeInformation.unsignedDispenseItem.value(), dispenseItemBundle);

    // GEMREQ-start A_22148#updateChargeInformation
    session.database()->updateChargeInformation(existingChargeInformation, blobId, salt);
    A_22152.finish();
    A_22148.finish();

    A_23624.start("do not return access code in response");
    existingChargeInformation.chargeItem.deleteAccessCode();
    A_23624.finish();

    makeResponse(session, HttpStatus::OK, &existingChargeInformation.chargeItem);
    // GEMREQ-end A_22148#updateChargeInformation

    // Collect Audit data
    session.auditDataCollector()
        .setPrescriptionId(prescriptionId)
        .setEventId(model::AuditEventId::PUT_ChargeItem_id)
        .setInsurantKvnr(newChargeItem.subjectKvnr().value())
        .setAction(model::AuditEvent::Action::update);
}
