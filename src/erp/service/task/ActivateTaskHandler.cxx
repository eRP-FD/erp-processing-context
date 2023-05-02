/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/service/task/ActivateTaskHandler.hxx"
#include "erp/ErpRequirements.hxx"
#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/database/Database.hxx"
#include "erp/fhir/internal/FhirSAXHandler.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/Composition.hxx"
#include "erp/model/KbvBundle.hxx"
#include "erp/model/KbvCoverage.hxx"
#include "erp/model/KbvMedicationBase.hxx"
#include "erp/model/KbvMedicationCompounding.hxx"
#include "erp/model/KbvMedicationRequest.hxx"
#include "erp/model/Parameters.hxx"
#include "erp/model/Patient.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/extensions/KBVMultiplePrescription.hxx"
#include "erp/server/response/ServerResponse.hxx"
#include "erp/tsl/error/TslError.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/TLog.hxx"
#include "erp/util/Uuid.hxx"
#include "erp/validation/InCodeValidator.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "fhirtools/validator/ValidatorOptions.hxx"

#include <date/date.h>
#include <date/tz.h>
#include <ranges>


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
    const CadesBesSignature cadesBesSignature = unpackCadesBesSignature(
        cadesBesSignatureFile, session.serviceContext.getTslManager());
    A_20704.finish();
    A_19020.finish();

    const auto& prescription = cadesBesSignature.payload();

    auto prescriptionBundle = prescriptionBundleFromXml(session.serviceContext, prescription);
    HttpStatus extensionsStatus = checkExtensions(prescriptionBundle);

    const auto compositions = prescriptionBundle.getResourcesByType<model::Composition>("Composition");
    ErpExpect(compositions.size() == 1, HttpStatus::BadRequest,
              "Expected exactly one Composition in prescription bundle, got: " + std::to_string(compositions.size()));
    auto legalBasisCode = compositions[0].legalBasisCode();

    const auto& medicationRequests = prescriptionBundle.getResourcesByType<model::KbvMedicationRequest>();
    ErpExpect(medicationRequests.size() <= 1, HttpStatus::BadRequest,
              "Too many MedicationRequests in prescription bundle: " + std::to_string(medicationRequests.size()));

    bool isMvo = ! medicationRequests.empty() && medicationRequests[0].isMultiplePrescription();
    std::optional<date::year_month_day> mvoEndDate =
        medicationRequests.empty() ? std::nullopt : medicationRequests[0].mvoEndDate();
    if (! medicationRequests.empty())
    {
        checkMultiplePrescription(medicationRequests[0].getExtension<model::KBVMultiplePrescription>(),
                                  prescriptionId.type(), legalBasisCode);
    }

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

    if (config.getOptionalBoolValue(ConfigurationKey::SERVICE_TASK_ACTIVATE_AUTHORED_ON_MUST_EQUAL_SIGNING_DATE, true))
    {
        try
        {
            checkAuthoredOnEqualsSigningDate(prescriptionBundle, *signingTime);
        }
        catch (const model::ModelException& m)
        {
            ErpFailWithDiagnostics(HttpStatus::BadRequest, "error checking authoredOn==signature date", m.what());
        }
    }

    checkValidCoverage(prescriptionBundle, prescriptionId.type());

    A_19025.start("3. reference the PKCS7 file in task");
    task->setHealthCarePrescriptionUuid();
    task->setPatientConfirmationUuid();
    const model::Binary healthCareProviderPrescriptionBinary(
        *task->healthCarePrescriptionUuid(),
        cadesBesSignature.getBase64());
    A_19025.finish();

    A_19999.start("enrich the task with ExpiryDate and AcceptDate from prescription bundle");
    date::year_month_day signingDay{date::floor<date::days>(
        date::make_zoned(model::Timestamp::GermanTimezone, signingTime->toChronoTimePoint()).get_local_time())};
    if (isMvo)
    {
        setMvoExpiryAcceptDates(*task, mvoEndDate, signingDay);
    }
    else
    {
        // A_19445 Part 1 and 4 are in $create
        A_19445_08.start("2. Task.ExpiryDate = <Date of QES Creation + 3 month");
        auto expiryDate = signingDay + date::months{3};
        if (! expiryDate.ok())
        {
            expiryDate = expiryDate.year() / expiryDate.month() / date::last;
        }
        task->setExpiryDate(model::Timestamp{date::sys_days{expiryDate}});
        A_19445_08.finish();
        A_19445_08.start("3. Task.AcceptDate = <Date of QES Creation + 28 days");
        A_19517_02.start("different validity duration (accept date) for different types");
        task->setAcceptDate(*signingTime, legalBasisCode,
                            config.getIntValue(ConfigurationKey::SERVICE_TASK_ACTIVATE_ENTLASSREZEPT_VALIDITY_WD));
        A_19517_02.finish();
        A_19445_08.finish();
    }
    A_19999.finish();


    A_19127.start("store KVNR from prescription bundle in task");
    const auto patients = prescriptionBundle.getResourcesByType<model::Patient>("Patient");
    ErpExpect(patients.size() == 1, HttpStatus::BadRequest,
              "Expected exactly one Patient in prescription bundle, got: " + std::to_string(patients.size()));
    std::optional<model::Kvnr> kvnr;
    try
    {
        kvnr = patients[0].kvnr();
    }
    catch (const model::ModelException& ex)
    {
        ErpFail(HttpStatus::BadRequest, ex.what());
    }
    task->setKvnr(*kvnr);
    A_19127.finish();

    A_19128.start("status transition draft -> ready");
    task->setStatus(model::Task::Status::ready);
    A_19128.finish();

    task->updateLastUpdate();

    A_19025.start("2. store the PKCS7 file in database");
    databaseHandle = session.database();
    databaseHandle->activateTask(task.value(), healthCareProviderPrescriptionBinary);
    A_19025.finish();

    makeResponse(session, extensionsStatus, &task.value());

    // Collect audit data:
    session.auditDataCollector()
        .setEventId(model::AuditEventId::POST_Task_activate)
        .setInsurantKvnr(*kvnr)
        .setAction(model::AuditEvent::Action::update)
        .setPrescriptionId(prescriptionId);
}

