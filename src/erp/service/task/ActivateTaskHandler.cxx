/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/service/task/ActivateTaskHandler.hxx"
#include "erp/database/Database.hxx"
#include "erp/model/EvdgaBundle.hxx"
#include "erp/model/KbvPractitioner.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/WorkflowParameters.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/crypto/SignedPrescription.hxx"
#include "shared/model/Binary.hxx"
#include "shared/model/Composition.hxx"
#include "shared/model/KbvBundle.hxx"
#include "shared/model/KbvCoverage.hxx"
#include "shared/model/KbvMedicationBase.hxx"
#include "shared/model/KbvMedicationCompounding.hxx"
#include "shared/model/KbvMedicationRequest.hxx"
#include "shared/model/Patient.hxx"
#include "shared/model/extensions/KBVMultiplePrescription.hxx"
#include "shared/server/response/ServerResponse.hxx"
#include "shared/tsl/error/TslError.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/TLog.hxx"
#include "shared/util/Uuid.hxx"

#include <date/date.h>
#include <date/tz.h>
#include <ranges>


ActivateTaskHandler::ActivateTaskHandler(const std::initializer_list<std::string_view>& allowedProfessionOiDs)
    : TaskHandlerBase(Operation::POST_Task_id_activate, allowedProfessionOiDs)
{
}

void ActivateTaskHandler::handleRequest(PcSessionContext& session)
{
    // GEMREQ-start A_19716#sampleUsage
    TVLOG(1) << name() << ": processing request to " << session.request.header().target();
    TVLOG(2) << "request body is '" << session.request.getBody() << "'";
    // GEMREQ-end A_19716#sampleUsage

    const auto prescriptionId = parseId(session.request, session.accessLog);

    auto* databaseHandle = session.database();
    auto taskAndKey = databaseHandle->retrieveTaskForUpdate(prescriptionId);

    ErpExpect(taskAndKey.has_value(), HttpStatus::NotFound, "Requested Task not found in DB");
    auto& task = taskAndKey->task;
    // the prescription ID is not persisted in $create in order to avoid a second DB access there.
    task.setPrescriptionId(prescriptionId);

    const auto taskStatus = task.status();
    ErpExpect(taskStatus != model::Task::Status::cancelled, HttpStatus::Gone, "Task has already been deleted");

    A_19024_03.start("check access code");
    checkAccessCodeMatches(session.request, task);
    A_19024_03.finish();

    A_19024_03.start("check status draft");
    ErpExpect(taskStatus == model::Task::Status::draft, HttpStatus::Forbidden,
              "Task not in status draft but in status " + std::string(model::Task::StatusNames.at(taskStatus)));
    A_19024_03.finish();

    const auto parameterResource = parseAndValidateRequestBody<model::ActivateTaskParameters>(session);
    ErpExpect(parameterResource.count() == 1, HttpStatus::BadRequest, "unexpected number of parameters");
    const auto* ePrescriptionValue = parameterResource.findResourceParameter("ePrescription");
    ErpExpect(ePrescriptionValue != nullptr, HttpStatus::BadRequest, "Failed to get ePrescription from requestBody");
    const auto& ePrescriptionParameter = model::Binary::fromJson(*ePrescriptionValue);
    const auto binaryData = ePrescriptionParameter.data();
    ErpExpect(binaryData.has_value(), HttpStatus::BadRequest, "Failed to get ePrescription. Document has no data.");
    const std::string cadesBesSignatureFile{binaryData.value()};

    A_19020.start("check PKCS#7 for structural validity");
    A_20704.start("Set VAU-Error-Code header field to invalid_prescription when an invalid "
                  "prescription has been transmitted");
    // GEMREQ-start A_20159-04#fromBinCall
    const auto cadesBesSignature =
        SignedPrescription::fromBin(cadesBesSignatureFile, session.serviceContext.getTslManager(),
                                    allowedProfessionOidsForQesSignature(task.type()));
    // GEMREQ-end A_20159-04#fromBinCall
    A_20704.finish();
    A_19020.finish();


    const auto signingAlgorithm = cadesBesSignature.getSigningAlgorithm();
    JsonLog(LogId::INFO, JsonLog::makeInfoLogReceiver(), false)
        .keyValue("issuer", cadesBesSignature.getIssuer())
        .keyValue("alg", to_string(signingAlgorithm));


    switch (task.type())
    {
        case model::PrescriptionType::digitaleGesundheitsanwendungen:
            handleDigaRequest(session, *taskAndKey, cadesBesSignature);
            return;
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
        case model::PrescriptionType::direkteZuweisung:
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case model::PrescriptionType::direkteZuweisungPkv:
        case model::PrescriptionType::tRezept:
            handlePrescriptionRequest(session, *taskAndKey, cadesBesSignature);
            return;
    }
    ErpFail(HttpStatus::BadRequest, "Invalid task type: " + std::to_string(static_cast<uintmax_t>(task.type())));
}

