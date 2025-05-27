/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/task/AcceptTaskHandler.hxx"
#include "erp/database/Database.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/Task.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/crypto/SecureRandomGenerator.hxx"
#include "shared/crypto/SignedPrescription.hxx"
#include "shared/model/Bundle.hxx"
#include "shared/model/PrescriptionId.hxx"
#include "shared/model/Binary.hxx"
#include "shared/model/KbvBundle.hxx"
#include "shared/model/KbvMedicationRequest.hxx"
#include "shared/model/extensions/KBVMultiplePrescription.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/ByteHelper.hxx"
#include "shared/util/TLog.hxx"

#include <date/tz.h>


AcceptTaskHandler::AcceptTaskHandler(const OIDsByWorkflow& allowedProfessionOiDsByWorkflow)
    : TaskHandlerBase(Operation::POST_Task_id_accept, allowedProfessionOiDsByWorkflow)
{
}

void AcceptTaskHandler::handleRequest (PcSessionContext& session)
{
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();

    const auto prescriptionId = parseId(session.request, session.accessLog);

    TVLOG(1) << "Working on Task for prescription id " << prescriptionId.toString();

    // GEMREQ-start A_19169-01#readFromDB
    auto* databaseHandle = session.database();

    auto [taskAndKey, healthCareProviderPrescription] = databaseHandle->retrieveTaskAndPrescription(prescriptionId);
    ErpExpect(taskAndKey.has_value(), HttpStatus::NotFound, "Task not found for prescription id");
    auto& task = taskAndKey->task;

    // GEMREQ-start A_24174#get-telematikid
    auto telematikId = session.request.getAccessToken().stringForClaim(JWT::idNumberClaim);
    ErpExpect(telematikId.has_value(), HttpStatus::BadRequest, "Missing Telematik-ID in ACCESS_TOKEN");
    // GEMREQ-end A_24174#get-telematikid
    checkTaskPreconditions(session, task, *telematikId);

    ErpExpect(healthCareProviderPrescription.has_value() && healthCareProviderPrescription->data().has_value(),
              HttpStatus::NotFound, "Healthcare provider prescription not found for prescription id");
    // GEMREQ-end A_19169-01#readFromDB

    const auto cadesBesSignature = SignedPrescription::fromBinNoVerify(std::string{*healthCareProviderPrescription->data()});
    const auto& prescription = cadesBesSignature.payload();
    checkMultiplePrescription(session, model::KbvBundle::fromXmlNoValidation(prescription));

    // GEMREQ-start A_19169-01
    A_19169_01.start("Set status to in-progress, create and set secret");
    task.setStatus(model::Task::Status::inprogress);
    const auto secret = SecureRandomGenerator::generate(32);
    task.setSecret(ByteHelper::toHex(secret));

    // GEMREQ-start A_24174#store-telematikid
    A_24174.start("store Task.owner");
    task.setOwner(*telematikId),
    A_24174.finish();
    // GEMREQ-end A_24174#store-telematikid

    task.updateLastUpdate();
    A_19169_01.finish();

    task.setHealthCarePrescriptionUuid();
    databaseHandle->updateTaskStatusAndSecret(task, *taskAndKey->key);

    // Create response:
    const auto linkBase = makeFullUrl("/Task/" + prescriptionId.toString());
    model::Bundle responseBundle(model::BundleType::collection, ::model::FhirResourceBase::NoProfile);
    responseBundle.setLink(model::Link::Type::Self, linkBase + "/$accept/");
    responseBundle.addResource(linkBase, {}, {}, task.jsonDocument());
    std::string uuid{};
    if (healthCareProviderPrescription->id().has_value())
    {
        uuid = Uuid{healthCareProviderPrescription->id().value()}.toUrn();
    }
    responseBundle.addResource(uuid, {}, {}, healthCareProviderPrescription->jsonDocument());
    // GEMREQ-end A_19169-01

    const auto kvnr = task.kvnr();
    Expect3(kvnr.has_value(), "Task has no KV number", std::logic_error);

    switch (prescriptionId.type())
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
        case model::PrescriptionType::direkteZuweisung:
        case model::PrescriptionType::digitaleGesundheitsanwendungen:
            break;
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case model::PrescriptionType::direkteZuweisungPkv: {
            A_22110.start("Evaluate consent for flowtype 200/209 (PKV prescription) and add to response bundle");
            const auto consent = databaseHandle->retrieveConsent(kvnr.value());
            if (consent.has_value())
            {
                TVLOG(1) << "Consent found for Kvnr of task";
                Expect3(consent->id().has_value(), "Consent id not set", std::logic_error);
                responseBundle.addResource(makeFullUrl("/Consent/" + std::string(consent->id().value())), {}, {},
                                           consent->jsonDocument());
            }
            A_22110.finish();
            break;
        }
    }

    A_19514.start("HttpStatus 200 for successful POST");
    makeResponse(session, HttpStatus::OK, &responseBundle);
    A_19514.finish();

    // Collect Audit data
    session.auditDataCollector()
        .setEventId(model::AuditEventId::POST_Task_accept)
        .setInsurantKvnr(kvnr.value())
        .setAction(model::AuditEvent::Action::update)
        .setPrescriptionId(prescriptionId);
}


