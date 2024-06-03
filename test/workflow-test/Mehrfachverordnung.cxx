/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
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

    void TestStep(Requirement& requirement, const std::string& description)
    {
        requirement.test(description.c_str());
        RecordProperty("Description", description);
    }

    void SetUp() override
    {
        if (serverUsesOldProfile())
        {
            GTEST_SKIP_("fading out KBV.1.02");
        }
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
    model::Timestamp authoredOn = model::Timestamp::now();
    EnvironmentVariableGuard mvoCheckEnabler{ConfigurationKey::SERVICE_TASK_ACTIVATE_MVOID_VALIDATION_MODE, "error"};
};

class MVO_A_22628 : public Mehrfachverordnung
{
};

TEST_F(MVO_A_22628, Step_01_Denominator_5)
{
    TestStep(A_22628, "ERP-A_22628.01 - Task aktivieren - Mehrfachverordnung - Denominator gleich 5");
    const auto mvoPrescription = kbvBundleMvoXml(
        {.prescriptionId = task->prescriptionId(), .authoredOn = authoredOn, .numerator = 3, .denominator = 5});

    if (!serverUsesOldProfile())
    {
        mActivateTaskRequestArgs.expectedMvoNumber = "XXX";
    }

    std::string issueText = "FHIR-Validation error";
    std::optional<std::string> issueDiagnostics = "Mehrfachverordnung: Nenner muss den Wert 2, 3 oder 4 haben";
    if (serverUsesOldProfile())
    {
        issueText = "Eine Mehrfachverordnungen darf aus maximal 4 Teilverordnungen bestehen";
        issueDiagnostics = {};
    }

    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, authoredOn), HttpStatus::BadRequest));

    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(
        validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid, issueText, issueDiagnostics));
}