void ActivateTaskHandler::handlePrescriptionRequest(PcSessionContext& session, Database::TaskAndKey& taskAndKey,
                                                    const SignedPrescription& cadesBesSignature)
{
    const auto& prescription = cadesBesSignature.payload();

    auto prescriptionBundle =
        prescriptionBundleFromXml<model::KbvBundle>(session, prescription);

    ErpExpect(! isTRezept(taskAndKey.task.prescriptionId().type()) ||
                  prescriptionBundle.getProfileVersionChecked() >= model::version::KBV_1_4,
              HttpStatus::BadRequest, "T-Rezept Workflow 166 not allowed before KBV-1.4");

    const auto compositions = prescriptionBundle.getResourcesByType<model::Composition>("Composition");
    ErpExpect(compositions.size() == 1, HttpStatus::BadRequest,
              "Expected exactly one Composition in prescription bundle, got: " + std::to_string(compositions.size()));
    auto legalBasisCode = compositions[0].legalBasisCode();

    const auto& medicationRequests = prescriptionBundle.getResourcesByType<model::KbvMedicationRequest>();
    ErpExpect(medicationRequests.size() == 1, HttpStatus::BadRequest,
              "Unexpected number of MedicationRequests in prescription bundle: " +
                  std::to_string(medicationRequests.size()));
    const auto& medicationRequest = medicationRequests[0];
    const bool isMvo = medicationRequest.isMultiplePrescription();
    ErpExpect(!(isMvo && isTRezept(taskAndKey.task.type())), HttpStatus::BadRequest, "T-Rezept cannot be MVO");
    std::optional<date::year_month_day> mvoEndDate = medicationRequest.mvoEndDate();
    const auto mvoExtension = medicationRequest.getExtension<model::KBVMultiplePrescription>();
    session.fillMvoBdeV2(mvoExtension);
    auto& task = taskAndKey.task;
    checkMultiplePrescription(mvoExtension, task.type(), legalBasisCode, medicationRequest.authoredOn());

    A_22231_01.start("check BTM");
    A_27813.start("check T-Rezept");
    checkMedicationCategoryCode(prescriptionBundle, task.type());
    A_27813.finish();
    A_22231_01.finish();

    if (isMvo)
    {
        const auto signingTime = cadesBesSignature.getSigningTime();
        ErpExpect(signingTime.has_value(), HttpStatus::BadRequest, "No signingTime in PKCS7 file");
        date::year_month_day signingDay{signingTime->localDay()};
        setMvoExpiryAcceptDates(task, mvoEndDate, signingDay);
    }

    handleGeneric(session, taskAndKey, cadesBesSignature, prescriptionBundle, isMvo, legalBasisCode);
}

