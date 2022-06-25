/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/task/ActivateTaskHandler.hxx"

#include "erp/ErpRequirements.hxx"
#include "erp/database/Database.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/Composition.hxx"
#include "erp/model/KbvMedicationCompounding.hxx"
#include "erp/model/KbvMedicationBase.hxx"
#include "erp/model/KbvMedicationRequest.hxx"
#include "erp/model/Patient.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/Parameters.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/KbvCoverage.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/Uuid.hxx"

#include <date/date.h>


ActivateTaskHandler::ActivateTaskHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs)
        : TaskHandlerBase(Operation::POST_Task_id_activate, allowedProfessionOiDs)
{
}

void ActivateTaskHandler::handleRequest (PcSessionContext& session)
{
    const auto& config = Configuration::instance();
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();
    TVLOG(2) << "request body is '" << session.request.getBody() << "'";

    const auto prescriptionId = parseId(session.request, session.accessLog);
    checkFeatureWf200(prescriptionId.type());

    auto* databaseHandle = session.database();
    auto task = databaseHandle->retrieveTaskForUpdate(prescriptionId);

    ErpExpect(task.has_value(), HttpStatus::NotFound, "Requested Task not found in DB");

    // the prescription ID is not persisted in $create in order to avoid a second DB access there.
    task->setPrescriptionId(prescriptionId);

    const auto taskStatus = task->status();

    ErpExpect(taskStatus != model::Task::Status::cancelled, HttpStatus::Gone, "Task has already been deleted");

    A_19024_01.start("check access code");
    checkAccessCodeMatches(session.request, *task);
    A_19024_01.finish();

    A_19024_01.start("check status draft");
    ErpExpect(taskStatus == model::Task::Status::draft, HttpStatus::Forbidden,
              "Task not in status draft but in status " + std::string(model::Task::StatusNames.at(taskStatus)));
    A_19024_01.finish();

    const auto parameterResource = parseAndValidateRequestBody<model::Parameters>(session, SchemaType::ActivateTaskParameters);
    const auto* ePrescriptionValue = parameterResource.findResourceParameter("ePrescription");
    ErpExpect(ePrescriptionValue != nullptr, HttpStatus::BadRequest, "Failed to get ePrescription from requestBody");
    const auto& ePrescriptionParameter = model::Binary::fromJson(*ePrescriptionValue);
    const auto binaryData = ePrescriptionParameter.data();
    ErpExpect(binaryData.has_value(), HttpStatus::BadRequest, "Failed to get ePrescription. Document has no data.");
    const std::string cadesBesSignatureFile{binaryData.value()};

    A_19020.start("check PKCS#7 for structural validity");
    A_20704.start("Set VAU-Error-Code header field to invalid_prescription when an invalid "
                  "prescription has been transmitted");
    const CadesBesSignature cadesBesSignature = unpackCadesBesSignature(
        cadesBesSignatureFile, session.serviceContext.getTslManager());
    A_20704.finish();
    A_19020.finish();

    const auto& prescription = cadesBesSignature.payload();

    auto prescriptionBundle = [&]() {
        if (config.getOptionalBoolValue( ConfigurationKey::SERVICE_TASK_ACTIVATE_KBV_VALIDATION, true))
        {
            return model::KbvBundle::fromXml(prescription, session.serviceContext.getXmlValidator(),
                                             session.serviceContext.getInCodeValidator(),
                                             SchemaType::KBV_PR_ERP_Bundle);
        }
        return model::KbvBundle::fromXml(prescription, session.serviceContext.getXmlValidator(),
                                         session.serviceContext.getInCodeValidator(), SchemaType::fhir);
    }();

    checkMultiplePrescription(prescriptionBundle);

    A_22231.start("check narcotics and Thalidomid");
    checkNarcoticsMatches(prescriptionBundle);
    A_22231.finish();

    std::optional<model::PrescriptionId> bundlePrescriptionId;
    try {
        bundlePrescriptionId.emplace(prescriptionBundle.getIdentifier());
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest,
                               "Error while extracting prescriptionId from QES-Bundle", ex.what());
    }
    ErpExpect(bundlePrescriptionId.has_value(), HttpStatus::BadRequest,
              "Failed to extract prescriptionId from QES-Bundle");

    A_21370.start("compare the task flowtype against the prefix of the prescription-id");
    ErpExpect(bundlePrescriptionId->type() == task->type(), HttpStatus::BadRequest,
              "Flowtype mismatch between Task and QES-Bundle");
    A_21370.finish();

    if (! config.getOptionalBoolValue(ConfigurationKey::DEBUG_DISABLE_QES_ID_CHECK, false))
    {
        A_21370.start("compare the prescription id of the QES bundle with the task");
        ErpExpect(*bundlePrescriptionId == task->prescriptionId(), HttpStatus::BadRequest,
                    "PrescriptionId mismatch between Task and QES-Bundle");
        A_21370.finish();
    }
    else
    {
        TLOG(ERROR) << "PrescriptionId check of QES-Bundle is disabled";
    }

    const auto signingTime = cadesBesSignature.getSigningTime();
    ErpExpect(signingTime.has_value(), HttpStatus::BadRequest, "No signingTime in PKCS7 file");

    date::year_month_day signingDay{date::floor<date::days>(signingTime->toChronoTimePoint())};
    if (config.getOptionalBoolValue(ConfigurationKey::SERVICE_TASK_ACTIVATE_AUTHORED_ON_MUST_EQUAL_SIGNING_DATE, true))
    {
        checkAuthoredOnEqualsSigningDate(prescriptionBundle, signingDay);
    }

    checkValidCoverage(prescriptionBundle, prescriptionId.type());

    A_19025.start("3. reference the PKCS7 file in task");
    task->setHealthCarePrescriptionUuid();
    task->setPatientConfirmationUuid();
    const model::Binary healthCareProviderPrescriptionBinary(*task->healthCarePrescriptionUuid(), cadesBesSignatureFile);
    A_19025.finish();

    A_19999.start("enrich the task with ExpiryDate and AcceptDate from prescription bundle");
    // A_19445 Part 1 and 4 are in $create
    A_19445_06.start("2. Task.ExpiryDate = <Date of QES Creation + 3 month");
    task->setExpiryDate(model::Timestamp{date::sys_days{signingDay + date::months{3}}});
    A_19445_06.finish();
    A_19445_06.start("3. Task.AcceptDate = <Date of QES Creation + 28 days");
    A_19517_02.start("different validity duration (accept date) for different types");
    const auto compositions = prescriptionBundle.getResourcesByType<model::Composition>("Composition");
    ErpExpect(compositions.size() == 1, HttpStatus::BadRequest,
        "Expected exactly one Composition in prescription bundle, got: " + std::to_string(compositions.size()));
    auto legalBasisCode = compositions[0].legalBasisCode();
    ErpExpect(legalBasisCode.has_value(), HttpStatus::BadRequest, "no legal basis code in composition");
    task->setAcceptDate(*signingTime, *legalBasisCode,
                        config.getIntValue(ConfigurationKey::SERVICE_TASK_ACTIVATE_ENTLASSREZEPT_VALIDITY_WD));
    A_19517_02.finish();
    A_19445_06.finish();
    A_19999.finish();



    A_19127.start("store KVNR from prescription bundle in task");
    const auto patients = prescriptionBundle.getResourcesByType<model::Patient>("Patient");
    ErpExpect(patients.size() == 1, HttpStatus::BadRequest,
              "Expected exactly one Patient in prescription bundle, got: " + std::to_string(patients.size()));
    std::string kvnr;
    try
    {
        kvnr = patients[0].kvnr();
    }
    catch (const model::ModelException& ex)
    {
        ErpFail(HttpStatus::BadRequest, ex.what());
    }
    task->setKvnr(kvnr);
    A_19127.finish();

    A_19128.start("status transition draft -> ready");
    task->setStatus(model::Task::Status::ready);
    A_19128.finish();

    task->updateLastUpdate();

    A_19025.start("2. store the PKCS7 file in database");
    databaseHandle = session.database();
    databaseHandle->activateTask(task.value(), healthCareProviderPrescriptionBinary);
    A_19025.finish();

    makeResponse(session, HttpStatus::OK, &task.value());

    // Collect audit data:
    session.auditDataCollector()
        .setEventId(model::AuditEventId::POST_Task_activate)
        .setInsurantKvnr(kvnr)
        .setAction(model::AuditEvent::Action::update)
        .setPrescriptionId(prescriptionId);
}