TEST_F(MVO_A_22628, Step_02_Numerator_5)
{
    TestStep(A_22628, "ERP-A_22628.02 - Task aktivieren - Mehrfachverordnung - Numerator gleich 5");
    const auto mvoPrescription = kbvBundleMvoXml(
        {.prescriptionId = task->prescriptionId(), .authoredOn = authoredOn, .numerator = 5, .denominator = 4});
    if (!serverUsesOldProfile())
    {
        mActivateTaskRequestArgs.expectedMvoNumber = "XXX";
    }
    RecordProperty("Prescription", Base64::encode(mvoPrescription));

    std::string issueText = "FHIR-Validation error";
    std::optional<std::string> issueDiagnostics = "Mehrfachverordnung: Zaehler muss den Wert 1, 2, 3 oder 4 haben";

    if (serverUsesOldProfile())
    {
        issueText = "Eine Mehrfachverordnungen darf aus maximal 4 Teilverordnungen bestehen";
        issueDiagnostics = {};
    }

    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, authoredOn), HttpStatus::BadRequest));

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
    const auto mvoPrescription = kbvBundleMvoXml(
        {.prescriptionId = task->prescriptionId(), .authoredOn = authoredOn, .numerator = 0, .denominator = 3});
    if (!serverUsesOldProfile())
    {
        mActivateTaskRequestArgs.expectedMvoNumber = "XXX";
    }

    std::string issueText = "FHIR-Validation error";
    std::optional<std::string> issueDiagnostics = "Mehrfachverordnung: Zaehler muss den Wert 1, 2, 3 oder 4 haben";
    if (serverUsesOldProfile())
    {
        issueText = "Für eine Mehrfachverordnungen muss der numerator größer 0 sein.";
        issueDiagnostics = {};
    }
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, authoredOn), HttpStatus::BadRequest));

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
    const auto mvoPrescription = kbvBundleMvoXml(
        {.prescriptionId = task->prescriptionId(), .authoredOn = authoredOn, .numerator = 1, .denominator = 1});
    if (!serverUsesOldProfile())
    {
        mActivateTaskRequestArgs.expectedMvoNumber = "XXX";
    }
    std::string issueText = "FHIR-Validation error";
    std::optional<std::string> issueDiagnostics = "Mehrfachverordnung: Nenner muss den Wert 2, 3 oder 4 haben";
    if (serverUsesOldProfile())
    {
        issueText = "Eine Mehrfachverordnung muss aus mindestens 2 Teilverordnungen bestehen";
        issueDiagnostics = {};
    }

    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, authoredOn), HttpStatus::BadRequest));

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
    const auto mvoPrescription = kbvBundleMvoXml(
        {.prescriptionId = task->prescriptionId(), .authoredOn = authoredOn, .numerator = 4, .denominator = 3});
    if (!serverUsesOldProfile())
    {
        mActivateTaskRequestArgs.expectedMvoNumber = "XXX";
    }
    std::string issueText = "FHIR-Validation error";
    std::optional<std::string> issueDiagnostics =
        "Mehrfachverordnung: Der Zaehler (Nummer der Teilverordnung) darf nicht größer als "
        "der Nenner (Gesamtanzahl) sein";
    if (serverUsesOldProfile())
    {
        issueText = "Numerator ist größer als denominator";
        issueDiagnostics = {};
    }
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, authoredOn), HttpStatus::BadRequest));

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
    auto mvoPrescription = kbvBundleXml({.prescriptionId = task->prescriptionId(), .authoredOn = authoredOn});
    constexpr std::string_view multiplePrescriptionExtStart =
        R"(<extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Multiple_Prescription">)";
    const auto extPos = mvoPrescription.find(multiplePrescriptionExtStart);
    ASSERT_NE(extPos, std::string::npos);
    mvoPrescription.insert(extPos + multiplePrescriptionExtStart.length(), multiplePrescriptionExtension);

    std::string issueText = "FHIR-Validation error";
    std::optional<std::string> issueDiagnostics =
        "Mehrfachverordnung: Wenn das Kennzeichen gleich false ist, darf kein Zeitraum angegeben werden.";
    if (serverUsesOldProfile())
    {
        issueText = "Zeitraum darf nur bei MVO angegeben werden";
        issueDiagnostics = {};
    }
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, authoredOn), HttpStatus::BadRequest));
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
    auto mvoPrescription = kbvBundleXml({.prescriptionId = task->prescriptionId(), .authoredOn = authoredOn});
    if (!serverUsesOldProfile())
    {
        mActivateTaskRequestArgs.expectedMvoNumber = "XXX";
    }

    constexpr std::string_view multiplePrescriptionExtStart =
        R"(<extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Multiple_Prescription">)";
    const auto extPos = mvoPrescription.find(multiplePrescriptionExtStart);
    ASSERT_NE(extPos, std::string::npos);
    mvoPrescription.insert(extPos + multiplePrescriptionExtStart.length(), multiplePrescriptionExtension);

    std::string issueText = "FHIR-Validation error";
    std::optional<std::string> issueDiagnostics =
        "Mehrfachverordnung: Wenn das Kennzeichen gleich false ist, darf keine Nummerierung "
        "(Zaehler und Nenner) angegeben werden";
    if (serverUsesOldProfile())
    {
        issueText = "Nummerierung darf nur bei MVO angegeben werden";
        issueDiagnostics = {};
    }

    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, authoredOn), HttpStatus::BadRequest));
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
    const auto mvoPrescription =
        kbvBundleMvoXml({.prescriptionId = task->prescriptionId(), .authoredOn = authoredOn, .legalBasisCode = "04"});

    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, authoredOn), HttpStatus::BadRequest));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid,
                                                     "Für Entlassrezepte sind keine Mehrfachverordnungen zulässig",
                                                     {}));
}

TEST_F(MVO_A_22632, Step_02_Code_14)
{
    TestStep(A_22632, "ERP-A_22632.02 - Task aktivieren - Mehrfachverordnung - Entlassrezept Code 14 ablehnen");
    const auto mvoPrescription =
        kbvBundleMvoXml({.prescriptionId = task->prescriptionId(), .authoredOn = authoredOn, .legalBasisCode = "14"});

    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, authoredOn), HttpStatus::BadRequest));
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
    const auto mvoPrescription =
        kbvBundleMvoXml({.prescriptionId = task->prescriptionId(), .authoredOn = authoredOn, .legalBasisCode = "10"});

    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, authoredOn), HttpStatus::BadRequest));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid,
                                                     "Für Ersatzverordnung sind keine Mehrfachverordnungen zulässig",
                                                     {}));
}