template<typename KbvOrEvdgaBundle>
void ActivateTaskHandler::handleGeneric(PcSessionContext& session, Database::TaskAndKey& taskAndKey,
                                        const SignedPrescription& cadesBesSignature,
                                        const KbvOrEvdgaBundle& kbvOrEvdgaBundle, bool isMvo,
                                        std::optional<model::KbvStatusKennzeichen> legalBasisCode)
{
    auto& task = taskAndKey.task;
    checkBundlePrescriptionId(task, kbvOrEvdgaBundle);

    const auto signingTime = cadesBesSignature.getSigningTime();
    ErpExpect(signingTime.has_value(), HttpStatus::BadRequest, "No signingTime in PKCS7 file");

    try
    {
        checkAuthoredOnEqualsSigningDate(kbvOrEvdgaBundle, *signingTime);
    }
    catch (const model::ModelException& m)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "error checking authoredOn==signature date", m.what());
    }

    const std::vector<model::KbvCoverage> coverage =
        kbvOrEvdgaBundle.template getResourcesByType<model::KbvCoverage>("Coverage");
    ErpExpect(coverage.size() == 1, HttpStatus::BadRequest,
              "Unexpected number of Coverage Resources in KBV/EVDGA Bundle");
    checkValidCoverage(coverage.front(), task.type());

    A_22731_01.start("store Coverage.type.coding.code in task for later usage");
    task.setIsPkv(isPkvCoverage(coverage.front()));
    A_22731_01.finish();

    HttpStatus responseStatus = HttpStatus::OK;

    if (! checkPractitioner(kbvOrEvdgaBundle, session))
    {
        A_24031.start("Use configuration value for ANR handling");
        const auto* errorMessage = "Ungültige Arztnummer (LANR oder ZANR): Die übergebene Arztnummer entspricht nicht "
                                   "den Prüfziffer-Validierungsregeln.";
        const auto& config = Configuration::instance();
        switch (config.anrChecksumValidationMode())
        {
            case Configuration::AnrChecksumValidationMode::warning:
                A_24033.start("Return a warning when ANR validation fails");
                session.response.addWarningHeader(252, "erp-server", errorMessage);
                responseStatus = HttpStatus::OkWarnInvalidAnr;
                A_24033.finish();
                break;
            case Configuration::AnrChecksumValidationMode::error:
                A_24032.start("Return an error when ANR validation fails");
                ErpFail(HttpStatus::BadRequest, errorMessage);
                A_24032.finish();
                break;
        }
        A_24031.finish();
    }

    A_19025_03.start("3. reference the PKCS7 file in task");
    task.setHealthCarePrescriptionUuid();
    task.setPatientConfirmationUuid();
    const model::Binary healthCareProviderPrescriptionBinary(*task.healthCarePrescriptionUuid(),
                                                             cadesBesSignature.getBase64());
    A_19025_03.finish();

    A_19999.start("enrich the task with ExpiryDate and AcceptDate from prescription bundle");
    date::year_month_day signingDay{signingTime->localDay()};
    const auto& config = Configuration::instance();
    if (isTRezept(task.type()))
    {
        A_27846.start("FlowType 166 Expiry Date and Accept Date");
        task.setExpiryDate(*signingTime + (std::chrono::days{6}));
        task.setAcceptDate(*signingTime, legalBasisCode,
                   config.getIntValue(ConfigurationKey::SERVICE_TASK_ACTIVATE_ENTLASSREZEPT_VALIDITY_WD));
        A_27846.finish();
    }
    else if (! isMvo)
    {
        // A_19445 Part 1 and 4 are in $create
        A_27844.start("FlowType 160 Expiry Date");
        A_27845.start("FlowType 162 Expiry Date");
        A_27847.start("FlowType 169 Expiry Date");
        A_27848.start("FlowType 200 Expiry Date");
        A_27849.start("FlowType 209 Expiry Date");
        auto expiryDate = signingDay + date::months{3};
        if (! expiryDate.ok())
        {
            expiryDate = expiryDate.year() / expiryDate.month() / date::last;
        }
        task.setExpiryDate(model::Timestamp{date::sys_days{expiryDate}});
        A_27844.finish();
        A_27845.finish();
        A_27847.finish();
        A_27848.finish();
        A_27849.finish();
        A_19517_04.start("different validity duration (accept date) for different types");
        task.setAcceptDate(*signingTime, legalBasisCode,
                           config.getIntValue(ConfigurationKey::SERVICE_TASK_ACTIVATE_ENTLASSREZEPT_VALIDITY_WD));
        A_19517_04.finish();
    }
    A_19999.finish();

    // GEMREQ-start A_19127
    A_19127_03.start("store KVNR from prescription bundle in task");
    const auto kvnr = getKvnrFromPatientResource(kbvOrEvdgaBundle);
    task.setKvnr(kvnr);
    A_19127_03.finish();
    // GEMREQ-end A_19127

    A_19128.start("status transition draft -> ready");
    task.setStatus(model::Task::Status::ready);
    A_19128.finish();

    A_27768.start("Bestimmung der Einlösbarkeit im EU-Ausland");
    task.setEuRedeemableByProperties(isPrescriptionEuRedeemable(task, kbvOrEvdgaBundle));
    A_27768.finish();

    task.updateLastUpdate();

    A_19025_03.start("2. store the PKCS7 file in database");
    auto* databaseHandle = session.database();
    ErpExpect(taskAndKey.key.has_value(), HttpStatus::InternalServerError, "Missing task key.");
    databaseHandle->activateTask(task, *taskAndKey.key, healthCareProviderPrescriptionBinary,
                                 session.request.getAccessToken());
    A_19025_03.finish();

    if (!Configuration::instance().getBoolValue(ConfigurationKey::FEATURE_EU))
    {
        task.deleteEuRedeemableByProperties();
    }

    makeResponse(session, responseStatus, &task);

    // Collect audit data:
    session.auditDataCollector()
        .setEventId(model::AuditEventId::POST_Task_activate)
        .setInsurantKvnr(kvnr)
        .setAction(model::AuditEvent::Action::update)
        .setPrescriptionId(task.prescriptionId());
}