CadesBesSignature ActivateTaskHandler::unpackCadesBesSignature(
    const std::string& cadesBesSignatureFile, TslManager& tslManager)
{
    try
    {
        A_19025.start("verify the QES signature");
        A_20159.start("verify HBA Signature Certificate ");
        return {cadesBesSignatureFile,
                tslManager,
                false,
                {profession_oid::oid_arzt,
                 profession_oid::oid_zahnarzt,
                 profession_oid::oid_arztekammern}};
        A_20159.finish();
        A_19025.finish();
    }
    catch (const TslError& ex)
    {
        VauFail(ex.getHttpStatus(), VauErrorCode::invalid_prescription, ex.what());
    }
    catch (const CadesBesSignature::UnexpectedProfessionOidException& ex)
    {
        A_19225.start("Report 400 because of unexpected ProfessionOIDs in QES certificate.");
        VauFail(HttpStatus::BadRequest, VauErrorCode::invalid_prescription, ex.what());
        A_19225.finish();
    }
    catch (const CadesBesSignature::VerificationException& ex)
    {
        VauFail(HttpStatus::BadRequest, VauErrorCode::invalid_prescription, ex.what());
    }
    catch (const ErpException& ex)
    {
        TVLOG(1) << "ErpException: " << ex.what();
        VauFail(ex.status(), VauErrorCode::invalid_prescription, "ErpException");
    }
    catch (const std::exception& ex)
    {
        VauFail(HttpStatus::InternalServerError, VauErrorCode::invalid_prescription,
                ex.what());
    }
    catch (...)
    {
        VauFail(HttpStatus::InternalServerError, VauErrorCode::invalid_prescription,
                "unexpected throwable");
    }
}

