/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/task/CloseTaskHandler.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/network/message/MimeType.hxx"
#include "erp/crypto/SignedPrescription.hxx"
#include "erp/database/Database.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/Composition.hxx"
#include "erp/model/Device.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/model/KbvMedicationCompounding.hxx"
#include "erp/model/KbvMedicationFreeText.hxx"
#include "erp/model/KbvMedicationIngredient.hxx"
#include "erp/model/KbvMedicationPzn.hxx"
#include "shared/model/Kvnr.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/MedicationDispenseBundle.hxx"
#include "erp/model/MedicationDispenseId.hxx"
#include "erp/model/MedicationsAndDispenses.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "erp/model/Signature.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/extensions/KBVMultiplePrescription.hxx"
#include "shared/model/ResourceNames.hxx"
#include "erp/model/Task.hxx"
#include "shared/server/request/ServerRequest.hxx"
#include "erp/service/MedicationDispenseHandlerBase.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Demangle.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/TLog.hxx"
#include "shared/util/Uuid.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"

#include <boost/algorithm/string.hpp>
#include <boost/range/adaptors.hpp>
#include <memory>
#include <optional>

namespace
{
struct DigestIdentifier {
    DigestIdentifier(Configuration::PrescriptionDigestRefType refType, const std::string& linkBase,
                     const model::PrescriptionId& prescriptionId)
    {
        switch (refType)
        {
            case Configuration::PrescriptionDigestRefType::uuid:
                id = Uuid{}.toString();
                ref = "urn:uuid:" + id;
                fullUrl = ref;
                break;
            case Configuration::PrescriptionDigestRefType::relative:
                id = "PrescriptionDigest-" + prescriptionId.toString();
                ref = "Binary/" + id;
                fullUrl = linkBase + '/' + ref;
                break;
        }
    }
    std::string id;
    std::string ref;
    std::string fullUrl;
};
}


CloseTaskHandler::CloseTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : MedicationDispenseHandlerBase(Operation::POST_Task_id_close, allowedProfessionOiDs)
{
}

void CloseTaskHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();
    TVLOG(2) << "request body is '" << session.request.getBody() << "'";

    const auto prescriptionId = parseId(session.request, session.accessLog);

    TVLOG(1) << "Working on Task for prescription id " << prescriptionId.toString();

    auto* databaseHandle = session.database();

    auto [taskAndKey, prescription] = databaseHandle->retrieveTaskForUpdateAndPrescription(prescriptionId);
    ErpExpect(taskAndKey.has_value(), HttpStatus::NotFound, "Task not found for prescription id");
    auto& task = taskAndKey->task;
    const auto taskStatus = task.status();
    const auto inProgressDate = task.lastStatusChangeDate();

    ErpExpect(taskStatus != model::Task::Status::cancelled, HttpStatus::Gone, "Task has already been deleted");

    A_19231_02.start("Check that task is in progress");
    ErpExpect(taskStatus == model::Task::Status::inprogress, HttpStatus::Forbidden,
              "Task has to be in progress, but is: " + std::string(model::Task::StatusNames.at(taskStatus)));
    A_19231_02.finish();
    A_19231_02.start("Check that secret from URL is equal to secret from task");
    const auto uriSecret = session.request.getQueryParameter("secret");
    A_20703.start("Set VAU-Error-Code header field to brute_force whenever AccessCode or Secret mismatches");
    VauExpect(uriSecret.has_value() && uriSecret.value() == task.secret(), HttpStatus::Forbidden,
              VauErrorCode::brute_force, "No or invalid secret provided for Task");
    A_20703.finish();
    A_19231_02.finish();

    A_19232.start("Set Task status to completed");
    task.setStatus(model::Task::Status::completed);
    A_19232.finish();

    const auto& accessToken = session.request.getAccessToken();
    const auto telematikIdFromAccessToken = accessToken.stringForClaim(JWT::idNumberClaim);
    ErpExpect(telematikIdFromAccessToken.has_value(), HttpStatus::BadRequest, "Telematik-ID not contained in JWT");
    const auto kvnr = task.kvnr();
    Expect3(kvnr.has_value(), "Task has no KV number", std::logic_error);

    model::MedicationsAndDispenses medicationsAndDispenses;
    if (!session.request.getBody().empty())
    {
        medicationsAndDispenses = parseBody(session, Operation::POST_Task_id_close, prescriptionId.type());
    }
    if(medicationsAndDispenses.medicationDispenses.empty())
    {
        A_24287.start("Aufruf ohne MedicationDispense im Request Body");
        model::MedicationDispenseId medicationDispenseId(prescriptionId, 0);
        auto retrieved = databaseHandle->retrieveMedicationDispense(kvnr.value(), medicationDispenseId);
        ErpExpect(!retrieved.medicationDispenses.empty(), HttpStatus::Forbidden, "Medication dispense does not exist.");
        A_24287.finish();
    }
    else
    {
        checkMedicationDispenses(medicationsAndDispenses.medicationDispenses, prescriptionId, kvnr.value(),
                                 telematikIdFromAccessToken.value());
        A_26337.start("update lastMedicationDispense timestamp");
        task.updateLastMedicationDispense();
        A_26337.finish();
    }

    A_19233_05.start(
        "Create receipt bundle including Telematik-ID, timestamp of in-progress, current timestamp, prescription-id");
    const auto completedTimestamp = model::Timestamp::now();
    ErpExpect(inProgressDate < completedTimestamp, HttpStatus::InternalServerError, "in-progress date later than completed time.");
    const auto linkBase = getLinkBase();
    const auto& configuration = Configuration::instance();
    const auto authorIdentifier = generateCloseTaskDeviceRef(configuration.closeTaskDeviceRefType(), linkBase);
    const DigestIdentifier digestIdentifier{configuration.prescriptionDigestRefType(), linkBase, prescriptionId};
    const model::Composition compositionResource(telematikIdFromAccessToken.value(), inProgressDate, completedTimestamp,
                                                 authorIdentifier, digestIdentifier.ref);
    const model::Device deviceResource;

    A_19233_05.start("Save bundle reference in task.output");
    task.setReceiptUuid();
    A_19233_05.finish();

    A_19233_05.start("Add the prescription signature digest");
    ErpExpect(prescription.has_value() && prescription.value().data().has_value(), ::HttpStatus::InternalServerError,
              "No matching prescription found.");

    const auto cadesBesSignature = SignedPrescription::fromBinNoVerify(std::string{*prescription->data()});
    const auto kbvBundle = model::KbvBundle::fromXmlNoValidation(cadesBesSignature.payload());
    session.fillMvoBdeV2(kbvBundle.getExtension<model::KBVMultiplePrescription>());

    const std::string digest = cadesBesSignature.getMessageDigest();
    const auto base64Digest = ::Base64::encode(digest);
    std::optional metaVersionId =
        configuration.getOptionalStringValue(ConfigurationKey::SERVICE_TASK_CLOSE_PRESCRIPTION_DIGEST_VERSION_ID);
    if (metaVersionId && metaVersionId->empty())
    {
        metaVersionId.reset();
    }
    const auto prescriptionDigestResource =
        ::model::Binary{digestIdentifier.id, base64Digest, ::model::Binary::Type::Digest, metaVersionId};

    const auto taskUrl = linkBase + "/Task/" + prescriptionId.toString();
    model::ErxReceipt responseReceipt(Uuid(*task.receiptUuid()), taskUrl + "/$close/", prescriptionId,
                                      compositionResource, authorIdentifier, deviceResource, digestIdentifier.fullUrl,
                                      prescriptionDigestResource);
    A_19233_05.finish();
    A_19233_05.finish();

    A_19233_05.start("Sign the receipt with ID.FD.SIG using [RFC5652] with profile CAdES-BES ([CAdES]) ");
    // GEMREQ-start GS-A_4357-02#signData
    const std::string serialized = responseReceipt.serializeToXmlString();
    const std::string base64SignatureData =
        CadesBesSignature(
            session.serviceContext.getCFdSigErp(),
            session.serviceContext.getCFdSigErpPrv(),
            serialized,
            std::nullopt,
            session.serviceContext.getCFdSigErpManager().getOcspResponse()).getBase64();
    // GEMREQ-end GS-A_4357-02#signData
    A_19233_05.finish();

    A_19233_05.start("Set signature");
    const model::Signature signature(base64SignatureData, model::Timestamp::now(), authorIdentifier);
    responseReceipt.setSignature(signature);
    A_19233_05.finish();

    A_20513.start("Deletion of Communication resources referenced by Task");
    databaseHandle->deleteCommunicationsForTask(prescriptionId);
    A_20513.finish();

    // store in DB:
    A_19248_05.start("Save modified Task and MedicationDispense / Receipt objects");
    task.updateLastUpdate();
    ErpExpect(taskAndKey->key.has_value(), HttpStatus::InternalServerError, "Missing key for task");
    if (medicationsAndDispenses.medicationDispenses.empty())
    {
        // body was empty so we don't override medicationDispense
        databaseHandle->updateTaskReceipt(task, responseReceipt, *taskAndKey->key, session.request.getAccessToken());
    }
    else
    {
        databaseHandle->updateTaskMedicationDispenseReceipt(task, *taskAndKey->key,
                                                            createBundle(medicationsAndDispenses), responseReceipt,
                                                            session.request.getAccessToken());
    }
    A_19248_05.finish();

    A_19514.start("HttpStatus 200 for successful POST");
    makeResponse(session, HttpStatus::OK, &responseReceipt);
    A_19514.finish();

    // Collect Audit data
    session.auditDataCollector()
        .setEventId(model::AuditEventId::POST_Task_close)
        .setInsurantKvnr(*kvnr)
        .setAction(model::AuditEvent::Action::update)
        .setPrescriptionId(prescriptionId);
}

std::string CloseTaskHandler::generateCloseTaskDeviceRef(Configuration::DeviceRefType refType, const std::string& linkBase)
{
    switch (refType)
    {
        case Configuration::DeviceRefType::url:
            return model::Device::createReferenceUrl(linkBase);
        case Configuration::DeviceRefType::uuid:
            return Uuid{}.toUrn();
    }
    Fail2("Invalid value for Configuration::DeviceRefType: " + std::to_string(static_cast<uintmax_t>(refType)), std::logic_error);
}