void ActivateTaskHandler::handleDigaRequest(PcSessionContext& session, Database::TaskAndKey& taskAndKey,
                                            const SignedPrescription& cadesBesSignature)
{
    const auto& evdgaPayload = cadesBesSignature.payload();

    auto evdgaBundle = prescriptionBundleFromXml<model::EvdgaBundle>(session, evdgaPayload);

    handleGeneric(session, taskAndKey, cadesBesSignature, evdgaBundle, false, std::nullopt);
}

void ActivateTaskHandler::setMvoExpiryAcceptDates(model::Task& task,
                                                  const std::optional<date::year_month_day>& mvoEndDate,
                                                  const date::year_month_day& signingDay)
{
    A_27844.start("FlowType 160 ExpiryDate und AcceptDate for MVO");
    A_27846.start("FlowType 166 ExpiryDate und AcceptDate for MVO");
    A_27847.start("FlowType 169 ExpiryDate und AcceptDate for MVO");
    A_27848.start("FlowType 200 ExpiryDate und AcceptDate for MVO");
    A_27849.start("FlowType 209 ExpiryDate und AcceptDate for MVO");
    if (mvoEndDate)
    {
        task.setExpiryDate(model::Timestamp{date::sys_days{*mvoEndDate}});
        task.setAcceptDate(model::Timestamp{date::sys_days{*mvoEndDate}});
    }
    else
    {
        task.setExpiryDate(model::Timestamp{date::sys_days{signingDay} + date::days{365}});
        task.setAcceptDate(model::Timestamp{date::sys_days{signingDay} + date::days{365}});
    }
}


void ActivateTaskHandler::checkMedicationCategoryCode(const model::KbvBundle& bundle, model::PrescriptionType workflow)
{
    const auto& medicationRequests = bundle.getResourcesByType<model::KbvMedicationGeneric>();
    for (const auto& mr : medicationRequests)
    {
        auto medicationCategoryCode = mr.getCategoryCode();
        A_22231_01.start(R"(Fehlercode 400 und einem Hinweis auf den Ausschluss von Betäubungsmittel )");
        ErpExpect(! isBTM(medicationCategoryCode), HttpStatus::BadRequest, "BTM nicht zulässig");
        A_22231_01.finish();
        switch (workflow)
        {
            case model::PrescriptionType::apothekenpflichigeArzneimittel:
            case model::PrescriptionType::direkteZuweisung:
            case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
            case model::PrescriptionType::direkteZuweisungPkv:
                A_25642.start("Flowtype 160/169/200/209 - Prüfung Arzneimittelverordnung");
                ErpExpect(isArzneimittelverordnung(medicationCategoryCode), HttpStatus::BadRequest,
                          "Für diesen Workflowtypen sind nur Arzneimittelverordnungen zulässig");
                A_25642.finish();
                break;
            case model::PrescriptionType::digitaleGesundheitsanwendungen:
                break;
            case model::PrescriptionType::tRezept:
                A_27813.start("Für EF 166 sind nur T-Rezept Verordnungen zulässig");
                ErpExpect(isTRezept(medicationCategoryCode), HttpStatus::BadRequest,
                          "Für diesen Workflowtypen sind nur T-Rezept Verordnungen zulässig");
                A_27813.finish();
                break;
        }
    }
}

