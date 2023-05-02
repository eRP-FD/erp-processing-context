/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/ErpRequirements.hxx"
#include "erp/model/OperationOutcome.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <date/tz.h>
#include <gtest/gtest.h>

class Mehrfachverordnung : public ErpWorkflowTest
{
public:
    void createTaskMember(model::PrescriptionType type = model::PrescriptionType::apothekenpflichigeArzneimittel)
    {
        ASSERT_NO_FATAL_FAILURE(task = taskCreate(type));
        ASSERT_TRUE(task.has_value());
    }

    void TestStep(Requirement & requirement, const std::string& description)
    {
        requirement.test(description.c_str());
        RecordProperty("Description", description);
    }

    void SetUp() override
    {
        createTaskMember();
    }

    void validateOperationOutcome(const model::OperationOutcome& outcome,
                                  model::OperationOutcome::Issue::Type expectedErrorCode, const std::string& issueText,
                                  const std::optional<std::string>& diagnosticsText)
    {
        const auto issue = outcome.issues().at(0);
        ASSERT_EQ(issue.severity, model::OperationOutcome::Issue::Severity::error);
        ASSERT_EQ(issue.code, expectedErrorCode);
        ASSERT_EQ(issue.detailsText, issueText);
        if (diagnosticsText.has_value())
        {
            ASSERT_TRUE(issue.diagnostics.has_value());
            ASSERT_TRUE(String::contains(issue.diagnostics.value(), diagnosticsText.value()))
                << diagnosticsText.value() << " not found in " << issue.diagnostics.value();
        }
    }

    std::optional<model::Task> task;
    model::Timestamp timestamp = model::Timestamp::fromFhirDateTime("2020-06-23T08:30:00+00:00");
};

class MVO_A_22628 : public Mehrfachverordnung
{
};

TEST_F(MVO_A_22628, Step_01_Denominator_5)
{
    TestStep(A_22628, "ERP-A_22628.01 - Task aktivieren - Mehrfachverordnung - Denominator gleich 5");
    const auto mvoPrescription = ResourceTemplates::kbvBundleMvoXml(
        {.prescriptionId = task->prescriptionId(), .timestamp = timestamp, .numerator = 3, .denominator = 5});

    std::string issueText = "parsing / validation error";
    std::optional<std::string> issueDiagnostics = "Mehrfachverordnung: Nenner muss den Wert 2, 3 oder 4 haben";
    if (model::ResourceVersion::currentBundle() == model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01)
    {
        issueText = "Eine Mehrfachverordnungen darf aus maximal 4 Teilverordnungen bestehen";
        issueDiagnostics = {};
    }

    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, timestamp), HttpStatus::BadRequest));

    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(
        validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid, issueText, issueDiagnostics));
}


TEST_F(MVO_A_22628, Step_02_Numerator_5)
{
    TestStep(A_22628, "ERP-A_22628.02 - Task aktivieren - Mehrfachverordnung - Numerator gleich 5");
    const auto mvoPrescription = ResourceTemplates::kbvBundleMvoXml(
        {.prescriptionId = task->prescriptionId(), .timestamp = timestamp, .numerator = 5, .denominator = 4});
    RecordProperty("Prescription", Base64::encode(mvoPrescription));

    std::string issueText = "parsing / validation error";
    std::optional<std::string> issueDiagnostics = "Mehrfachverordnung: Zaehler muss den Wert 1, 2, 3 oder 4 haben";

    if (model::ResourceVersion::currentBundle() == model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01)
    {
        issueText = "Eine Mehrfachverordnungen darf aus maximal 4 Teilverordnungen bestehen";
        issueDiagnostics = {};
    }

    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, timestamp), HttpStatus::BadRequest));

    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(
        validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid, issueText, issueDiagnostics));
}


class MVO_A_22704 : public Mehrfachverordnung
{
};

