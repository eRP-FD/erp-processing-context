/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/task/CloseTaskHandler.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/common/MimeType.hxx"
#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/database/Database.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/Composition.hxx"
#include "erp/model/Device.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/model/KbvMedicationCompounding.hxx"
#include "erp/model/KbvMedicationFreeText.hxx"
#include "erp/model/KbvMedicationIngredient.hxx"
#include "erp/model/KbvMedicationPzn.hxx"
#include "erp/model/Kvnr.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/MedicationDispenseBundle.hxx"
#include "erp/model/MedicationDispenseId.hxx"
#include "erp/model/NumberAsStringParserWriter.hxx"
#include "erp/model/Signature.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/model/Task.hxx"
#include "erp/server/request/ServerRequest.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/Uuid.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/parser/FhirPathParser.hxx"

#include <boost/algorithm/string.hpp>
#include <boost/range/adaptors.hpp>
#include <memory>
#include <optional>


CloseTaskHandler::CloseTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : TaskHandlerBase(Operation::POST_Task_id_close, allowedProfessionOiDs)
{
}

void CloseTaskHandler::handleRequest(PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();
    TVLOG(2) << "request body is '" << session.request.getBody() << "'";

    const auto prescriptionId = parseId(session.request, session.accessLog);

    TVLOG(1) << "Working on Task for prescription id " << prescriptionId.toString();

    auto* databaseHandle = session.database();

    auto [task, prescription] = databaseHandle->retrieveTaskForUpdateAndPrescription(prescriptionId);
    ErpExpect(task.has_value(), HttpStatus::NotFound, "Task not found for prescription id");
    const auto taskStatus = task->status();

    ErpExpect(taskStatus != model::Task::Status::cancelled, HttpStatus::Gone, "Task has already been deleted");

    A_19231_02.start("Check that task is in progress");
    ErpExpect(taskStatus == model::Task::Status::inprogress, HttpStatus::Forbidden,
              "Task has to be in progress, but is: " + std::string(model::Task::StatusNames.at(taskStatus)));
    A_19231_02.finish();
    A_19231_02.start("Check that secret from URL is equal to secret from task");
    const auto uriSecret = session.request.getQueryParameter("secret");
    A_20703.start("Set VAU-Error-Code header field to brute_force whenever AccessCode or Secret mismatches");
    VauExpect(uriSecret.has_value() && uriSecret.value() == task->secret(), HttpStatus::Forbidden,
              VauErrorCode::brute_force, "No or invalid secret provided for Task");
    A_20703.finish();
    A_19231_02.finish();

    A_19232.start("Set Task status to completed");
    task->setStatus(model::Task::Status::completed);
    A_19232.finish();

    const auto& accessToken = session.request.getAccessToken();
    const auto telematikIdFromAccessToken = accessToken.stringForClaim(JWT::idNumberClaim);
    ErpExpect(telematikIdFromAccessToken.has_value(), HttpStatus::BadRequest, "Telematik-ID not contained in JWT");
    const auto kvnr = task->kvnr();
    Expect3(kvnr.has_value(), "Task has no KV number", std::logic_error);

    auto medicationDispenses = medicationDispensesFromBody(session);
    for (size_t i = 0, end = medicationDispenses.size(); i < end; ++i)
    {
        auto& medicationDispense = medicationDispenses[i];
        A_19248_02.start("Check provided MedicationDispense object, especially PrescriptionID, KVNR and TelematikID");

        // See https://simplifier.net/erezept-workflow/GemerxMedicationDispense/~details
        // The logical id of the resource, as used in the URL for the resource. Once assigned, this value never changes.
        // The only time that a resource does not have an id is when it is being submitted to the server using a create operation.
        // As medication dispenses are queried by "GET /MedicationDispense/{Id}" where Id equals the TaskId (= prescriptionId)
        // the Id of the medication dispense resource is set to the prescriptionId.
        // Check for correct MedicationDispense.identifier
        try
        {
            ErpExpect(medicationDispense.prescriptionId() == prescriptionId, HttpStatus::BadRequest,
                      "Prescription ID in MedicationDispense does not match the one in the task");
        }
        catch (const model::ModelException& exc)
        {
            TVLOG(1) << "ModelException: " << exc.what();
            ErpFail(HttpStatus::BadRequest, "Invalid Prescription ID in MedicationDispense (wrong content)");
        }
        medicationDispense.setId({prescriptionId, i});

        ErpExpect(medicationDispense.kvnr() == *kvnr, HttpStatus::BadRequest,
                  "KVNR in MedicationDispense does not match the one in the task");

        ErpExpect(medicationDispense.telematikId() == *telematikIdFromAccessToken, HttpStatus::BadRequest,
                  "Telematik-ID in MedicationDispense does not match the one in the access token.");
        A_19248_02.finish();
    }

    A_19233_05.start(
        "Create receipt bundle including Telematik-ID, timestamp of in-progress, current timestamp, prescription-id");
    const auto inProgressDate = task->lastModifiedDate();
    const auto completedTimestamp = model::Timestamp::now();
    const auto linkBase = getLinkBase();
    const auto authorIdentifier = model::Device::createReferenceString(linkBase);
    const std::string prescriptionDigestIdentifier = "PrescriptionDigest-" + prescriptionId.toString();
    const model::Composition compositionResource(telematikIdFromAccessToken.value(), inProgressDate, completedTimestamp,
                                           authorIdentifier, "Binary/" + prescriptionDigestIdentifier);
    const model::Device deviceResource;

    A_19233_05.start("Save bundle reference in task.output");
    task->setReceiptUuid();
    A_19233_05.finish();

    A_19233_05.start("Add the prescription signature digest");
    ErpExpect(prescription.has_value() && prescription.value().data().has_value(), ::HttpStatus::InternalServerError,
              "No matching prescription found.");
    const std::string digest =
        CadesBesSignature{::std::string{prescription.value().data().value()}}.getMessageDigest();
    const auto base64Digest = ::Base64::encode(digest);
    const auto prescriptionDigestResource =
        ::model::Binary{prescriptionDigestIdentifier, base64Digest, ::model::Binary::Type::Digest};

    const auto taskUrl = linkBase + "/Task/" + prescriptionId.toString();
    const auto prescriptionDigestUrl = linkBase + "/Binary/" + prescriptionDigestIdentifier;
    model::ErxReceipt responseReceipt(Uuid(*task->receiptUuid()), taskUrl + "/$close/", prescriptionId,
                                      compositionResource, authorIdentifier, deviceResource,
                                      prescriptionDigestUrl, prescriptionDigestResource);
    A_19233_05.finish();
    A_19233_05.finish();

    A_19233_05.start("Sign the receipt with ID.FD.SIG using [RFC5652] with profile CAdES-BES ([CAdES]) ");
    const std::string serialized = responseReceipt.serializeToXmlString();
    const std::string base64SignatureData =
        CadesBesSignature(
            session.serviceContext.getCFdSigErp(),
            session.serviceContext.getCFdSigErpPrv(),
            serialized,
            std::nullopt,
            session.serviceContext.getCFdSigErpManager().getOcspResponse()).getBase64();
    A_19233_05.finish();

    A_19233_05.start("Set signature");
    const model::Signature signature(base64SignatureData, model::Timestamp::now(), authorIdentifier);
    responseReceipt.setSignature(signature);
    A_19233_05.finish();

    A_20513.start("Deletion of Communication resources referenced by Task");
    databaseHandle->deleteCommunicationsForTask(prescriptionId);
    A_20513.finish();

    // store in DB:
    A_19248_02.start("Save modified Task and MedicationDispense / Receipt objects");
    task->updateLastUpdate();
    databaseHandle->updateTaskMedicationDispenseReceipt(*task, medicationDispenses, responseReceipt);
    A_19248_02.finish();

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

std::vector<model::MedicationDispense> CloseTaskHandler::medicationDispensesFromBody(PcSessionContext& session)
{
    try
    {
        A_22069.start("Detect input resource type: Bundle or MedicationDispense");
        auto unspec = createResourceFactory<model::UnspecifiedResource>(session);
        const auto resourceType = unspec.getResourceType();
        ErpExpect(resourceType == model::Bundle::resourceTypeName ||
                    resourceType == model::MedicationDispense::resourceTypeName,
                HttpStatus::BadRequest, "Unsupported resource type in request body: " + std::string(resourceType));
        A_22069.finish();

        if (resourceType == model::Bundle::resourceTypeName)
        {
            auto bundle = for_resource<model::MedicationDispenseBundle>(std::move(unspec)).getNoValidation();
            auto ret = bundle.getResourcesByType<model::MedicationDispense>();
            validateWithoutMedicationProfiles<model::MedicationDispenseBundle>(std::move(bundle).jsonDocument(),
                                            "entry.resource.medicationReference.resolve()", session);
            validateMedications(ret, session.serviceContext);
            return ret;
        }
        else
        {
            std::vector<model::MedicationDispense> ret;
            const auto& dispense =
                    ret.emplace_back(for_resource<model::MedicationDispense>(std::move(unspec)).getNoValidation());
            model::NumberAsStringParserDocument doc;
            doc.CopyFrom(dispense.jsonDocument(), doc.GetAllocator());
            validateWithoutMedicationProfiles<model::MedicationDispense>(std::move(doc), "medicationReference.resolve()",
                                                                        session);
            validateMedications(ret, session.serviceContext);
            return ret;
        }
    }
    catch(const model::ModelException& e)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Error during request parsing.", e.what());
    }
}