template<typename KbvOrEvdgaBundle>
void ActivateTaskHandler::checkAuthoredOnEqualsSigningDate(const KbvOrEvdgaBundle& kbvOrEvdgaBundle,
                                                           const model::Timestamp& signingTime)
{
    A_22487.start("check equality of signing day and authoredOn");
    // using German timezone was decided, but sadly did not make it into the requirement
    // date::local_days is a date not bound to any timezone.
    const date::local_days signingDay{signingTime.localDay()};
    const date::local_days authoredOn{kbvOrEvdgaBundle.authoredOn().localDay()};

    if (signingDay != authoredOn)
    {
        std::ostringstream oss;
        oss << "KBVBundle.signature.signingDay=" << signingDay
            << " != KBVBundle/EVDGABundle.MedicationRequest/DeviceRequest.authoredOn=" << authoredOn;
        TVLOG(1) << oss.str();
        ErpFailWithDiagnostics(
            HttpStatus::BadRequest,
            "Ausstellungsdatum und Signaturzeitpunkt weichen voneinander ab, müssen aber taggleich sein", oss.str());
    }
    A_22487.finish();
}

template<typename KbvOrEvdgaBundle>
KbvOrEvdgaBundle ActivateTaskHandler::prescriptionBundleFromXml(SessionContext& sessionContext,
                                                                std::string_view prescription)
{
    using KbvBundleFactory = model::ResourceFactory<KbvOrEvdgaBundle>;
    const auto& xmlValidator = sessionContext.serviceContext.getXmlValidator();
    try
    {
        auto factory = KbvBundleFactory::fromXml(prescription, xmlValidator);

        const auto header = Header::profileVersionHeader(KbvOrEvdgaBundle::profileType);
        if (! header.empty())
        {
            if (const auto profileVersion = factory.profileVersion())
            {
                sessionContext.addOuterResponseHeaderField(header, to_string(*profileVersion));
            }
        }

        return std::move(factory).getValidated(KbvOrEvdgaBundle::profileType);
    }
    catch (const model::ModelException& er)
    {
        TVLOG(1) << "runtime_error: " << er.what();
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "parsing / validation error", er.what());
    }
}