TEST_F(MVO_A_22704, Step_01_Numerator_0)
{
    TestStep(A_22704, "ERP-A_22704.01 - Task aktivieren - Mehrfachverordnung - Numerator gleich 0");
    const auto mvoPrescription = ResourceTemplates::kbvBundleMvoXml(
        {.prescriptionId = task->prescriptionId(), .timestamp = timestamp, .numerator = 0, .denominator = 3});

    std::string issueText = "parsing / validation error";
    std::optional<std::string> issueDiagnostics = "Mehrfachverordnung: Zaehler muss den Wert 1, 2, 3 oder 4 haben";
    if (model::ResourceVersion::currentBundle() == model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01)
    {
        issueText = "Für eine Mehrfachverordnungen muss der numerator größer 0 sein.";
        issueDiagnostics = {};
    }
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, timestamp), HttpStatus::BadRequest));

    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(
        validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid, issueText, issueDiagnostics));
}

class MVO_A_22629 : public Mehrfachverordnung
{
};

TEST_F(MVO_A_22629, Step_01_Denominator_1)
{
    TestStep(A_22629, "ERP-A_22629.01 - Task aktivieren - Mehrfachverordnung - Denominator gleich 1");
    const auto mvoPrescription = ResourceTemplates::kbvBundleMvoXml(
        {.prescriptionId = task->prescriptionId(), .timestamp = timestamp, .numerator = 1, .denominator = 1});
    std::string issueText = "parsing / validation error";
    std::optional<std::string> issueDiagnostics = "Mehrfachverordnung: Nenner muss den Wert 2, 3 oder 4 haben";
    if (model::ResourceVersion::currentBundle() == model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01)
    {
        issueText = "Eine Mehrfachverordnung muss aus mindestens 2 Teilverordnungen bestehen";
        issueDiagnostics = {};
    }

    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, timestamp), HttpStatus::BadRequest));

    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(
        validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid, issueText, issueDiagnostics));
}


class MVO_A_22630 : public Mehrfachverordnung
{
};

TEST_F(MVO_A_22630, Step_01_Numerator_greater_Denominator)
{
    TestStep(A_22630,
             "ERP-A_22630.01 - E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - Numerator > Denominator");
    const auto mvoPrescription = ResourceTemplates::kbvBundleMvoXml(
        {.prescriptionId = task->prescriptionId(), .timestamp = timestamp, .numerator = 4, .denominator = 3});
    std::string issueText = "parsing / validation error";
    std::optional<std::string> issueDiagnostics =
        "Mehrfachverordnung: Der Zaehler (Nummer der Teilverordnung) darf nicht größer als "
        "der Nenner (Gesamtanzahl) sein";
    if (model::ResourceVersion::currentBundle() == model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01)
    {
        issueText = "Numerator ist größer als denominator";
        issueDiagnostics = {};
    }
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, timestamp), HttpStatus::BadRequest));

    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(
        validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid, issueText, issueDiagnostics));
}


class MVO_A_22631 : public Mehrfachverordnung
{
};

TEST_F(MVO_A_22631, Step_01_Zeitraum)
{
    TestStep(A_22631, "ERP-A_22631.01 - Task aktivieren - Mehrfachverordnung - Verordnung mit Zeitraum");
    const auto* multiplePrescriptionExtension = R"(<extension url="Zeitraum">
                        <valuePeriod>
                            <start value="2021-01-02"/>
                            <end value="2021-03-30"/>
                        </valuePeriod>
                    </extension>)";
    auto mvoPrescription = ResourceTemplates::kbvBundleXml({.prescriptionId = task->prescriptionId(),
                                                                  .timestamp = timestamp});
    constexpr std::string_view multiplePrescriptionExtStart =
        R"(<extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Multiple_Prescription">)";
    const auto extPos = mvoPrescription.find(multiplePrescriptionExtStart);
    ASSERT_NE(extPos, std::string::npos);
    mvoPrescription.insert(extPos + multiplePrescriptionExtStart.length(), multiplePrescriptionExtension);

    std::string issueText = "parsing / validation error";
    std::optional<std::string> issueDiagnostics =
        "Mehrfachverordnung: Wenn das Kennzeichen gleich false ist, darf kein Zeitraum angegeben werden.";
    if (model::ResourceVersion::currentBundle() == model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01)
    {
        issueText = "Zeitraum darf nur bei MVO angegeben werden";
        issueDiagnostics = {};
    }
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, timestamp), HttpStatus::BadRequest));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(
        validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid, issueText, issueDiagnostics));
}


