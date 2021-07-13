#include <rapidjson/pointer.h>
#include "erp/service/task/ActivateTaskHandler.hxx"

#include "erp/ErpRequirements.hxx"
#include "erp/database/Database.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/Composition.hxx"
#include "erp/model/Patient.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/Parameters.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/util/TLog.hxx"

#include "erp/util/Uuid.hxx"

ActivateTaskHandler::ActivateTaskHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs)
        : TaskHandlerBase(Operation::POST_Task_id_activate, allowedProfessionOiDs)
{
}

void ActivateTaskHandler::handleRequest (PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();
    TVLOG(2) << "request body is '" << session.request.getBody() << "'";

    const auto prescriptionId = parseId(session.request);

    auto databaseHandle = session.database();
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
    ErpExpect(taskStatus == model::Task::Status::draft, HttpStatus::Forbidden, "Task not in status draft");
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
        try
        {
            auto document = Fhir::instance().converter().xmlStringToJsonWithValidation(
                prescription, session.serviceContext.getXmlValidator(), SchemaType::fhir);
            if (Configuration::instance().getOptionalBoolValue(
                ConfigurationKey::SERVICE_TASK_ACTIVATE_KBV_VALIDATION, true))
            {
                (void) Fhir::instance().converter().xmlStringToJsonWithValidation(
                    prescription, session.serviceContext.getXmlValidator(), SchemaType::KBV_PR_ERP_Bundle);
            }
            return model::Bundle::fromJson(std::move(document));
        }
        catch (const std::runtime_error& er)
        {
            TVLOG(1) << "runtime_error: " << er.what();
            ErpFail(HttpStatus::BadRequest, "runtime_error");
        }
    }();

    const auto signingTime = cadesBesSignature.getSigningTime();
    ErpExpect(signingTime.has_value(), HttpStatus::BadRequest, "No signingTime in PKCS7 file");

    A_19025.start("3. reference the PKCS7 file in task");
    task->setHealthCarePrescriptionUuid();
    task->setPatientConfirmationUuid();
    const model::Binary healthCareProviderPrescriptionBinary(*task->healthCarePrescriptionUuid(), cadesBesSignatureFile);
    A_19025.finish();

    A_19999.start("enrich the task with ExpiryDate and AcceptDate from prescription bundle");
    // A_19445 Part 1 and 4 are in $create
    A_19445.start("2. Task.ExpiryDate = <Date of QES Creation + 92 days");
    task->setExpiryDate(*signingTime + std::chrono::hours(24) * 92);
    A_19445.finish();
    A_19445.start("3. Task.AcceptDate = <Date of QES Creation + 30 days");
    A_19517_01.start("different validity duration (accept date) for different types");
    const auto compositions = prescriptionBundle.getResourcesByType<model::Composition>("Composition");
    ErpExpect(compositions.size() == 1, HttpStatus::BadRequest,
        "Expected exactly one Composition in prescription bundle, got: " + std::to_string(compositions.size()));
    auto legalBasisCode = compositions[0].legalBasisCode();
    ErpExpect(legalBasisCode.has_value(), HttpStatus::BadRequest, "no legal basis code in composition");
    task->setAcceptDate(*signingTime, *legalBasisCode,
                        Configuration::instance().getIntValue(
                            ConfigurationKey::SERVICE_TASK_ACTIVATE_ENTLASSREZEPT_VALIDITY_WD));
    A_19517_01.finish();
    A_19445.finish();
    A_19999.finish();



    A_19127.start("store KVNR from prescription bundle in task");
    const auto patients = prescriptionBundle.getResourcesByType<model::Patient>("Patient");
    ErpExpect(patients.size() == 1, HttpStatus::BadRequest,
              "Expected exactly one Patient in prescription bundle, got: " + std::to_string(patients.size()));
    task->setKvnr(patients[0].getKvnr());
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
        .setInsurantKvnr(patients[0].getKvnr())
        .setAction(model::AuditEvent::Action::update)
        .setPrescriptionId(prescriptionId);
}

CadesBesSignature ActivateTaskHandler::unpackCadesBesSignature(
    const std::string& cadesBesSignatureFile, TslManager* tslManager)
{
    try
    {
        A_19025.start("verify the QES signature");
        A_20159.start("verify HBA Signature Certificate ");
        return CadesBesSignature(cadesBesSignatureFile, tslManager);
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