void ActivateTaskHandler::checkMultiplePrescription(const std::optional<model::KBVMultiplePrescription>& mPExt,
                                                    const model::PrescriptionType prescriptionType,
                                                    std::optional<model::KbvStatusKennzeichen> legalBasisCode,
                                                    const model::Timestamp& authoredOn)
{
    if (! mPExt)
    {
        return;
    }
    if (mPExt->isMultiplePrescription())
    {
        A_24901.start("validate MVO-ID");
        const auto mvoId = mPExt->mvoId();
        bool mvoIdValid = false;
        if (mvoId.has_value())
        {
            mvoIdValid = Uuid::isValidUrnUuid(*mvoId, true);
        }
        ErpExpectWithDiagnostics(
            mvoIdValid, HttpStatus::BadRequest, "MVO-ID entspricht nicht urn:uuid format",
            "urn:uuid format pattern: urn:uuid:<XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX>, found: " +
                std::string{mvoId.value_or("")});
        A_24901.finish();

        A_23164.start("Fehlercode 400 wenn das Ende der Einlösefrist vor dem Beginn liegt.");
        if (mPExt->endDate().has_value())
        {
            ErpExpect(mPExt->startDate() <= mPExt->endDate(), HttpStatus::BadRequest,
                      "Ende der Einlösefrist liegt vor Beginn der Einlösefrist");
        }
        A_23164.finish();
        A_22627_01.start("Fehlercode 400 wenn Flowtype ungleich 160, 169, 200 oder 209");
        switch (prescriptionType)
        {
            case model::PrescriptionType::apothekenpflichigeArzneimittel:
            case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
            case model::PrescriptionType::direkteZuweisung:
            case model::PrescriptionType::direkteZuweisungPkv:
            case model::PrescriptionType::tRezept:
                break;
            case model::PrescriptionType::digitaleGesundheitsanwendungen:
                ErpFailWithDiagnostics(
                    HttpStatus::BadRequest,
                    "Mehrfachverordnungen nur für die Verordnungen von apothekenpflichtigen Arzneimittel zulässig",
                    "Invalid FlowType " + std::string(model::PrescriptionTypeDisplay.at(prescriptionType)) +
                        " for MVO");
                break;
        }
        A_22627_01.finish();

        auto numerator = mPExt->numerator().value_or(0);
        auto denominator = mPExt->denominator().value_or(0);

        A_22628.start("Fehlercode 400 wenn Numerator oder Denominator größer als 4 ist");
        ErpExpect(numerator <= 4 && denominator <= 4, HttpStatus::BadRequest,
                  "Eine Mehrfachverordnungen darf aus maximal 4 Teilverordnungen bestehen");
        A_22628.finish();

        A_22704.start("Fehlercode 400 wenn Numerator kleiner als 1 ist");
        ErpExpect(numerator > 0, HttpStatus::BadRequest,
                  "Für eine Mehrfachverordnungen muss der numerator größer 0 sein.");
        A_22704.finish();

        A_22629.start("Fehlercode 400 wenn Denominator kleiner als 2 ist");
        ErpExpect(denominator > 1, HttpStatus::BadRequest,
                  "Eine Mehrfachverordnung muss aus mindestens 2 Teilverordnungen bestehen");
        A_22629.finish();

        A_22630.start("Fehlercode 400 wenn Numerator größer als der Denominator");
        ErpExpect(numerator <= denominator, HttpStatus::BadRequest, "Numerator ist größer als denominator");
        A_22630.finish();

        if (legalBasisCode.has_value())
        {
            switch (*legalBasisCode)
            {
                case model::KbvStatusKennzeichen::ohneErsatzverordnungskennzeichen:
                case model::KbvStatusKennzeichen::asvKennzeichen:
                case model::KbvStatusKennzeichen::tssKennzeichen:
                    break;
                case model::KbvStatusKennzeichen::nurErsatzverordnungsKennzeichen:
                case model::KbvStatusKennzeichen::asvKennzeichenMitErsatzverordnungskennzeichen:
                case model::KbvStatusKennzeichen::tssKennzeichenMitErsatzverordungskennzeichen:
                    A_22633.start("Fehlercode 400 wenn Ersatzverordnung");
                    ErpFail(HttpStatus::BadRequest, "Für Ersatzverordnung sind keine Mehrfachverordnungen zulässig");
                    A_22633.finish();
                    break;
                case model::KbvStatusKennzeichen::entlassmanagementKennzeichen:
                case model::KbvStatusKennzeichen::entlassmanagementKennzeichenMitErsatzverordungskennzeichen:
                    A_22632.start("Fehlercode 400 wenn Entlassrezept");
                    ErpFail(HttpStatus::BadRequest, "Für Entlassrezepte sind keine Mehrfachverordnungen zulässig");
                    A_22632.finish();
                    break;
            }
        }

        A_22634.start("Fehlercode 400 wenn Beginn der Einlösefrist nicht angegeben ist");
        ErpExpect(mPExt->startDate().has_value(), HttpStatus::BadRequest,
                  "Beginn der Einlösefrist für MVO ist erforderlich");
        A_22634.finish();

        A_23537.start("Fehlercode 400 wenn Beginn der Einlösefrist vor Austellungsdatum");
        ErpExpect(mPExt->startDate().value() >= authoredOn, HttpStatus::BadRequest,
                  "Einlösefrist liegt zeitlich vor dem Ausstellungsdatum");
        A_23537.finish();
    }
    else// if (mPExt->isMultiplePrescription())
    {
        A_22631.start(
            "Fehlercode 400 wenn Kennzeichen = false, aber eine Extension Nummerierung oder Zeitraum vorhanden");
        ErpExpect(! mPExt->getExtension<model::KBVMultiplePrescription::Nummerierung>().has_value(),
                  HttpStatus::BadRequest, "Nummerierung darf nur bei MVO angegeben werden");
        ErpExpect(! mPExt->getExtension<model::KBVMultiplePrescription::Zeitraum>().has_value(), HttpStatus::BadRequest,
                  "Zeitraum darf nur bei MVO angegeben werden");
        A_22631.finish();
    }
}