void ActivateTaskHandler::checkNarcoticsMatches(const model::KbvBundle& bundle)
{
    A_22231.start(R"(Fehlercode 400 und einem Hinweis auf den Ausschluss von Betäubungsmittel und T-Rezepten)"
                  R"(("BTM und Thalidomid nicht zulässig" im OperationOutcome))");
    const auto& medicationRequests = bundle.getResourcesByType<model::KbvMedicationGeneric>();
    for (const auto& mr : medicationRequests)
    {
        ErpExpect(!mr.isNarcotics(), HttpStatus::BadRequest, "BTM und Thalidomid nicht zulässig");
    }
    A_22231.finish();
}

void ActivateTaskHandler::checkAuthoredOnEqualsSigningDate(const model::KbvBundle& bundle,
                                                           const date::year_month_day& signingDay)
{
    A_22487.start("check equality of signing day and authoredOn");
    const auto& medicationRequests = bundle.getResourcesByType<model::KbvMedicationRequest>();
    for (const auto& mr : medicationRequests)
    {
        ErpExpect(signingDay == date::floor<date::days>(mr.authoredOn().toChronoTimePoint()), HttpStatus::BadRequest,
                  "Ausstellungsdatum und Signaturzeitpunkt weichen voneinander ab, müssen aber taggleich sein");
    }
    A_22487.finish();
}

void ActivateTaskHandler::checkMultiplePrescription(const model::KbvBundle& bundle)
{
    A_22068.start(R"(Fehlercode 400 und einem Hinweis auf den Ausschluss der Mehrfachverorndung)"
                  R"(("Mehrfachverordnung nicht zulässig" im OperationOutcome))");
    const auto& medicationRequests = bundle.getResourcesByType<model::KbvMedicationRequest>();
    for (const auto& mr : medicationRequests)
    {
        ErpExpect(!mr.isMultiplePrescription(), HttpStatus::BadRequest, "Mehrfachverordnung nicht zulässig");
    }
    A_22068.finish();
}

void ActivateTaskHandler::checkValidCoverage(const model::KbvBundle& bundle, const model::PrescriptionType prescriptionType)
{
    A_22222.start("Check for allowed coverage type");
    const auto& config = Configuration::instance();
    const auto& coverage = bundle.getResourcesByType<model::KbvCoverage>("Coverage");
    bool featureWf200enabled = config.featureWf200Enabled();
    bool pkvCovered = false;
    for (const auto& currentCoverage : coverage)
    {
        const auto coverageType = currentCoverage.typeCodingCode();
        pkvCovered = featureWf200enabled && (coverageType == "PKV");
        ErpExpect((coverageType == "GKV")
                  || (coverageType == "SEL")
                  || (coverageType == "BG")
                  || (coverageType == "UK")
                  || pkvCovered,

                  HttpStatus::BadRequest,
                  "Kostenträger nicht zulässig");
    }
    A_22222.finish();
    // PKV related: check PKV coverage
    A_22347.start("Check coverage type for flowtype 200 (PKV prescription)");
    if(featureWf200enabled && prescriptionType == model::PrescriptionType::apothekenpflichtigeArzneimittelPkv)
    {
        ErpExpect(pkvCovered, HttpStatus::BadRequest, "Coverage \"PKV\" not set for flowtype 200");
    }
    A_22347.finish();
}