TEST_F(MVO_A_22631, Step_02_Nummerierung)
{
    TestStep(A_22631, "ERP-A_22631.02 - Task aktivieren - Mehrfachverordnung - Verordnung mit Nummerierung");
    const auto* multiplePrescriptionExtension = R"(<extension url="Nummerierung">
                        <valueRatio>
                            <numerator>
                                <value value="1"/>
                            </numerator>
                            <denominator>
                                <value value="3"/>
                            </denominator>
                        </valueRatio>
                    </extension>)";
    auto mvoPrescription =
        ResourceTemplates::kbvBundleXml({.prescriptionId = task->prescriptionId(), .timestamp = timestamp});

    constexpr std::string_view multiplePrescriptionExtStart =
        R"(<extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Multiple_Prescription">)";
    const auto extPos = mvoPrescription.find(multiplePrescriptionExtStart);
    ASSERT_NE(extPos, std::string::npos);
    mvoPrescription.insert(extPos + multiplePrescriptionExtStart.length(), multiplePrescriptionExtension);

    std::string issueText = "parsing / validation error";
    std::optional<std::string> issueDiagnostics =
        "Mehrfachverordnung: Wenn das Kennzeichen gleich false ist, darf keine Nummerierung "
        "(Zaehler und Nenner) angegeben werden";
    if (model::ResourceVersion::currentBundle() == model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01)
    {
        issueText = "Nummerierung darf nur bei MVO angegeben werden";
        issueDiagnostics = {};
    }

    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, timestamp), HttpStatus::BadRequest));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(
        validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid, issueText, issueDiagnostics));
}


class MVO_A_22632 : public Mehrfachverordnung
{
};

TEST_F(MVO_A_22632, Step_01_Code_04)
{
    TestStep(A_22632, "ERP-A_22632.01 - Task aktivieren - Mehrfachverordnung - Entlassrezept Code 04 ablehnen");
    const auto mvoPrescription = ResourceTemplates::kbvBundleMvoXml(
        {.prescriptionId = task->prescriptionId(), .timestamp = timestamp, .legalBasisCode = "04"});

    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, timestamp), HttpStatus::BadRequest));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid,
                                                     "Für Entlassrezepte sind keine Mehrfachverordnungen zulässig",
                                                     {}));
}

TEST_F(MVO_A_22632, Step_02_Code_14)
{
    TestStep(A_22632, "ERP-A_22632.02 - Task aktivieren - Mehrfachverordnung - Entlassrezept Code 14 ablehnen");
    const auto mvoPrescription = ResourceTemplates::kbvBundleMvoXml(
        {.prescriptionId = task->prescriptionId(), .timestamp = timestamp, .legalBasisCode = "14"});

    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, timestamp), HttpStatus::BadRequest));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid,
                                                     "Für Entlassrezepte sind keine Mehrfachverordnungen zulässig",
                                                     {}));
}

class MVO_A_22633 : public Mehrfachverordnung
{
};

TEST_F(MVO_A_22633, Step_01_Code_10)
{
    TestStep(A_22633, "ERP-A_22633.01 - Task aktivieren - Ersatzverordnung als Mehrfachverordnung ablehnen - Code 10");
    const auto mvoPrescription = ResourceTemplates::kbvBundleMvoXml(
        {.prescriptionId = task->prescriptionId(), .timestamp = timestamp, .legalBasisCode = "10"});

    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, timestamp), HttpStatus::BadRequest));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid,
                                                     "Für Ersatzverordnung sind keine Mehrfachverordnungen zulässig",
                                                     {}));
}