void ActivateTaskHandler::checkValidCoverage(const model::KbvCoverage& coverage,
                                             const model::PrescriptionType prescriptionType)
{
    bool pkvCovered = isPkvCoverage(coverage);
    // PKV related: check PKV coverage
    A_22347_01.start("Check coverage type for flowtype 200/209 (PKV prescription)");
    A_23443_01.start("Check coverage type for flowtype 160/162/169");
    switch (prescriptionType)
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
        case model::PrescriptionType::digitaleGesundheitsanwendungen:
        case model::PrescriptionType::direkteZuweisung:
            ErpExpect(! pkvCovered, HttpStatus::BadRequest, "Coverage \"PKV\" not allowed for flowtype 160/162/169");
            break;
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case model::PrescriptionType::direkteZuweisungPkv:
            ErpExpect(pkvCovered, HttpStatus::BadRequest, "Coverage \"PKV\" not set for flowtype 200/209");
            break;
        case model::PrescriptionType::tRezept:
            break;
    }
    A_23443_01.finish();
    A_22347_01.finish();
}

bool ActivateTaskHandler::isPkvCoverage(const model::KbvCoverage& coverage)
{
    const auto coverageType = coverage.typeCodingCode();
    return coverageType == "PKV";
}

template<typename KbvOrEvdgaBundle>
bool ActivateTaskHandler::checkPractitioner(const KbvOrEvdgaBundle& bundle, PcSessionContext& session)
{
    const std::vector<model::KbvPractitioner> kbvPractitioners =
        bundle.template getResourcesByType<model::KbvPractitioner>();
    for (const auto& practitioner : kbvPractitioners)
    {
        A_23891.start("Validate Practitioner Checksum");
        auto anr = practitioner.anr();
        if (anr.has_value() && ! anr->validChecksum())
        {
            A_23090_07.start("\"anr\": $anrvalue: Der Wert des Feldes identifier:ANR.value bei aufgetretenem "
                             "Prüfungsfehler gem. A_24032, Datentyp Integer");
            session.addOuterResponseHeaderField(Header::ANR, anr->id());
            A_23090_07.finish();
            return false;
        }

        auto zanr = practitioner.zanr();
        if (zanr.has_value() && ! zanr->validChecksum())
        {
            A_23090_07.start("\"zanr\": $zanrvalue: Der Wert des Feldes identifier:ZANR.value bei aufgetretenem "
                             "Prüfungsfehler gem. A_24032, Datentyp Integer");
            session.addOuterResponseHeaderField(Header::ZANR, zanr->id());
            A_23090_07.finish();
            return false;
        }
        A_23891.finish();
    }
    return true;
}

// GEMREQ-start A_19127#getKvnrFromPatientResource
template<typename KbvOrEvdgaBundle>
model::Kvnr ActivateTaskHandler::getKvnrFromPatientResource(const KbvOrEvdgaBundle& bundle)
{
    const std::vector<model::Patient> patients = bundle.template getResourcesByType<model::Patient>("Patient");
    ErpExpect(patients.size() == 1, HttpStatus::BadRequest,
              "Expected exactly one Patient in prescription bundle, got: " + std::to_string(patients.size()));
    try
    {
        auto kvnr = patients[0].kvnr();
        A_23890_01.start("Validate Kvnr Checksum");
        ErpExpect(kvnr.validChecksum(), HttpStatus::BadRequest,
                  "Ungültige Versichertennummer (KVNR): Die übergebene Versichertennummer des Patienten entspricht "
                  "nicht den Prüfziffer-Validierungsregeln.");
        A_23890_01.finish();
        return kvnr;
    }
    catch (const model::ModelException& ex)
    {
        ErpFail(HttpStatus::BadRequest, ex.what());
    }
    ErpFail(HttpStatus::BadRequest, "Failed to get KVNR from Bundle.entry:Patient");
}
// GEMREQ-end A_19127#getKvnrFromPatientResource