void CloseTaskHandler::validateMedications(const std::vector<model::MedicationDispense>& medicationDispenses,
                                           const PcServiceContext& service)
{
    using namespace std::string_literals;
    const auto* repo = std::addressof(Fhir::instance().structureRepository());
    auto extractMedication = fhirtools::FhirPathParser::parse(repo, "medicationReference.resolve()");
    Expect3(extractMedication, "Failed parsing extractMedication", std::logic_error);
    for (const auto& md : medicationDispenses)
    {
        auto elem = std::make_shared<ErpElement>(repo, std::weak_ptr<const ErpElement>{}, "MedicationDispense",
                                                 std::addressof(md.jsonDocument()));
        auto medication = extractMedication->eval(fhirtools::Collection{elem});
        ErpExpect(! medication.empty(), HttpStatus::BadRequest, "Failed to extract Medication");
        ErpExpect(medication.size() == 1, HttpStatus::BadRequest,
                  "medicationReference references more than one Medication");
        auto medicationElement = std::dynamic_pointer_cast<const ErpElement>(medication[0]);
        if (medicationElement == nullptr)
        {
            const auto& medication0 = *medication[0];
            Fail2("Expression returned non-ErpElement: "s.append(typeid(medication0).name()), std::logic_error);
        }
        model::KbvMedicationGeneric::validateMedication(*medicationElement, service.getXmlValidator(),
                                                        service.getInCodeValidator());
    }
}