TEST_F(MVO_A_22633, Step_02_Code_11)
{
    TestStep(A_22633, "ERP-A_22633.02 - Task aktivieren - Ersatzverordnung als Mehrfachverordnung ablehnen - Code 11");
    const auto mvoPrescription = ResourceTemplates::kbvBundleMvoXml(
        {.prescriptionId = task->prescriptionId(), .timestamp = timestamp, .legalBasisCode = "11"});

    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, timestamp), HttpStatus::BadRequest));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid,
                                                     "Für Ersatzverordnung sind keine Mehrfachverordnungen zulässig",
                                                     {}));
}

TEST_F(MVO_A_22633, Step_03_Code_17)
{
    TestStep(A_22633, "ERP-A_22633.03 - Task aktivieren - Ersatzverordnung als Mehrfachverordnung ablehnen - Code 17");
    const auto mvoPrescription = ResourceTemplates::kbvBundleMvoXml(
        {.prescriptionId = task->prescriptionId(), .timestamp = timestamp, .legalBasisCode = "17"});

    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, timestamp), HttpStatus::BadRequest));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid,
                                                     "Für Ersatzverordnung sind keine Mehrfachverordnungen zulässig",
                                                     {}));
}

class MVO_A_22634 : public Mehrfachverordnung
{
};

TEST_F(MVO_A_22634, Step_01_MissingStart)
{
    TestStep(A_22634, "ERP-A_22634.01 - Task aktivieren - Mehrfachverordnung - Beginn Einlösefrist nicht angegeben");
    const auto mvoPrescription = ResourceTemplates::kbvBundleMvoXml(
        {.prescriptionId = task->prescriptionId(), .timestamp = timestamp, .redeemPeriodStart = {}});
    RecordProperty("Prescription", Base64::encode(mvoPrescription));

    std::string issueText = "parsing / validation error";
    std::optional<std::string> issueDiagnostics = "valuePeriod.start: error: missing mandatory element";
    if (model::ResourceVersion::currentBundle() == model::ResourceVersion::FhirProfileBundleVersion::v_2022_01_01)
    {
        issueText = "Beginn der Einlösefrist für MVO ist erforderlich";
        issueDiagnostics = {};
    }
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, timestamp), HttpStatus::BadRequest));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(
        validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid, issueText, issueDiagnostics));
}

class A_22635Test : public Mehrfachverordnung
{
};

TEST_F(A_22635Test, Step_01_FutureStartDate)
{
    TestStep(A_22635, "01 Apotheker ruft Accept Task für eine MVO mit Start-Datum in der Zukunft");
    using namespace std::chrono_literals;
    const auto tomorrow = model::Timestamp(std::chrono::system_clock::now() + 24h);
    const auto mvoPrescription = ResourceTemplates::kbvBundleMvoXml({.prescriptionId = task->prescriptionId(),
                                                                     .timestamp = timestamp,
                                                                     .redeemPeriodStart = tomorrow,
                                                                     .redeemPeriodEnd = {}});
    std::string tomorrowStr = tomorrow.toGermanDateFormat();
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    ASSERT_NO_FATAL_FAILURE(
        taskActivate(task->prescriptionId(), task->accessCode(), toCadesBesSignature(mvoPrescription, timestamp)));
    ASSERT_NO_FATAL_FAILURE(taskAccept(task->prescriptionId(), std::string{task->accessCode()}, HttpStatus::Forbidden,
                                       model::OperationOutcome::Issue::Type::forbidden,
                                       "Teilverordnung ab " + tomorrowStr + " einlösbar."));
}

class A_19445Test : public Mehrfachverordnung
{
};

