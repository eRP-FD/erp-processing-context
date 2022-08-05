/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
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


namespace
{

void checkChargeItemConsistency(
    const model::ChargeItem& chargeItem,
    const model::PrescriptionId& prescriptionId,
    const std::string_view& telematikId,
    const std::string_view& kvnr)
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

    A_22130.start("Check existence of 'task' parameter");
    const auto prescriptionId = parseIdFromQuery(session.request, session.accessLog, "task");
    A_22130.finish();

    A_22731.start("check flow type in PUT/POST ChargeItem");
    ErpExpect(prescriptionId.isPkv(), HttpStatus::BadRequest, "Referenced task is not of type PKV");
    A_22731.finish();

    auto* databaseHandle = session.database();

    A_22131.start("Check existence of referenced task in status 'completed'");
    auto [task, prescription, receipt] = databaseHandle->retrieveTaskAndPrescriptionAndReceipt(prescriptionId);

    ErpExpect(task.has_value(), HttpStatus::Conflict, "Referenced task not found for provided prescription id");
    ErpExpect(task->status() == model::Task::Status::completed, HttpStatus::Conflict, "Referenced task must be in status 'completed'");
    A_22131.finish();

    A_22132.start("Check that secret from URL is equal to secret from referenced task");
    const auto uriSecret = session.request.getQueryParameter("secret");
    A_20703.start("Set VAU-Error-Code header field to brute_force whenever AccessCode or Secret mismatches");
    Expect3(task->secret().has_value(), "Task has no secret", std::logic_error);
    VauExpect(uriSecret.has_value() && uriSecret.value() == task->secret().value(), HttpStatus::Forbidden,
              VauErrorCode::brute_force, "No or invalid secret provided for referenced Task");
    A_20703.finish();
    A_22132.finish();

    const auto telematikIdClaim = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    Expect3(telematikIdClaim.has_value(), "JWT does not contain TelematikId", std::logic_error); // should not happen because of JWT verification;

    A_22136.start("Validate input ChargeItem resource");
    auto chargeItem = parseAndValidateRequestBody<model::ChargeItem>(session, SchemaType::fhir, false);
    // TODO in unserem Schema is Binary == Gem_erxBinary mit contentType == application/pkcs7-mime, aber laut https://simplifier.net/erezept-workflow/GemerxBinary für
    // "PCKS signed ePrescription Bundle". Im PKV-Feature-Dokument wird contentType für das containedbinary nicht spezifiziert.
    // In ActivateTask wird die Signatur für Prescription im Enveloping gesendet, d.h. "signed data are sub-element of signature"
    const auto kvnr = task->kvnr();
    Expect3(kvnr.has_value(), "Referenced task has no KV number", std::logic_error);
    checkChargeItemConsistency(chargeItem, prescriptionId, telematikIdClaim.value(), kvnr.value());
    A_22136.finish();

    A_22133.start("Check consent");
    const auto consent = databaseHandle->retrieveConsent(kvnr.value());
    ErpExpect(consent.has_value(), HttpStatus::BadRequest, "No consent exists for this patient");
    A_22133.finish();

    A_22134.start("KBV prescription bundle");
    Expect3(prescription.has_value(), "No prescription found", std::logic_error);
    Expect3(prescription.value().data().has_value(), "Prescription binary has no data", std::logic_error);
    const auto signedPrescription =
        ::CadesBesSignature{::std::string{prescription->data().value()}, session.serviceContext.getTslManager(), true};
    auto prescriptionBundle = ::model::Bundle::fromXmlNoValidation(signedPrescription.payload());
    chargeItem.setSupportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItem, prescriptionBundle);
    A_22134.finish();

    A_22135.start("Receipt");
    Expect3(receipt.has_value(), "No receipt found", std::logic_error);
    chargeItem.setSupportingInfoReference(model::ChargeItem::SupportingInfoType::receipt, *receipt);
    // removal of signature can be done on retrieval;
    A_22135.finish();

    A_22137.start("Extract PKV dispense item");
    auto dispenseItem = getDispenseItemBinary(chargeItem);
    chargeItem.deleteContainedBinary();
    A_22137.finish();

    A_22138.start("Validate PKV dispense item");
    A_22139.start("Check signature of PKV dispense bundle");
    A_22140.start("Check signature certificate of PKV dispense bundle");
    A_22141.start("Check signature certificate SMC-B");
    A_22142.start("Save OCSP response for signature certificate");
    auto dispenseItemBundle = validatedBundleFromSigned(*dispenseItem, SchemaType::fhir /* TODO correct schema */,
                                                        session.serviceContext, ::VauErrorCode::invalid_dispense);
    A_22142.finish();
    A_22141.finish();
    A_22140.finish();
    A_22139.finish();
    A_22138.finish();

    A_22137.start("Set PKV dispense item reference in ChargeItem");
    chargeItem.setSupportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem, dispenseItemBundle);
    A_22137.finish();

    A_22143.start("Fill ChargeItem.enteredDate");
    chargeItem.setEnteredDate(fhirtools::Timestamp::now());
    A_22143.finish();

    chargeItem.setId(prescriptionId);

    A_22614.start("create access code for pharmacy");
    auto pharmacyAccessCode = ChargeItemHandlerBase::createPharmacyAccessCode();
    chargeItem.setAccessCode(std::move(pharmacyAccessCode));
    A_22614.finish();

    session.response.setHeader(Header::Location, getLinkBase() + "/ChargeItem/" + prescriptionId.toString());

    const auto chargeInformation = ::model::ChargeInformation{
        ::std::move(chargeItem),           ::std::move(prescription.value()), ::std::move(prescriptionBundle),
        ::std::move(dispenseItem.value()), ::std::move(dispenseItemBundle),   ::std::move(receipt.value())};

    databaseHandle->storeChargeInformation(chargeInformation);

    makeResponse(session, HttpStatus::Created, &chargeInformation.chargeItem);

    // Collect Audit data
    session.auditDataCollector()
        .setEventId(model::AuditEventId::POST_ChargeItem)
        .setInsurantKvnr(*kvnr)
        .setAction(model::AuditEvent::Action::create)
        .setPrescriptionId(prescriptionId);
}