TEST_F(MVO_A_22633, Step_02_Code_11)
{
    TestStep(A_22633, "ERP-A_22633.02 - Task aktivieren - Ersatzverordnung als Mehrfachverordnung ablehnen - Code 11");
    const auto mvoPrescription =
        kbvBundleMvoXml({.prescriptionId = task->prescriptionId(), .authoredOn = authoredOn, .legalBasisCode = "11"});

    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, authoredOn), HttpStatus::BadRequest));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid,
                                                     "Für Ersatzverordnung sind keine Mehrfachverordnungen zulässig",
                                                     {}));
}

TEST_F(MVO_A_22633, Step_03_Code_17)
{
    TestStep(A_22633, "ERP-A_22633.03 - Task aktivieren - Ersatzverordnung als Mehrfachverordnung ablehnen - Code 17");
    const auto mvoPrescription =
        kbvBundleMvoXml({.prescriptionId = task->prescriptionId(), .authoredOn = authoredOn, .legalBasisCode = "17"});

    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, authoredOn), HttpStatus::BadRequest));
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
    const auto mvoPrescription =
        kbvBundleMvoXml({.prescriptionId = task->prescriptionId(), .authoredOn = authoredOn, .redeemPeriodStart = {}});
    if (!serverUsesOldProfile())
    {
        mActivateTaskRequestArgs.expectedMvoNumber = "XXX";
    }
    RecordProperty("Prescription", Base64::encode(mvoPrescription));

    std::string issueText = "FHIR-Validation error";
    std::optional<std::string> issueDiagnostics = "valuePeriod.start: error: missing mandatory element";
    if (serverUsesOldProfile())
    {
        issueText = "Beginn der Einlösefrist für MVO ist erforderlich";
        issueDiagnostics = {};
    }
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, authoredOn), HttpStatus::BadRequest));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(
        validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid, issueText, issueDiagnostics));
}

class MVO_A_22635Test : public Mehrfachverordnung
{
};

TEST_F(MVO_A_22635Test, Step_01_FutureStartDate)
{
    TestStep(A_22635, "ERP-A_22635.01 Apotheker ruft Accept Task für eine MVO mit Start-Datum in der Zukunft");
    using namespace std::chrono_literals;
    const auto tomorrow = authoredOn + 24h;
    const auto mvoPrescription = kbvBundleMvoXml({.prescriptionId = task->prescriptionId(),
                                                  .authoredOn = authoredOn,
                                                  .redeemPeriodStart = tomorrow.toGermanDate(),
                                                  .redeemPeriodEnd = {}});
    std::string tomorrowStr = tomorrow.toGermanDateFormat();
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    ASSERT_NO_FATAL_FAILURE(
        taskActivate(task->prescriptionId(), task->accessCode(), toCadesBesSignature(mvoPrescription, authoredOn)));
    ASSERT_NO_FATAL_FAILURE(taskAccept(task->prescriptionId(), std::string{task->accessCode()}, HttpStatus::Forbidden,
                                       model::OperationOutcome::Issue::Type::forbidden,
                                       "Teilverordnung ab " + tomorrowStr + " einlösbar."));
}

TEST_F(MVO_A_22635Test, Step_01_Zeitraum_InvalidDate)
{
    auto mvoPrescription = kbvBundleMvoXml({.prescriptionId = task->prescriptionId(),
                                            .authoredOn = authoredOn,
                                            .redeemPeriodStart = "2021-01-01",
                                            .redeemPeriodEnd = "2021-01-02T23:59:59.999+01:00"});
    mActivateTaskRequestArgs.expectedMvoNumber = "XXX";

    std::string issueText = "FHIR-Validation error";
    std::optional<std::string> issueDiagnostics =
        "Begrenzung der Datumsangabe auf 10 Zeichen JJJJ-MM-TT";
    if (serverUsesOldProfile())
    {
        issueText = "date does not match YYYY-MM-DD in extension Zeitraum";
        issueDiagnostics = {};
    }
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, authoredOn), HttpStatus::BadRequest));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(
        validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid, issueText, issueDiagnostics));
}