template<typename KbvOrEvdgaBundle>
void ActivateTaskHandler::checkBundlePrescriptionId(const model::Task& task, const KbvOrEvdgaBundle& bundle)
{
    std::optional<model::PrescriptionId> bundlePrescriptionId;
    try
    {
        bundlePrescriptionId.emplace(bundle.getIdentifier());
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "Error while extracting prescriptionId from QES-Bundle",
                               ex.what());
    }
    ErpExpect(bundlePrescriptionId.has_value(), HttpStatus::BadRequest,
              "Failed to extract prescriptionId from QES-Bundle");

    A_21370.start("compare the task flowtype against the prefix of the prescription-id");
    ErpExpect(bundlePrescriptionId->type() == task.type(), HttpStatus::BadRequest,
              "Flowtype mismatch between Task and QES-Bundle");
    A_21370.finish();

    const auto& config = Configuration::instance();
    if (! config.getOptionalBoolValue(ConfigurationKey::DEBUG_DISABLE_QES_ID_CHECK, false))
    {
        A_21370.start("compare the prescription id of the QES bundle with the task");
        ErpExpect(*bundlePrescriptionId == task.prescriptionId(), HttpStatus::BadRequest,
                  "PrescriptionId mismatch between Task and QES-Bundle");
        A_21370.finish();
    }
    else
    {
        TLOG(ERROR) << "PrescriptionId check of QES-Bundle is disabled";
    }
}

// GEMREQ-start A_20159-04#allowedProfessionOidsForQesSignature
std::vector<std::string_view>
ActivateTaskHandler::allowedProfessionOidsForQesSignature(model::PrescriptionType prescriptionType)
{
    A_19225_02.start("Return professionOIDs allowed for QES Signature - Flowtype 160/169/200/209");
    A_25990.start("Return professionOIDs allowed for QES Signature - Flowtype 162");
    A_27812.start("Return professionOIDs allowed for QES Signature - Flowtype 166");
    switch (prescriptionType)
    {
        case model::PrescriptionType::digitaleGesundheitsanwendungen:
            return {profession_oid::oid_arzt, profession_oid::oid_zahnarzt, profession_oid::oid_psychotherapeut,
                    profession_oid::oid_ps_psychotherapeut, profession_oid::oid_kuj_psychotherapeut};
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
        case model::PrescriptionType::direkteZuweisung:
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case model::PrescriptionType::direkteZuweisungPkv:
            return {profession_oid::oid_arzt, profession_oid::oid_zahnarzt};
        case model::PrescriptionType::tRezept:
            return {profession_oid::oid_arzt};
    }
    Fail("Invalid professionOID type: " + std::to_string(static_cast<uintmax_t>(prescriptionType)));
    return {};
}
// GEMREQ-end A_20159-04#allowedProfessionOidsForQesSignature

bool ActivateTaskHandler::isPrescriptionEuRedeemable(const model::Task& task, const model::KbvBundle& bundle)
{
    switch (task.type())
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
            break;
        case model::PrescriptionType::digitaleGesundheitsanwendungen:
        case model::PrescriptionType::direkteZuweisung:
        case model::PrescriptionType::direkteZuweisungPkv:
        case model::PrescriptionType::tRezept:
            return false;
    }
    const auto& medication = bundle.getUniqueResourceByType<model::KbvMedicationGeneric>();
    return medication.getProfile() == model::ProfileType::KBV_PR_ERP_Medication_PZN;
}

bool ActivateTaskHandler::isPrescriptionEuRedeemable(const model::Task&, const model::EvdgaBundle&)
{
    return false;
}
