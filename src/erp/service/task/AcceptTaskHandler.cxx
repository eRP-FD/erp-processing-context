/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/task/AcceptTaskHandler.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/common/MimeType.hxx"
#include "erp/crypto/SecureRandomGenerator.hxx"
#include "erp/database/Database.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/Bundle.hxx"
#include "erp/model/Consent.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/KbvMedicationRequest.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/extensions/KBVMultiplePrescription.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/TLog.hxx"

#include <date/tz.h>


AcceptTaskHandler::AcceptTaskHandler (const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : TaskHandlerBase(Operation::POST_Task_id_accept, allowedProfessionOiDs)
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

    checkTaskPreconditions(session, task);

    ErpExpect(healthCareProviderPrescription.has_value() && healthCareProviderPrescription->data().has_value(),
              HttpStatus::NotFound, "Healthcare provider prescription not found for prescription id");
    // GEMREQ-end A_19169-01#readFromDB

    const auto cadesBesSignature =
        unpackCadesBesSignatureNoVerify(std::string{*healthCareProviderPrescription->data()});
    const auto& prescription = cadesBesSignature.payload();
    checkMultiplePrescription(model::KbvBundle::fromXmlNoValidation(prescription));

    // GEMREQ-start A_19169-01
    A_19169_01.start("Set status to in-progress, create and set secret");
    task.setStatus(model::Task::Status::inprogress);
    const auto secret = SecureRandomGenerator::generate(32);
    task.setSecret(ByteHelper::toHex(secret));
    task.updateLastUpdate();
    A_19169_01.finish();

    task.setHealthCarePrescriptionUuid();
    databaseHandle->updateTaskStatusAndSecret(task, *taskAndKey->key);

    // Create response:
    const auto linkBase = makeFullUrl("/Task/" + prescriptionId.toString());
    model::Bundle responseBundle(model::BundleType::collection, ::model::ResourceBase::NoProfile);
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


void AcceptTaskHandler::checkTaskPreconditions(const PcSessionContext& session, const model::Task& task)
{
    std::string taskAccessCode;
    try
    {
        taskAccessCode = task.accessCode();
    }
    catch (const model::ModelException&)
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

    A_19168.start("Check if task has correct status");
    if(taskStatus == model::Task::Status::completed ||
       taskStatus == model::Task::Status::inprogress ||
       taskStatus == model::Task::Status::draft)
    {
        ErpFail(HttpStatus::Conflict,
                "Task has invalid status " + std::string(model::Task::StatusNames.at(taskStatus)));
    }
    A_19168.finish();
}

void AcceptTaskHandler::checkMultiplePrescription(const model::KbvBundle& prescription)
{
    A_22635.start("check MVO period start");
    A_23539.start("check MVO period end");
    const auto now = std::chrono::system_clock::now();
    const auto& medicationRequests = prescription.getResourcesByType<model::KbvMedicationRequest>();
    if (!medicationRequests.empty())
    {
        auto mPExt = medicationRequests[0].getExtension<model::KBVMultiplePrescription>();
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
                ss << "Teilverordnung ab " << germanFmtTs << " einlösbar.";
                ErpFail(HttpStatus::Forbidden, ss.str());
            }

            const auto endDate = mPExt->endDateTime();
            if (endDate.has_value())
            {
                using namespace std::chrono_literals;
                auto validUntil =
                    date::make_zoned(model::Timestamp::GermanTimezone, endDate->toChronoTimePoint() + 24h);
                validUntil = floor<date::days>(validUntil.get_local_time());
                if (validUntil.get_sys_time() < now)
                {
                    std::string germanFmtTs = model::Timestamp(endDate->toChronoTimePoint()).toGermanDateFormat();
                    std::ostringstream ss;
                    ss << "Teilverordnung bis " << germanFmtTs << " einlösbar.";
                    ErpFail(HttpStatus::Forbidden, ss.str());
                }
            }
        }
    }
}