template<typename ModelT>
void CloseTaskHandler::validateWithoutMedicationProfiles(
        model::NumberAsStringParserDocument&& medicationDispenseOrBundleDoc,
        std::string_view medicationsPath, const PcSessionContext& session)
{
    using namespace std::string_literals;
    namespace resource = model::resource;
    namespace elements = resource::elements;
    namespace rv = model::ResourceVersion;
    using Factory = model::ResourceFactory<ModelT>;
    static rapidjson::Pointer metaPtr{resource::ElementName::path(elements::meta)};
    const auto* repo = std::addressof(Fhir::instance().structureRepository());
    auto extractMedications = fhirtools::FhirPathParser::parse(repo, medicationsPath);
    Expect3(extractMedications != nullptr, "Failed parsing: "s.append(medicationsPath), std::logic_error);

    auto rootElement = std::make_shared<ErpElement>(repo, std::weak_ptr<const ErpElement>{}, ModelT::resourceTypeName,
                                            std::addressof(medicationDispenseOrBundleDoc));
    auto medications = extractMedications->eval(fhirtools::Collection{rootElement});
    validateSameMedicationVersion(medications);
    TVLOG(3) << "patching " << medications.size()  << " medications";
    for (const auto& medicationElement: medications)
    {
        const auto& mutableMedicationElement = dynamic_cast<const ErpElement&>(*medicationElement);
        //NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        metaPtr.Erase(*const_cast<rapidjson::Value*>(mutableMedicationElement.erpValue()));
    }
    typename Factory::Options factoryOptions{};
    if (rv::supportedBundles().contains(rv::FhirProfileBundleVersion::v_2023_07_01))
    {
        // ERP-13560 Ablehnen von FHIR 2022 Medication Dispense ab dem 1.7.2023 in Close Task
        factoryOptions.enforcedVersion =
            rv::ProfileBundle<rv::FhirProfileBundleVersion::v_2023_07_01, typename ModelT::SchemaVersionType>::version;
    }
    auto medicationDispenseOrBundle = Factory::fromJson(std::move(medicationDispenseOrBundleDoc), factoryOptions);
    (void) std::move(medicationDispenseOrBundle).getValidated(ModelT::schemaType,
                                                              session.serviceContext.getXmlValidator(),
                                                              session.serviceContext.getInCodeValidator());
}

void CloseTaskHandler::validateSameMedicationVersion(const fhirtools::Collection& medications)
{
    A_23384_draft.start("E-Rezept-Fachdienst: Prüfung Gültigkeit Profilversionen");
    using namespace std::string_literals;
    if (!medications.empty())
    {
        auto firstProfiles = medications[0]->profiles();
        ErpExpect(!firstProfiles.empty(), HttpStatus::BadRequest, "Missing meta.profile in Medication.");
        for (const auto& medicationElement: medications)
        {
            auto hasFirstProfile = std::ranges::any_of(medicationElement->profiles(),
                    [firstProfiles](std::string_view p){ return p == firstProfiles[0];});
            ErpExpectWithDiagnostics(hasFirstProfile, HttpStatus::BadRequest,
                    "All Medications must have the same profile.", "expected: "s.append(firstProfiles[0]));
        }
    }
    A_23384_draft.finish();
}