class MVO_A_19445Test : public Mehrfachverordnung
{
};

TEST_F(MVO_A_19445Test, ExpiryAcceptDate365)
{
    using namespace date::literals;
    TestStep(A_19445_08, "ERP-A_19445-08.01 signing date + 365 days");
    const auto mvoPrescription = kbvBundleMvoXml({.prescriptionId = task->prescriptionId(),
                                                  .authoredOn = authoredOn,
                                                  .redeemPeriodStart = authoredOn.toGermanDate(),
                                                  .redeemPeriodEnd = {}});
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<model::Task> activatedTask;
    ASSERT_NO_FATAL_FAILURE(activatedTask =
                                taskActivateWithOutcomeValidation(task->prescriptionId(), task->accessCode(),
                                                                  toCadesBesSignature(mvoPrescription, authoredOn)));
    ASSERT_TRUE(activatedTask.has_value());
    EXPECT_EQ(activatedTask->acceptDate().localDay(), authoredOn.localDay() + date::days{365});
    EXPECT_EQ(activatedTask->expiryDate().localDay(), authoredOn.localDay() + date::days{365});
}

TEST_F(MVO_A_19445Test, ExpiryAcceptDateEndDate)
{
    TestStep(A_19445_08, "ERP-A_19445-08.02 signing date given");
    auto startDate = authoredOn + date::days{3};
    auto endDate = authoredOn + date::days{10};
    const auto mvoPrescription = kbvBundleMvoXml({.prescriptionId = task->prescriptionId(),
                                                  .authoredOn = authoredOn,
                                                  .redeemPeriodStart = startDate.toGermanDate(),
                                                  .redeemPeriodEnd = endDate.toGermanDate()});
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<model::Task> activatedTask;
    ASSERT_NO_FATAL_FAILURE(activatedTask =
                                taskActivateWithOutcomeValidation(task->prescriptionId(), task->accessCode(),
                                                                  toCadesBesSignature(mvoPrescription, authoredOn)));
    ASSERT_TRUE(activatedTask.has_value());
    EXPECT_EQ(activatedTask->acceptDate().localDay(), endDate.localDay());
    EXPECT_EQ(activatedTask->expiryDate().localDay(), endDate.localDay());
}


class MVO_A_23164 : public Mehrfachverordnung
{
};

TEST_F(MVO_A_23164, Step_01_EndDateBeforeStartDateNoValidation_BadRequest)
{
    // when running as integration-test the server will be on "disable", so we have to use disable in unit-test, too
    EnvironmentVariableGuard environmentVariableGuardOld{ConfigurationKey::SERVICE_OLD_PROFILE_GENERIC_VALIDATION_MODE,
                                                         "disable"};
    const auto startDate = authoredOn + date::days{3};
    auto endDate = authoredOn + date::days{2};
    // Issue and Diagnostic from the new version.
    std::string issueText = "FHIR-Validation error";
    std::optional<std::string> issueDiagnostics =
        "start SHALL have a lower value than end (from profile: http://hl7.org/fhir/StructureDefinition/Period|4.0.1);";
    if (serverUsesOldProfile())
    {
        // Issue and Diagnostic from the current/old version.
        issueText = "Ende der Einlösefrist liegt vor Beginn der Einlösefrist";
        issueDiagnostics = {};
    }

    TestStep(A_23164, "ERP-A_23164.01 - Task aktivieren - Mehrfachverordnung - Endedatum vor Startdatum");

    const auto mvoPrescription = kbvBundleMvoXml({.prescriptionId = task->prescriptionId(),
                                                  .authoredOn = authoredOn,
                                                  .redeemPeriodStart = startDate.toGermanDate(),
                                                  .redeemPeriodEnd = endDate.toGermanDate()});
    if (!serverUsesOldProfile())
    {
        mActivateTaskRequestArgs.expectedMvoNumber = "XXX";
    }

    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, authoredOn), HttpStatus::BadRequest));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(
        validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid, issueText, issueDiagnostics));
}

