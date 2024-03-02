/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/chargeitem/ChargeItemPostHandler.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/ModelException.hxx"
#include "erp/model/Task.hxx"
#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/TLog.hxx"

#include <pqxx/except>

namespace
{

void checkChargeItemConsistency(
    const model::ChargeItem& chargeItem,
    const model::PrescriptionId& prescriptionId,
    const std::string_view& telematikId,
    const model::Kvnr& kvnr)
{
    try
    {
        ErpExpect(chargeItem.prescriptionId() == prescriptionId, HttpStatus::BadRequest,
                  "Prescription ID in ChargeItem does not match the 'task' parameter");
        ErpExpect(chargeItem.entererTelematikId() == telematikId, HttpStatus::BadRequest,
                  "Telematik ID in ChargeItem does not match the one from the access token");
        ErpExpect(chargeItem.subjectKvnr() == kvnr, HttpStatus::BadRequest,
                  "Kvnr in ChargeItem does not match the one from the referenced task");
    }
    catch(const model::ModelException& exc)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Invalid charge item provided", exc.what());
    }
}

} // anonymous namespace


ChargeItemPostHandler::ChargeItemPostHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : ChargeItemBodyHandlerBase(Operation::POST_ChargeItem, allowedProfessionOiDs)
{
}

void ChargeItemPostHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    // GEMREQ-start A_22135-01#getReceipt, A_22134#getReceipt
    A_22130.start("Check existence of 'task' parameter");
    const auto prescriptionId = parseIdFromQuery(session.request, session.accessLog, "task");
    A_22130.finish();

    A_22731.start("check flow type in PUT/POST ChargeItem");
    ErpExpect(prescriptionId.isPkv(), HttpStatus::BadRequest, "Referenced task is not of type PKV");
    A_22731.finish();

    auto* databaseHandle = session.database();

    A_22131.start("Check existence of referenced task in status 'completed'");
    auto [task, prescription, receipt] = databaseHandle->retrieveTaskAndPrescriptionAndReceipt(prescriptionId);
    // GEMREQ-end A_22135-01#getReceipt, A_22134#getReceipt

    ErpExpect(task.has_value(), HttpStatus::Conflict, "Referenced task not found for provided prescription id");
    ErpExpect(task->status() == model::Task::Status::completed, HttpStatus::Conflict, "Referenced task must be in status 'completed'");
    A_22131.finish();

    A_22132_02.start("Check that secret from URL is equal to secret from referenced task");
    const auto uriSecret = session.request.getQueryParameter("secret");
    A_20703.start("Set VAU-Error-Code header field to brute_force whenever AccessCode or Secret mismatches");
    Expect3(task->secret().has_value(), "Task has no secret", std::logic_error);
    VauExpect(uriSecret.has_value() && uriSecret.value() == task->secret().value(), HttpStatus::Forbidden,
              VauErrorCode::brute_force, "No or invalid secret provided for referenced Task");
    A_20703.finish();
    A_22132_02.finish();

    const auto telematikIdClaim = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    Expect3(telematikIdClaim.has_value(), "JWT does not contain TelematikId", std::logic_error); // should not happen because of JWT verification;

    A_22136_01.start("Validate input ChargeItem resource");
    std::optional<model::ChargeItem> chargeItemOptional{};
    std::optional<model::ChargeItemMarkingFlags> markingFlags{};
    try
    {
        chargeItemOptional = parseAndValidateRequestBody<model::ChargeItem>(session, SchemaType::Gem_erxChargeItem);
        markingFlags = chargeItemOptional->markingFlags();
    }
    catch (const model::ModelException& exc)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Invalid charge item provided", exc.what());
    }

    auto& chargeItem = *chargeItemOptional;
    ErpExpect(
        ! markingFlags.has_value(),
        HttpStatus::BadRequest,
        "Marking flag should not be set in POST ChargeItem by pharmacy");

    const auto kvnr = task->kvnr();
    Expect3(kvnr.has_value(), "Referenced task has no KV number", std::logic_error);
    checkChargeItemConsistency(chargeItem, prescriptionId, telematikIdClaim.value(), kvnr.value());
    A_22136_01.finish();

    A_22133.start("Check consent");
    const auto consent = databaseHandle->retrieveConsent(kvnr.value());
    ErpExpect(
        consent.has_value(),
        HttpStatus::Forbidden,
        "Die versicherte Person hat keine Einwilligung im E-Rezept Fachdienst hinterlegt. Abrechnungsinformation kann nicht gespeichert werden.");
    A_22133.finish();

    // GEMREQ-start A_22134#setReference
    A_22134.start("KBV prescription bundle");
    Expect3(prescription.has_value(), "No prescription found", std::logic_error);
    Expect3(prescription.value().data().has_value(), "Prescription binary has no data", std::logic_error);
    const auto signedPrescription =
        ::CadesBesSignature{::std::string{prescription->data().value()}, session.serviceContext.getTslManager(), true};
    auto prescriptionBundle = ::model::Bundle::fromXmlNoValidation(signedPrescription.payload());
    chargeItem.setSupportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItemBundle);
    A_22134.finish();
    // GEMREQ-end A_22134#setReference

    // GEMREQ-start A_22135-01#setReference
    A_22135_01.start("Receipt");
    Expect3(receipt.has_value(), "No receipt found", std::logic_error);
    chargeItem.setSupportingInfoReference(model::ChargeItem::SupportingInfoType::receiptBundle);
    A_22135_01.finish();
    // GEMREQ-end A_22135-01#setReference

    // GEMREQ-start A_22137#remove-binary
    auto rawDispenseItem = getDispenseItemBinary(chargeItem);
    chargeItem.deleteContainedBinary();
    // GEMREQ-end A_22137#remove-binary

    // GEMREQ-start A_22141#chargeItemCadesBes, A_22140
    A_22138.start("Validate PKV dispense item");
    A_22139.start("Check signature of PKV dispense bundle");
    A_22140_01.start("Check signature certificate of PKV dispense bundle");
    A_22141.start("Check signature certificate SMC-B");
    auto [dispenseItemBundle, dispenseItemWithOcsp] = validatedBundleFromSigned(
        *rawDispenseItem,
        session.serviceContext,
        ::VauErrorCode::invalid_dispense);

    A_22141.finish();
    A_22140_01.finish();
    A_22139.finish();
    A_22138.finish();
    // GEMREQ-end A_22141#chargeItemCadesBes, A_22140

    // GEMREQ-start A_22137#addBundleRef
    chargeItem.setSupportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle);
    // GEMREQ-end A_22137#addBundleRef

    A_22143.start("Fill ChargeItem.enteredDate");
    chargeItem.setEnteredDate(model::Timestamp::now());
    A_22143.finish();

    chargeItem.setId(prescriptionId);

    // GEMREQ-start A_22614-02#setAccessCode
    A_22614_02.start("create access code for pharmacy");
    const auto pharmacyAccessCode = ChargeItemHandlerBase::createPharmacyAccessCode();
    chargeItem.setAccessCode(pharmacyAccessCode);
    A_22614_02.finish();
    // GEMREQ-end A_22614-02#setAccessCode

    session.response.setHeader(Header::Location, getLinkBase() + "/ChargeItem/" + prescriptionId.toString());

    // GEMREQ-start A_22137#storeChargeItem, A_22135-01#storeChargeItem, A_22134#storeChargeItem
    model::Binary dispenseItemBinary = model::Binary{*rawDispenseItem->id(), dispenseItemWithOcsp};
    auto chargeInformation = model::ChargeInformation{.chargeItem = std::move(chargeItem),
                                                      .prescription = std::move(prescription.value()),
                                                      .unsignedPrescription = std::move(prescriptionBundle),
                                                      .dispenseItem = std::move(dispenseItemBinary),
                                                      .unsignedDispenseItem = std::move(dispenseItemBundle),
                                                      .receipt = std::move(receipt.value())};

    try
    {
        databaseHandle->storeChargeInformation(chargeInformation);
    }
    catch(const pqxx::unique_violation& exc)
    {
        ErpFail(HttpStatus::Conflict, "ChargeItem already exists for this prescription ID");
    }
    // GEMREQ-end A_22137#storeChargeItem, A_22135-01#storeChargeItem, A_22134#storeChargeItem

    A_23704.start("do not return access code in response");
    chargeInformation.chargeItem.deleteAccessCode();
    A_23704.finish();

    makeResponse(session, HttpStatus::Created, &chargeInformation.chargeItem);

    // Collect Audit data
    session.auditDataCollector()
        .setEventId(model::AuditEventId::POST_ChargeItem)
        .setInsurantKvnr(*kvnr)
        .setAction(model::AuditEvent::Action::create)
        .setPrescriptionId(prescriptionId);
}
