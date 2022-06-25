/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/chargeitem/ChargeItemPutHandler.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/util/TLog.hxx"

namespace
{

void checkChargeItemConsistencyCommon(
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
        const auto receiptRef = newChargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::receipt);
        const auto prescriptionRef = newChargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItem);
        const auto dispenseRef = newChargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem);
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

            ErpExpect(receiptRef == existingChargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::receipt),
                      HttpStatus::BadRequest,
                      "Receipt reference in provided ChargeItem does not match the one from the ChargeItem to update");
            ErpExpect(prescriptionRef == existingChargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItem),
                      HttpStatus::BadRequest,
                      "Prescription reference in provided ChargeItem does not match the one from the ChargeItem to update");
            ErpExpect(dispenseRef == existingChargeItem.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem),
                      HttpStatus::BadRequest,
                      "Dispense reference in provided ChargeItem does not match the one from the ChargeItem to update");
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


void ChargeItemPutHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    auto* databaseHandle = session.database();

    const auto prescriptionId = parseIdFromPath(session.request, session.accessLog);
    auto [existingChargeItem, existingDispenseItem] = databaseHandle->retrieveChargeInformationForUpdate(prescriptionId);

    A_22152.start("Validate input ChargeItem and check that common fields are unchanged");
    auto newChargeItem = parseAndValidateRequestBody<model::ChargeItem>(session, SchemaType::Gem_erxChargeItem);
    checkChargeItemConsistencyCommon(existingChargeItem, newChargeItem);
    A_22152.finish();

    A_22215.start("Check consent");
    const auto consent = databaseHandle->retrieveConsent(newChargeItem.subjectKvnr());
    ErpExpect(consent.has_value(), HttpStatus::BadRequest, "No consent exists for this patient");
    A_22215.finish();

    const auto professionOIDClaim = session.request.getAccessToken().stringForClaim(JWT::professionOIDClaim);
    Expect3(professionOIDClaim.has_value(), "Missing professionOIDClaim", std::logic_error);  // should not happen because of JWT verification;

    const auto idClaim = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    Expect3(idClaim.has_value(), "JWT does not contain Id", std::logic_error); // should not happen because of JWT verification;

    model::AuditEventId auditEventId{};

    if(professionOIDClaim == profession_oid::oid_versicherter)
    {
        handleRequestInsurant(session, existingChargeItem, existingDispenseItem, newChargeItem, idClaim.value());
        auditEventId = model::AuditEventId::PUT_ChargeItem_id_insurant;
    }
    else if(professionOIDClaim == profession_oid::oid_oeffentliche_apotheke ||
            professionOIDClaim == profession_oid::oid_krankenhausapotheke)
    {
        handleRequestPharmacy(session, existingChargeItem, newChargeItem, idClaim.value());
        auditEventId = model::AuditEventId::PUT_ChargeItem_id_pharmacy;
    }
    else
    {
        Fail2("Invalid professionOID", std::logic_error); // should not happen because of OID check;
    }

    makeResponse(session, HttpStatus::OK, &existingChargeItem);

    // Collect Audit data
    session.auditDataCollector()
        .setPrescriptionId(prescriptionId)
        .setEventId(auditEventId)
        .setInsurantKvnr(newChargeItem.subjectKvnr())
        .setAction(model::AuditEvent::Action::update);
}


void ChargeItemPutHandler::handleRequestInsurant(
    PcSessionContext& session,
    model::ChargeItem& existingChargeItem,
    const model::Bundle& existingDispenseItem,
    const model::ChargeItem& newChargeItem,
    const std::string& idClaim)
{
    A_22145.start("Insurant: validation of Kvnr");
    ErpExpect(newChargeItem.subjectKvnr() == idClaim, HttpStatus::Forbidden,
              "Kvnr in ChargeItem does not match the one from the access token");
    A_22145.finish();

    // Insurant may only adapt markings, the rest remains unchanged:
    A_22152.start("Insurant may only update marking");
    // TODO Check that dispense item is unchanged (see question https://dth01.ibmgcloud.net/jira/browse/ERP-9118)
    const auto newMarkingFlag = newChargeItem.markingFlag();
    if(newMarkingFlag.has_value())
    {
        existingChargeItem.setMarkingFlag(newChargeItem.markingFlag().value());
    }
    else
    {
        existingChargeItem.deleteMarkingFlag();
    }

    session.database()->storeChargeInformation(
        existingChargeItem.entererTelematikId(), existingChargeItem, existingDispenseItem);
    A_22152.finish();
}


void ChargeItemPutHandler::handleRequestPharmacy(
    PcSessionContext& session,
    model::ChargeItem& existingChargeItem,
    model::ChargeItem& newChargeItem,
    const std::string& idClaim)
{
    A_22146.start("Pharmacy: validation of TelematikId");
    ErpExpect(newChargeItem.entererTelematikId() == idClaim, HttpStatus::Forbidden,
              "TelematikId in ChargeItem does not match the one from the access token");
    A_22146.finish();
    A_22147.start("Pharmacy: warn if ChargedItem is already marked by insurant");
    const auto existingMarkingFlag = existingChargeItem.markingFlag();
    const auto existingMarkings = existingMarkingFlag.has_value() ?
        existingMarkingFlag->getAllMarkings() : model::ChargeItemMarkingFlag::MarkingContainer() ;
    if(model::ChargeItemMarkingFlag::isMarked(existingMarkings))
    {
        session.response.setHeader(Header::Warning, "Accounted");
    }
    A_22147.finish();

    A_22616.start("verify pharmacy access code");
    ChargeItemHandlerBase::verifyPharmacyAccessCode(session.request, existingChargeItem, true);
    A_22616.finish();

    A_22152.start("Pharmacy: check that marking is unchanged");
    const auto newMarkingFlag = newChargeItem.markingFlag();
    const auto newMarkings = newMarkingFlag.has_value() ?
        newMarkingFlag->getAllMarkings() : model::ChargeItemMarkingFlag::MarkingContainer() ;
    ErpExpect(existingMarkings == newMarkings, HttpStatus::BadRequest,
              "Markings in provided ChargeItem do not match the ones from the ChargeItem to update");
    A_22152.finish();

    A_22615.start("create access code for pharmacy");
    auto pharmacyAccessCode = ChargeItemHandlerBase::createPharmacyAccessCode();
    newChargeItem.setAccessCode(std::move(pharmacyAccessCode));
    A_22615.finish();

    A_22148.start("Pharmacy: get PKV dispense item");
    const auto containedBinary = getDispenseItemBinary(newChargeItem);
    A_22148.finish();

    A_22149.start("Pharmacy: validate PKV dispense item");
    A_22150.start("Pharmacy: Check signature of PKV dispense bundle");
    A_22151.start("Pharmacy: check signature certificate of PKV dispense bundle");
    const auto dispenseItemBundle = createAndValidateDispenseItem(*containedBinary, session.serviceContext);
    A_22151.finish();
    A_22150.finish();
    A_22149.finish();

    A_22148.start("Pharmacy: set PKV dispense item reference in ChargeItem");
    setDispenseItemReference(existingChargeItem, dispenseItemBundle);
    A_22148.finish();

    A_22152.start("Pharmacy may only update dispense item");
    session.database()->storeChargeInformation(
        existingChargeItem.entererTelematikId(), existingChargeItem, dispenseItemBundle);
    A_22152.finish();
}