class MVO_A_23537 : public Mehrfachverordnung
{
};

TEST_F(MVO_A_23537, Step_01_StartDateBeforeAuthoredOn)
{
    TestStep(A_23537, "ERP-A_23537.01: Task aktivieren - Mehrfachverordnung - Startdatum vor Ausstellungsdatum");
    const auto startDate = authoredOn - date::days{3};
    auto endDate = authoredOn + date::days{3};
    const auto mvoPrescription = kbvBundleMvoXml({.prescriptionId = task->prescriptionId(),
                                                  .authoredOn = authoredOn,
                                                  .redeemPeriodStart = startDate.toGermanDate(),
                                                  .redeemPeriodEnd = endDate.toGermanDate()});
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, authoredOn), HttpStatus::BadRequest));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid,
                                                     "Einlösefrist liegt zeitlich vor dem Ausstellungsdatum", {}));
}

TEST_F(MVO_A_23537, Step_02_StartDateEqualsAuthoredOn)
{
    TestStep(A_23537, "ERP-A_23537.02: Task aktivieren - Mehrfachverordnung - Startdatum gleich Ausstellungsdatum");
    const auto startDate = authoredOn;
    auto endDate = authoredOn + date::days{7};
    const auto mvoPrescription = kbvBundleMvoXml({.prescriptionId = task->prescriptionId(),
                                                  .authoredOn = authoredOn,
                                                  .redeemPeriodStart = startDate.toGermanDate(),
                                                  .redeemPeriodEnd = endDate.toGermanDate()});
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<model::Task> activatedTask;
    ASSERT_NO_FATAL_FAILURE(activatedTask =
                                taskActivateWithOutcomeValidation(task->prescriptionId(), task->accessCode(),
                                                                  toCadesBesSignature(mvoPrescription, authoredOn)));
    ASSERT_TRUE(activatedTask.has_value());
}

TEST_F(MVO_A_23537, Step_03_StartDateAfterAuthoredOn)
{
    TestStep(A_23537, "ERP-A_23537.03: Task aktivieren - Mehrfachverordnung - Startdatum nach Ausstellungsdatum");
    const auto startDate = authoredOn + date::days{1};
    const auto endDate = authoredOn + date::days{7};
    const auto mvoPrescription = kbvBundleMvoXml({.prescriptionId = task->prescriptionId(),
                                                  .authoredOn = authoredOn,
                                                  .redeemPeriodStart = startDate.toGermanDate(),
                                                  .redeemPeriodEnd = endDate.toGermanDate()});
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<model::Task> activatedTask;
    ASSERT_NO_FATAL_FAILURE(activatedTask =
                                taskActivateWithOutcomeValidation(task->prescriptionId(), task->accessCode(),
                                                                  toCadesBesSignature(mvoPrescription, authoredOn)));
    ASSERT_TRUE(activatedTask.has_value());
}

class MVO_A_23539Test : public Mehrfachverordnung
{
};

TEST_F(MVO_A_23539Test, Step_01_PastEndDate)
{
    TestStep(A_23539_01, "ERP-A_23539.01 Apotheker ruft Accept Task für eine MVO mit End-Datum in der Vergangenheit");
    using namespace std::chrono_literals;
    const auto yesterday = authoredOn - 24h;
    const auto mvoPrescription = kbvBundleMvoXml({.prescriptionId = task->prescriptionId(),
                                                  .authoredOn = yesterday,
                                                  .redeemPeriodStart = yesterday.toGermanDate(),
                                                  .redeemPeriodEnd = yesterday.toGermanDate()});
    std::string yesterdayStr = yesterday.toGermanDateFormat();
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE( result =
            taskActivate(task->prescriptionId(), task->accessCode(), toCadesBesSignature(mvoPrescription, yesterday))
        );
    ASSERT_TRUE(std::holds_alternative<model::Task>(*result));
    const auto& resultTask = std::get<model::Task>(*result);
    EXPECT_EQ(resultTask.expiryDate().toGermanDateFormat(), yesterdayStr);
    mAcceptTaskRequestArgs.expectedMvoNumber = "XXX";
    ASSERT_NO_FATAL_FAILURE(taskAccept(task->prescriptionId(), std::string{task->accessCode()}, HttpStatus::Forbidden,
                                       model::OperationOutcome::Issue::Type::forbidden,
                                       "Verordnung bis " + yesterdayStr + " einlösbar."));
}