void AcceptTaskHandler::checkTaskPreconditions(const PcSessionContext& session, const model::Task& task,
                                               const std::string& telematikId)
{
    std::string taskAccessCode;
    try
    {
        taskAccessCode = task.accessCode();
    }
    catch (const model::ModelException&) // NOLINT(*-empty-catch)
    {
        // handled by taskAccessCode.empty() below.
    }

    const auto taskStatus = task.status();

    A_19149_02.start("Check if task meanwhile deleted");
    if(taskAccessCode.empty() ||
       taskStatus == model::Task::Status::cancelled)
    {
        ErpFail(HttpStatus::Gone, "Task meanwhile deleted for prescription id");
    }
    A_19149_02.finish();

    A_19167_04.start("Check if access code from URL is equal to access code from task");
    checkAccessCodeMatches(session.request, task);
    A_19167_04.finish();
    A_19168_01.start("Check if task has correct status");
    switch (taskStatus)
    {
        case model::Task::Status::inprogress:
            if (task.owner() == telematikId)
            {
                ErpFail2(HttpStatus::Conflict,
                         "Task has invalid status " + std::string(model::Task::StatusNames.at(taskStatus)),
                         "Task is processed by requesting institution");
            }
            else
            {
                [[fallthrough]];
            }
        case model::Task::Status::draft:
        case model::Task::Status::completed:
            ErpFail(HttpStatus::Conflict,
                    "Task has invalid status " + std::string(model::Task::StatusNames.at(taskStatus)));
        case model::Task::Status::ready:
        case model::Task::Status::cancelled:
            break;
    }
    A_19168_01.finish();

    A_23539_01.start("Check task expiry date");
    const auto now = std::chrono::system_clock::now();
    const auto expiryDate = task.expiryDate();
    using namespace std::chrono_literals;
    auto validUntil =
        date::make_zoned(model::Timestamp::GermanTimezone, expiryDate.toChronoTimePoint() + 24h);
    validUntil = floor<date::days>(validUntil.get_local_time());
    if (validUntil.get_sys_time() < now)
    {
        ErpFail(HttpStatus::Forbidden, "Verordnung bis " + expiryDate.toGermanDateFormat() + " einlösbar.");
    }
    A_23539_01.finish();
}

void AcceptTaskHandler::checkMultiplePrescription(PcSessionContext& session, const model::KbvBundle& prescription)
{
    A_22635_02.start("check MVO period start");
    const auto now = std::chrono::system_clock::now();
    const auto& medicationRequests = prescription.getResourcesByType<model::KbvMedicationRequest>();
    if (!medicationRequests.empty())
    {
        auto mPExt = medicationRequests[0].getExtension<model::KBVMultiplePrescription>();
        session.fillMvoBdeV2(mPExt);
        if (mPExt && mPExt->isMultiplePrescription())
        {
            const auto startDate = mPExt->startDateTime();
            ErpExpect(startDate.has_value(), HttpStatus::InternalServerError,
                      "MedicationRequest.extension:Mehrfachverordnung.extension:Zeitraum.value[x]:valuePeriod.start "
                      "not present");
            auto validFrom =
                    date::make_zoned(model::Timestamp::GermanTimezone, startDate->toChronoTimePoint());
            validFrom = floor<date::days>(validFrom.get_local_time());
            if (now < validFrom.get_sys_time())
            {
                std::string germanFmtTs = model::Timestamp(startDate->toChronoTimePoint()).toGermanDateFormat();
                std::ostringstream ss;
                ss << "Teilverordnung zur Mehrfachverordnung " << mPExt->mvoId().value_or("<unknown>") << " ist ab "
                   << germanFmtTs << " einlösbar.";
                ErpFail(HttpStatus::Forbidden, ss.str());
            }
        }
    }
}