TEST_F(A_19445Test, ExpiryAcceptDate365)
{
    using namespace date::literals;
    TestStep(A_19445_08, "01 signing date + 365 days");
    auto timestamp = model::Timestamp::fromXsDate("2020-02-03");
    const auto mvoPrescription = ResourceTemplates::kbvBundleMvoXml({.prescriptionId = task->prescriptionId(),
                                                                     .timestamp = timestamp,
                                                                     .redeemPeriodStart = timestamp,
                                                                     .redeemPeriodEnd = {}});
    auto expectedDate =
        date::make_zoned(model::Timestamp::GermanTimezone, floor<date::days>(date::local_days{2021_y / 02 / 02}))
            .get_sys_time();
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<model::Task> activatedTask;
    ASSERT_NO_FATAL_FAILURE(activatedTask =
                                taskActivateWithOutcomeValidation(task->prescriptionId(), task->accessCode(),
                                                           toCadesBesSignature(mvoPrescription, timestamp)));
    ASSERT_TRUE(activatedTask.has_value());
    EXPECT_EQ(activatedTask->acceptDate(), model::Timestamp(expectedDate));
    EXPECT_EQ(activatedTask->expiryDate(), model::Timestamp(expectedDate));
}

TEST_F(A_19445Test, ExpiryAcceptDateEndDate)
{
    TestStep(A_19445_08, "02 signing date given");
    auto signingTime = model::Timestamp::fromXsDate("2020-02-03");
    auto startDate = model::Timestamp::fromXsDate("2021-01-02");
    auto endDate = model::Timestamp::fromXsDate("2021-03-01");
    const auto mvoPrescription = ResourceTemplates::kbvBundleMvoXml({.prescriptionId = task->prescriptionId(),
                                                                     .timestamp = signingTime,
                                                                     .redeemPeriodStart = startDate,
                                                                     .redeemPeriodEnd = endDate});
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<model::Task> activatedTask;
    ASSERT_NO_FATAL_FAILURE(activatedTask =
                                taskActivateWithOutcomeValidation(task->prescriptionId(), task->accessCode(),
                                                           toCadesBesSignature(mvoPrescription, signingTime)));
    ASSERT_TRUE(activatedTask.has_value());
    auto expectedDate = model::Timestamp::fromGermanDate("2021-03-01");
    EXPECT_EQ(activatedTask->acceptDate(), expectedDate);
    EXPECT_EQ(activatedTask->expiryDate(), expectedDate);
}


class MVO_A_23164 : public Mehrfachverordnung
{
};

TEST_F(MVO_A_23164, Step_01_EndDateBeforeStartDateNoValidation_BadRequest)
{
    bool oldProfile = (serverGematikProfileVersion() == model::ResourceVersion::DeGematikErezeptWorkflowR4::v1_1_1);
    EnvironmentVariableGuard environmentVariableGuard{"ERP_SERVICE_GENERIC_VALIDATION_MODE",
                                                      oldProfile ? "disable" : "require_success"};
    // Issue and Diagnostic from the new version.
    std::string issueText = "parsing / validation error";
    std::optional<std::string> issueDiagnostics = "start SHALL have a lower value than end (from profile: http://hl7.org/fhir/StructureDefinition/Period|4.0.1);";
    if (oldProfile)
    {
        // Issue and Diagnostic from the current/old version.
        issueText = "Ende der Einlösefrist liegt vor Beginn der Einlösefrist";
        issueDiagnostics = {};
    }

    TestStep(A_23164, "ERP-A_22634.01 - Task aktivieren - Mehrfachverordnung - Endedatum vor Startdatum");

    const auto mvoPrescription =
        ResourceTemplates::kbvBundleMvoXml({.prescriptionId = task->prescriptionId(),
                                            .timestamp = timestamp,
                                            .redeemPeriodStart = model::Timestamp::fromGermanDate("2021-03-15"),
                                            .redeemPeriodEnd = model::Timestamp::fromGermanDate("2021-03-10")});

    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, timestamp), HttpStatus::BadRequest));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(
        validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid, issueText, issueDiagnostics));
}