TEST_F(MVO_A_23539Test, Step_02_TodayEndDate)
{
    TestStep(A_23539_01, "ERP-A_23539.02 Apotheker ruft Accept Task für eine MVO mit End-Datum heute");
    using namespace std::chrono_literals;
    const auto today = authoredOn;
    const auto mvoPrescription = kbvBundleMvoXml({.prescriptionId = task->prescriptionId(),
                                                  .authoredOn = authoredOn,
                                                  .redeemPeriodStart = today.toGermanDate(),
                                                  .redeemPeriodEnd = today.toGermanDate()});
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    ASSERT_NO_FATAL_FAILURE(
        taskActivate(task->prescriptionId(), task->accessCode(), toCadesBesSignature(mvoPrescription, authoredOn)));
    ASSERT_NO_FATAL_FAILURE(taskAccept(task->prescriptionId(), std::string{task->accessCode()}, HttpStatus::OK));
}

class MVO_A_24901Test : public Mehrfachverordnung
{
};

TEST_F(MVO_A_24901Test, InvalidMvoIdCheckEnabled)
{
    TestStep(A_23539_01, "ERP-A_24901.01 MVO-ID ungültig");

    using namespace std::chrono_literals;
    const auto today = authoredOn;
    const auto mvoPrescription = kbvBundleMvoXml({.prescriptionId = task->prescriptionId(),
                                                  .authoredOn = authoredOn,
                                                  .redeemPeriodStart = today.toGermanDate(),
                                                  .redeemPeriodEnd = today.toGermanDate(),
                                                  .mvoId = "urn::uuid:24e2e10d-e962-4d1c-be4f-8760e690a5f0"});
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result =
                                taskActivate(task->prescriptionId(), task->accessCode(),
                                             toCadesBesSignature(mvoPrescription, authoredOn), HttpStatus::BadRequest));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(*result));
    const auto& outcome = std::get<model::OperationOutcome>(*result);
    ASSERT_NO_FATAL_FAILURE(validateOperationOutcome(outcome, model::OperationOutcome::Issue::Type::invalid,
                                                     "MVO-ID entspricht nicht urn:uuid format", {}));
}

TEST_F(MVO_A_24901Test, InvalidMvoIdCheckDisabled)
{
    TestStep(A_23539_01, "ERP-A_24901.02 MVO-ID ungültig, Prüfung abgeschaltet");

    EnvironmentVariableGuard mvoCheckDisabler(ConfigurationKey::SERVICE_TASK_ACTIVATE_MVOID_VALIDATION_MODE, "disable");

    using namespace std::chrono_literals;
    const auto today = authoredOn;
    const auto mvoPrescription = kbvBundleMvoXml({.prescriptionId = task->prescriptionId(),
                                                  .authoredOn = authoredOn,
                                                  .redeemPeriodStart = today.toGermanDate(),
                                                  .redeemPeriodEnd = today.toGermanDate(),
                                                  .mvoId = "urn::uuid:24e2e10d-e962-4d1c-be4f-8760e690a5f0"});
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<std::variant<model::Task, model::OperationOutcome>> result;
    ASSERT_NO_FATAL_FAILURE(result = taskActivate(task->prescriptionId(), task->accessCode(),
                                                  toCadesBesSignature(mvoPrescription, authoredOn), HttpStatus::OK));
}