void ActivateTaskHandler::setMvoExpiryAcceptDates(model::Task& task,
                                                  const std::optional<date::year_month_day>& mvoEndDate,
                                                  const date::year_month_day& signingDay) const
{
    if (mvoEndDate)
    {
        A_19445_08.start(
            "2. Task.ExpiryDate = "
            "MedicationRequest.extension:Mehrfachverordnung.extension:Zeitraum.value[x]:valuePeriod.end");
        task.setExpiryDate(model::Timestamp{date::sys_days{*mvoEndDate}});
        A_19445_08.start(
            "2. Task.AcceptDate = "
            "MedicationRequest.extension:Mehrfachverordnung.extension:Zeitraum.value[x]:valuePeriod.end");
        task.setAcceptDate(model::Timestamp{date::sys_days{*mvoEndDate}});
    }
    else
    {
        A_19445_08.start("Task.ExpiryDate = <Datum der QES.Erstellung im Signaturobjekt> + 365 Kalendertage");
        task.setExpiryDate(model::Timestamp{date::sys_days{signingDay} + date::days{365}});
        A_19445_08.start("Task.AcceptDate = <Datum der QES.Erstellung im Signaturobjekt> + 365 Kalendertage");
        task.setAcceptDate(model::Timestamp{date::sys_days{signingDay} + date::days{365}});
    }
}

fhirtools::ValidatorOptions ActivateTaskHandler::validationOptions()
{
    const auto& config = Configuration::instance();
    auto valOpts = model::ResourceBase::defaultValidatorOptions();
    valOpts.allowNonLiteralAuthorReference =
        (config.kbvValidationNonLiteralAuthorRef() == Configuration::NonLiteralAutherRefMode::allow);
    switch (config.kbvValidationOnUnknownExtension())
    {
        using enum Configuration::OnUnknownExtension;
        using ReportUnknownExtensionsMode = fhirtools::ValidatorOptions::ReportUnknownExtensionsMode;
        case ignore:
            valOpts.reportUnknownExtensions = ReportUnknownExtensionsMode::disable;
            break;
        case report:
        case reject:
            if (config.genericValidationMode() == Configuration::GenericValidationMode::require_success)
            {
                // unknown extensions in closed slicing will be reported as error
                // open slices will be unslicedWarning
                valOpts.reportUnknownExtensions = ReportUnknownExtensionsMode::onlyOpenSlicing;
            }
            else
            {
                // downgrade slice strictness of closed slicings to allow unknown detection for closed slices, too
                valOpts.reportUnknownExtensions = ReportUnknownExtensionsMode::enable;
            }
            break;
    }
    return valOpts;

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
                                                           const model::Timestamp& signingTime)
{
    A_22487.start("check equality of signing day and authoredOn");
    // using German timezone was decided, but sadly did not make it into the requirement
    // date::local_days is a date not bound to any timezone.
    const date::local_days signingDay{date::floor<date::days>(
        date::make_zoned(model::Timestamp::GermanTimezone, signingTime.toChronoTimePoint()).get_local_time())};
    const auto& medicationRequests = bundle.getResourcesByType<model::KbvMedicationRequest>();
    for (const auto& mr : medicationRequests)
    {
        const date::local_days authoredOn{
            date::floor<date::days>(date::make_zoned(mr.authoredOn().toChronoTimePoint()).get_local_time())};

        if (signingDay != authoredOn)
        {
            std::ostringstream oss;
            oss << "KBVBundle.signature.signingDay=" << signingDay
                << " != KBVBundle.MedicationRequest.authoredOn=" << authoredOn;
            TVLOG(1) << oss.str();
            ErpFailWithDiagnostics(
                HttpStatus::BadRequest,
                "Ausstellungsdatum und Signaturzeitpunkt weichen voneinander ab, müssen aber taggleich sein",
                oss.str());
        }
    }
    A_22487.finish();
}

