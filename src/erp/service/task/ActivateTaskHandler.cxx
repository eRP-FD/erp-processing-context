/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
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
#include "erp/model/KbvPractitioner.hxx"
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

    auto [responseStatus, prescriptionBundle] =
        prescriptionBundleFromXml(session.serviceContext, prescription, prescriptionId);

    const auto compositions = prescriptionBundle.getResourcesByType<model::Composition>("Composition");
    ErpExpect(compositions.size() == 1, HttpStatus::BadRequest,
              "Expected exactly one Composition in prescription bundle, got: " + std::to_string(compositions.size()));
    auto legalBasisCode = compositions[0].legalBasisCode();

    const auto& medicationRequests = prescriptionBundle.getResourcesByType<model::KbvMedicationRequest>();
    ErpExpect(medicationRequests.size() == 1, HttpStatus::BadRequest,
              "Unexpected number of MedicationRequests in prescription bundle: " +
                  std::to_string(medicationRequests.size()));
    const auto& medicationRequest = medicationRequests[0];
    bool isMvo = medicationRequest.isMultiplePrescription();
    std::optional<date::year_month_day> mvoEndDate = medicationRequest.mvoEndDate();
    const auto mvoExtension = medicationRequest.getExtension<model::KBVMultiplePrescription>();
    fillMvoBdeV2(mvoExtension, session);
    checkMultiplePrescription(mvoExtension, prescriptionId.type(), legalBasisCode, medicationRequest.authoredOn());

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
    ErpExpect(bundlePrescriptionId->type() == task.type(), HttpStatus::BadRequest,
              "Flowtype mismatch between Task and QES-Bundle");
    A_21370.finish();

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

    const auto signingTime = cadesBesSignature.getSigningTime();
    ErpExpect(signingTime.has_value(), HttpStatus::BadRequest, "No signingTime in PKCS7 file");

    try
    {
        checkAuthoredOnEqualsSigningDate(medicationRequest, *signingTime);
    }
    catch (const model::ModelException& m)
    {
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "error checking authoredOn==signature date", m.what());
    }

    checkValidCoverage(prescriptionBundle, prescriptionId.type());

    if (! checkPractitioner(prescriptionBundle, session))
    {
        A_24031.start("Use configuration value for ANR handling");
        const auto* errorMessage = "Ungültige Arztnummer (LANR oder ZANR): Die übergebene Arztnummer entspricht nicht "
                                   "den Prüfziffer-Validierungsregeln.";
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

    A_19025_02.start("3. reference the PKCS7 file in task");
    task.setHealthCarePrescriptionUuid();
    task.setPatientConfirmationUuid();
    const model::Binary healthCareProviderPrescriptionBinary(
        *task.healthCarePrescriptionUuid(),
        cadesBesSignature.getBase64());
    A_19025_02.finish();

    A_19999.start("enrich the task with ExpiryDate and AcceptDate from prescription bundle");
    date::year_month_day signingDay{signingTime->localDay()};
    if (isMvo)
    {
        setMvoExpiryAcceptDates(task, mvoEndDate, signingDay);
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
        task.setExpiryDate(model::Timestamp{date::sys_days{expiryDate}});
        A_19445_08.finish();
        A_19445_08.start("3. Task.AcceptDate = <Date of QES Creation + 28 days");
        A_19517_02.start("different validity duration (accept date) for different types");
        task.setAcceptDate(*signingTime, legalBasisCode,
                            config.getIntValue(ConfigurationKey::SERVICE_TASK_ACTIVATE_ENTLASSREZEPT_VALIDITY_WD));
        A_19517_02.finish();
        A_19445_08.finish();
    }
    A_19999.finish();

    // GEMREQ-start A_19127-01
    A_19127_01.start("store KVNR from prescription bundle in task");
    const auto patients = prescriptionBundle.getResourcesByType<model::Patient>("Patient");
    ErpExpect(patients.size() == 1, HttpStatus::BadRequest,
              "Expected exactly one Patient in prescription bundle, got: " + std::to_string(patients.size()));
    std::optional<model::Kvnr> kvnr;
    try
    {
        kvnr = patients[0].kvnr();
        A_23890.start("Validate Kvnr Checksum");
        ErpExpect(kvnr->validChecksum(), HttpStatus::BadRequest,
                  "Ungültige Versichertennummer (KVNR): Die übergebene Versichertennummer des Patienten entspricht "
                  "nicht den Prüfziffer-Validierungsregeln.");
        A_23890.finish();
    }
    catch (const model::ModelException& ex)
    {
        ErpFail(HttpStatus::BadRequest, ex.what());
    }
    task.setKvnr(*kvnr);
    A_19127_01.finish();
    // GEMREQ-end A_19127-01

    A_19128.start("status transition draft -> ready");
    task.setStatus(model::Task::Status::ready);
    A_19128.finish();

    task.updateLastUpdate();

    A_19025_02.start("2. store the PKCS7 file in database");
    databaseHandle = session.database();
    ErpExpect(taskAndKey->key.has_value(), HttpStatus::InternalServerError, "Missing task key.");
    databaseHandle->activateTask(taskAndKey->task, *taskAndKey->key, healthCareProviderPrescriptionBinary);
    A_19025_02.finish();

    makeResponse(session, responseStatus, &task);

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

fhirtools::ValidatorOptions
ActivateTaskHandler::validationOptions(model::ResourceVersion::FhirProfileBundleVersion version)
{
    const auto& config = Configuration::instance();
    auto valOpts = Configuration::instance().defaultValidatorOptions(version, SchemaType::KBV_PR_ERP_Bundle);
    switch (config.kbvValidationOnUnknownExtension())
    {
        using enum Configuration::OnUnknownExtension;
        using ReportUnknownExtensionsMode = fhirtools::ValidatorOptions::ReportUnknownExtensionsMode;
        case ignore:
            valOpts.reportUnknownExtensions = ReportUnknownExtensionsMode::disable;
            break;
        case report:
        case reject:
            if (config.genericValidationMode(version) == Configuration::GenericValidationMode::require_success)
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

void ActivateTaskHandler::checkAuthoredOnEqualsSigningDate(const model::KbvMedicationRequest& medicationRequest,
                                                           const model::Timestamp& signingTime)
{
    A_22487.start("check equality of signing day and authoredOn");
    // using German timezone was decided, but sadly did not make it into the requirement
    // date::local_days is a date not bound to any timezone.
    const date::local_days signingDay{signingTime.localDay()};
    const date::local_days authoredOn{medicationRequest.authoredOn().localDay()};

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
    A_22487.finish();
}

std::tuple<HttpStatus, model::KbvBundle>
ActivateTaskHandler::prescriptionBundleFromXml(PcServiceContext& serviceContext, std::string_view prescription,
                                               const model::PrescriptionId& prescriptionId)
{
    using namespace std::string_literals;
    using KbvBundleFactory = model::ResourceFactory<model::KbvBundle>;
    using OnUnknownExtension = Configuration::OnUnknownExtension;
    using GenericValidationMode = Configuration::GenericValidationMode;

    const auto& config = Configuration::instance();
    const auto& xmlValidator = serviceContext.getXmlValidator();
    const auto& inCodeValidator = serviceContext.getInCodeValidator();

    try
    {
        auto factory = KbvBundleFactory::fromXml(prescription, xmlValidator);
        auto profileName = factory.getProfileName();
        ErpExpect(profileName.has_value(), HttpStatus::BadRequest, "Missing meta.profile in Bundle.");
        const auto* profInfo = model::ResourceVersion::profileInfoFromProfileName(*profileName);
        ErpExpectWithDiagnostics(profInfo != nullptr, HttpStatus::BadRequest, "unknown or unexpected profile",
                                 "Unable to determine profile type from name: "s.append(*profileName));
        auto fhirProfileBundleVersion = profInfo->bundleVersion;

        A_23384.start("E-Rezept-Fachdienst: Prüfung Gültigkeit Profilversionen");
        const auto referenceTimestamp = factory.getValidationReferenceTimestamp();
        ErpExpect(referenceTimestamp.has_value(), HttpStatus::BadRequest,
                  "Unable to determine validation timestamp. authoredOn missing?");

        std::set<model::ResourceVersion::FhirProfileBundleVersion> supportedBundles;
        auto patchValidFrom = model::Timestamp::fromXsDate(
            config.getStringValue(ConfigurationKey::FHIR_PROFILE_PATCH_VALID_FROM), model::Timestamp::GermanTimezone);
        if (referenceTimestamp->localDay() >= patchValidFrom.localDay())
        {
            supportedBundles.emplace(model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01_patch);
            if (fhirProfileBundleVersion == model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01)
            {
                fhirProfileBundleVersion = model::ResourceVersion::FhirProfileBundleVersion::v_2023_07_01_patch;
            }
        }
        else
        {
            supportedBundles = model::ResourceVersion::supportedBundles(referenceTimestamp.value());
        }

        ErpExpect(supportedBundles.contains(fhirProfileBundleVersion), HttpStatus::BadRequest,
                  "Unsupported profile version");
        ErpExpect(! prescriptionId.isPkv() ||
                      fhirProfileBundleVersion > model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01,
                  HttpStatus::BadRequest,
                  "unsupported fhir version for PKV workflow 200/209: " + std::string(profileName.value()));
        const auto& fhirStructureRepo =
            Fhir::instance().structureRepository(*referenceTimestamp, fhirProfileBundleVersion);
        const auto genericValidationMode = config.genericValidationMode(fhirProfileBundleVersion);
        const auto onUnknownExtension = config.kbvValidationOnUnknownExtension();
        factory.enableAdditionalValidation(false);
        if (onUnknownExtension != OnUnknownExtension::ignore &&
            genericValidationMode == GenericValidationMode::require_success)
        {
            // downgrade to detail only as validateGeneric will be called later anyways
            factory.genericValidationMode(GenericValidationMode::detail_only);
        }
        else
        {
            factory.genericValidationMode(genericValidationMode);
        }
        const auto valOpts = validationOptions(fhirProfileBundleVersion);
        factory.validatorOptions(valOpts);
        factory.validate(SchemaType::KBV_PR_ERP_Bundle, xmlValidator, inCodeValidator, supportedBundles);
        auto status = HttpStatus::OK;
        if (onUnknownExtension != OnUnknownExtension::ignore)
        {
            status = checkExtensions(factory, onUnknownExtension, genericValidationMode, *fhirStructureRepo, valOpts);
        }
        std::tuple<HttpStatus, model::KbvBundle> result{status, std::move(factory).getNoValidation()};
        get<model::KbvBundle>(result).additionalValidation();
        return result;
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
        switch (Configuration::instance().mvoIdValidationMode())
        {
            case Configuration::MvoIdValidationMode::disable:
                break;
            case Configuration::MvoIdValidationMode::error:
                ErpExpectWithDiagnostics(
                    mvoIdValid, HttpStatus::BadRequest, "MVO-ID entspricht nicht urn:uuid format",
                    "urn:uuid format pattern: urn:uuid:<XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX>, found: " +
                        std::string{mvoId.value_or("")});
                break;
        }
        A_24901.finish();

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

        A_23537.start("Fehlercode 400 wenn Beginn der Einlösefrist vor Austellungsdatum");
        ErpExpect(mPExt->startDate().value() >= authoredOn, HttpStatus::BadRequest,
                  "Einlösefrist liegt zeitlich vor dem Ausstellungsdatum");
        A_23537.finish();
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

HttpStatus ActivateTaskHandler::checkExtensions(const model::ResourceFactory<model::KbvBundle>& factory,
                                                Configuration::OnUnknownExtension onUnknownExtension,
                                                Configuration::GenericValidationMode genericValidationMode,
                                                const fhirtools::FhirStructureRepository& fhirStructureRepo,
                                                const fhirtools::ValidatorOptions& valOpts)
{
    using OnUnknownExtension = Configuration::OnUnknownExtension;
    using GenericValidationMode = Configuration::GenericValidationMode;
    A_22927.start(" A_22927: E-Rezept-Fachdienst - Task aktivieren - Ausschluss unspezifizierter Extensions");
    auto validationResult = factory.validateGeneric(fhirStructureRepo, valOpts, {});
    auto highestSeverity = validationResult.highestSeverity();
    if (highestSeverity >= fhirtools::Severity::error &&
        genericValidationMode == GenericValidationMode::require_success)
    {
#ifdef ENABLE_DEBUG_LOG
        validationResult.dumpToLog();
#endif
        ErpFailWithDiagnostics(HttpStatus::BadRequest, "FHIR-Validation error", validationResult.summary());
    }
    bool haveUnslicedWarn = std::ranges::any_of(validationResult.results(), [](const fhirtools::ValidationError& err) {
        return err.severity() == fhirtools::Severity::unslicedWarning;
    });
    if (haveUnslicedWarn)
    {
        if (onUnknownExtension == OnUnknownExtension::reject)
        {
            ErpFailWithDiagnostics(
                HttpStatus::BadRequest,
                "unintendierte Verwendung von Extensions an unspezifizierter Stelle im Verordnungsdatensatz",
                validationResult.summary(fhirtools::Severity::unslicedWarning));
        }
        return HttpStatus::Accepted;
    }
    A_22927.finish();
    return HttpStatus::OK;
}


void ActivateTaskHandler::checkValidCoverage(const model::KbvBundle& bundle, const model::PrescriptionType prescriptionType)
{
    A_22222.start("Check for allowed coverage type");
    const auto& coverage = bundle.getResourcesByType<model::KbvCoverage>("Coverage");
    ErpExpect(coverage.size() <= 1, HttpStatus::BadRequest, "Unexpected number of Coverage Resources in KBV Bundle");
    bool pkvCovered = false;
    for (const auto& currentCoverage : coverage)
    {
        const auto coverageType = currentCoverage.typeCodingCode();
        pkvCovered = (coverageType == "PKV");
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
    A_23443.start("Check coverage type for flowtype 160/169");
    switch (prescriptionType)
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
        case model::PrescriptionType::direkteZuweisung:
            ErpExpect(!pkvCovered, HttpStatus::BadRequest, "Coverage \"PKV\" not allowed for flowtype 160/169");
            break;
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case model::PrescriptionType::direkteZuweisungPkv:
            ErpExpect(pkvCovered, HttpStatus::BadRequest, "Coverage \"PKV\" not set for flowtype 200/209");
            break;
    }
    A_23443.finish();
    A_22347_01.finish();
}


bool ActivateTaskHandler::checkPractitioner(const model::KbvBundle& bundle, PcSessionContext& session)
{
    const auto kbvPractitioners = bundle.getResourcesByType<model::KbvPractitioner>();
    for (const auto& practitioner : kbvPractitioners)
    {
        A_23891.start("Validate Practitioner Checksum");
        auto anr = practitioner.anr();
        if (anr.has_value() && ! anr->validChecksum())
        {
            A_23090_02.start("\"anr\": $anrvalue: Der Wert des Feldes identifier:ANR.value bei aufgetretenem "
                             "Prüfungsfehler gem. A_24032, Datentyp Integer");
            session.addOuterResponseHeaderField(Header::ANR, anr->id());
            A_23090_02.finish();
            return false;
        }

        auto zanr = practitioner.zanr();
        if (zanr.has_value() && ! zanr->validChecksum())
        {
            A_23090_02.start("\"zanr\": $zanrvalue: Der Wert des Feldes identifier:ZANR.value bei aufgetretenem "
                             "Prüfungsfehler gem. A_24032, Datentyp Integer");
            session.addOuterResponseHeaderField(Header::ZANR, zanr->id());
            A_23090_02.finish();
            return false;
        }
        A_23891.finish();
    }
    return true;
}