model::KbvBundle ActivateTaskHandler::prescriptionBundleFromXml(PcServiceContext& serviceContext,
                                                                std::string_view prescription)
{
    const auto& xmlValidator = serviceContext.getXmlValidator();
    try
    {
        auto fhirSchemaValidationContext = xmlValidator.getSchemaValidationContextNoVer(SchemaType::fhir);
        FhirSaxHandler::validateXML(Fhir::instance().structureRepository(), prescription, *fhirSchemaValidationContext);
        auto kbvBundle = model::KbvBundle::fromXmlNoValidation(prescription);
        model::ResourceVersion::KbvItaErp schemaVersion =
            kbvBundle.getSchemaVersion(model::ResourceVersion::deprecated<model::ResourceVersion::KbvItaErp>());
        std::optional<std::string_view> profileName = kbvBundle.getProfileName();
        ErpExpect(profileName.has_value(), HttpStatus::BadRequest, "meta.profile missing");
        const auto bundleVersion = std::get<model::ResourceVersion::FhirProfileBundleVersion>(
            model::ResourceVersion::profileVersionFromName(profileName.value()));
        try
        {
            if (schemaVersion == model::ResourceVersion::KbvItaErp::v1_0_2)
            {
                const auto& inCodeValidator = serviceContext.getInCodeValidator();
                const auto& schemaValidationContext =
                    xmlValidator.getSchemaValidationContext(SchemaType::KBV_PR_ERP_Bundle, schemaVersion);
                FhirSaxHandler::validateXML(Fhir::instance().structureRepository(), prescription,
                                            *schemaValidationContext);
                inCodeValidator.validate(kbvBundle, SchemaType::KBV_PR_ERP_Bundle, schemaVersion, xmlValidator);
            }
            kbvBundle.additionalValidation();
        }
        catch (const ErpException& erpException)
        {
            const auto& config = Configuration::instance();
            using enum Configuration::GenericValidationMode;
            if (config.genericValidationMode() == disable || erpException.status() != HttpStatus::BadRequest)
            {
                throw;
            }
            fhirtools::ValidationResults validationResult;
            if (erpException.diagnostics())
            {
                validationResult.add(fhirtools::Severity::error, *erpException.diagnostics(), {}, nullptr);
            }
            validationResult.merge(kbvBundle.genericValidate(bundleVersion, validationOptions()));
            auto details = validationResult.summary(fhirtools::Severity::unslicedWarning);
            ErpFailWithDiagnostics(HttpStatus::BadRequest, erpException.what(), std::move(details));
        }
        // generic validation will be handled in checkExtensions
        return kbvBundle;
    }
    catch (const model::ModelException& er)
    {
        TVLOG(1) << "runtime_error: " << er.what();
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "parsing / validation error", er.what());
    }
}

void ActivateTaskHandler::checkMultiplePrescription(const std::optional<model::KBVMultiplePrescription>& mPExt,
                                                    const model::PrescriptionType prescriptionType,
                                                    std::optional<model::KbvStatusKennzeichen> legalBasisCode)
{
    if (! mPExt)
    {
        return;
    }
    if (mPExt->isMultiplePrescription())
    {
        A_23164.start("Fehlercode 400 wenn das Ende der Einlösefrist vor dem Beginn liegt.");
        if (mPExt->endDate().has_value()) {
            ErpExpect(mPExt->startDate() <= mPExt->endDate(), HttpStatus::BadRequest, "Ende der Einlösefrist liegt vor Beginn der Einlösefrist");
        }
        A_23164.finish();
        A_22627_01.start("Fehlercode 400 wenn Flowtype ungleich 160, 169, 200 oder 209");
        switch (prescriptionType)
        {
            case model::PrescriptionType::apothekenpflichigeArzneimittel:
            case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
            case model::PrescriptionType::direkteZuweisung:
            case model::PrescriptionType::direkteZuweisungPkv:
                break;
            // Error message for future FlowTypes that do not support MVO.
                ErpFailWithDiagnostics(
                    HttpStatus::BadRequest,
                    "Mehrfachverordnungen nur für die Verordnungen von apothekenpflichtigen Arzneimittel zulässig",
                    "Invalid FlowType " + std::string(model::PrescriptionTypeDisplay.at(prescriptionType)) +
                        " for MVO");
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
    }
    else // if (mPExt->isMultiplePrescription())
    {
        A_22631.start(
            "Fehlercode 400 wenn Kennzeichen = false, aber eine Extension Nummerierung oder Zeitraum vorhanden");
        ErpExpect(! mPExt->getExtension<model::KBVMultiplePrescription::Nummerierung>().has_value(),
                  HttpStatus::BadRequest, "Nummerierung darf nur bei MVO angegeben werden");
        ErpExpect(! mPExt->getExtension<model::KBVMultiplePrescription::Zeitraum>().has_value(),
                  HttpStatus::BadRequest, "Zeitraum darf nur bei MVO angegeben werden");
        A_22631.finish();
    }
}

HttpStatus ActivateTaskHandler::checkExtensions(const model::KbvBundle& kbvBundle)
{
    A_22927.start(" A_22927: E-Rezept-Fachdienst - Task aktivieren - Ausschluss unspezifizierter Extensions");
    const auto& config = Configuration::instance();
    auto onUnknownExtension = config.kbvValidationOnUnknownExtension();
    auto genericValidationMode = config.genericValidationMode();
    if (onUnknownExtension == Configuration::OnUnknownExtension::ignore)
    {
        switch (genericValidationMode)
        {
            case Configuration::GenericValidationMode::disable:
            case Configuration::GenericValidationMode::detail_only:
                return HttpStatus::OK;
            case Configuration::GenericValidationMode::ignore_errors:
            case Configuration::GenericValidationMode::require_success:
                break;
        }
    }
    model::ResourceVersion::FhirProfileBundleVersion bundleVersion = model::ResourceVersion::currentBundle();
    try
    {
        bundleVersion = std::get<model::ResourceVersion::FhirProfileBundleVersion>(
            model::ResourceVersion::profileVersionFromName(kbvBundle.getProfileName().value()));
    }
    catch (const model::ModelException& ex)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "invalid KBV profile", ex.what());
    }
    const auto& validationResult = kbvBundle.genericValidate(bundleVersion, validationOptions());
    fhirtools::Severity highestSeverity = validationResult.highestSeverity();
#ifdef ENABLE_DEBUG_LOG
    if (highestSeverity >= fhirtools::Severity::unslicedWarning)
    {
        validationResult.dumpToLog();
    }
#endif
    if (genericValidationMode == Configuration::GenericValidationMode::require_success)
    {
        ErpExpectWithDiagnostics(highestSeverity < fhirtools::Severity::error, HttpStatus::BadRequest,
                                 "parsing / validation error",
                                 validationResult.summary(fhirtools::Severity::unslicedWarning));
    }
    bool haveUnslicedWarning = std::ranges::any_of(validationResult.results(), [](const auto& valErr) {
        return valErr.severity() == fhirtools::Severity::unslicedWarning;
    });
    switch (onUnknownExtension)
    {
        using enum Configuration::OnUnknownExtension;
        case ignore:
            return HttpStatus::OK;
        case report:
            return haveUnslicedWarning ? HttpStatus::Accepted : HttpStatus::OK;
        case reject:
            ErpExpectWithDiagnostics(
                !haveUnslicedWarning, HttpStatus::BadRequest,
                "unintendierte Verwendung von Extensions an unspezifizierter Stelle im Verordnungsdatensatz",
                validationResult.summary(fhirtools::Severity::unslicedWarning));
            return HttpStatus::OK;
    }
    A_22927.finish();
    Fail2("Invalid value for Configuration::OnUnknownExtension: " + std::to_string(intmax_t(onUnknownExtension)),
          std::logic_error);
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
    A_22347_01.start("Check coverage type for flowtype 200/209 (PKV prescription)");
    switch (prescriptionType)
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
        case model::PrescriptionType::direkteZuweisung:
            break;
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case model::PrescriptionType::direkteZuweisungPkv:
            if (featureWf200enabled)
            {
                ErpExpect(pkvCovered, HttpStatus::BadRequest, "Coverage \"PKV\" not set for flowtype 200/209");
            }
            break;
    }
    A_22347_01.finish();
}
