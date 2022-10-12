/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef ERP_PROCESSING_CONTEXT_A_22068_MEHRFACHVERORDNUNGABLEHNEN_H
#define ERP_PROCESSING_CONTEXT_A_22068_MEHRFACHVERORDNUNGABLEHNEN_H

#include <gtest/gtest.h>

#include "erp/ErpRequirements.hxx"
#include "erp/model/OperationOutcome.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <date/tz.h>


class Mehrfachverordnung : public ErpWorkflowTest
{
public:
    Mehrfachverordnung()
    {
        auto bundle = resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_mehrfachverordnung.xml");
        mvoPrescription = patchVersionsInBundle(bundle);
    }
    void createTaskMember(model::PrescriptionType type = model::PrescriptionType::apothekenpflichigeArzneimittel)
    {
        auto kvnr = generateNewRandomKVNR();
        ASSERT_NO_FATAL_FAILURE(task = taskCreate(type));
        ASSERT_TRUE(task.has_value());
        mvoPrescription =
            String::replaceAll(mvoPrescription, "160.000.000.004.713.80", task->prescriptionId().toString());
    }
    void TestStep(Requirement& requirement, const std::string& description)
    {
        requirement.test(description.c_str());
        RecordProperty("Description", description);
    }
    ResourceManager& resourceManager = ResourceManager::instance();
    std::string mvoPrescription;
    std::optional<model::Task> task;
};

class MVO_A_22628 : public Mehrfachverordnung
{
public:
    void SetUp() override
    {
        createTaskMember();
        mvoPrescription = String::replaceAll(mvoPrescription, "####LEGAL_BASIS_CODE####", "00");
    }
};

TEST_F(MVO_A_22628, Step_01_Denominator_5)
{
    TestStep(A_22628, "ERP-A_22628.01 - Task aktivieren - Mehrfachverordnung - Denominator gleich 5");
    mvoPrescription = String::replaceAll(mvoPrescription, "####NUMERATOR####", "3");
    mvoPrescription = String::replaceAll(mvoPrescription, "####DENOMINATOR####", "5");
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    ASSERT_NO_FATAL_FAILURE(
        taskActivate(task->prescriptionId(), task->accessCode(),
                     toCadesBesSignature(mvoPrescription, model::Timestamp::fromXsDate("2020-02-03")),
                     HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid,
                     "Eine Mehrfachverordnungen darf aus maximal 4 Teilverordnungen bestehen"));
}
TEST_F(MVO_A_22628, Step_02_Numerator_5)
{
    TestStep(A_22628,"ERP-A_22628.02 - Task aktivieren - Mehrfachverordnung - Numerator gleich 5");
    mvoPrescription = String::replaceAll(mvoPrescription, "####NUMERATOR####", "5");
    mvoPrescription = String::replaceAll(mvoPrescription, "####DENOMINATOR####", "4");
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    ASSERT_NO_FATAL_FAILURE(
        taskActivate(task->prescriptionId(), task->accessCode(),
                     toCadesBesSignature(mvoPrescription, model::Timestamp::fromXsDate("2020-02-03")),
                     HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid,
                     "Eine Mehrfachverordnungen darf aus maximal 4 Teilverordnungen bestehen"));
}

class MVO_A_22704 : public Mehrfachverordnung
{
public:
};

TEST_F(MVO_A_22704, Step_01_Numerator_0)
{
    TestStep(A_22704,"ERP-A_22704.01 - Task aktivieren - Mehrfachverordnung - Numerator gleich 0");
    mvoPrescription = String::replaceAll(mvoPrescription, "####NUMERATOR####", "0");
    mvoPrescription = String::replaceAll(mvoPrescription, "####DENOMINATOR####", "3");
    mvoPrescription = String::replaceAll(mvoPrescription, "####LEGAL_BASIS_CODE####", "00");
    createTaskMember();
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    ASSERT_NO_FATAL_FAILURE(
        taskActivate(task->prescriptionId(), task->accessCode(),
                     toCadesBesSignature(mvoPrescription, model::Timestamp::fromXsDate("2020-02-03")),
                     HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid,
                     "Für eine Mehrfachverordnungen muss der numerator größer 0 sein."));
}

class MVO_A_22629 : public Mehrfachverordnung
{
public:
};

TEST_F(MVO_A_22629, Step_01_Denominator_1)
{
    TestStep(A_22629,"ERP-A_22629.01 - Task aktivieren - Mehrfachverordnung - Denominator gleich 1");
    mvoPrescription = String::replaceAll(mvoPrescription, "####NUMERATOR####", "1");
    mvoPrescription = String::replaceAll(mvoPrescription, "####DENOMINATOR####", "1");
    mvoPrescription = String::replaceAll(mvoPrescription, "####LEGAL_BASIS_CODE####", "00");
    createTaskMember();
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    ASSERT_NO_FATAL_FAILURE(
        taskActivate(task->prescriptionId(), task->accessCode(),
                     toCadesBesSignature(mvoPrescription, model::Timestamp::fromXsDate("2020-02-03")),
                     HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid,
                     "Eine Mehrfachverordnung muss aus mindestens 2 Teilverordnungen bestehen"));
}

class MVO_A_22630 : public Mehrfachverordnung
{
};

TEST_F(MVO_A_22630, Step_01_Numerator_greater_Denominator)
{
    TestStep(A_22630,"ERP-A_22630.01 - E-Rezept-Fachdienst - Task aktivieren - Mehrfachverordnung - Numerator > Denominator");
    mvoPrescription = String::replaceAll(mvoPrescription, "####NUMERATOR####", "4");
    mvoPrescription = String::replaceAll(mvoPrescription, "####DENOMINATOR####", "3");
    mvoPrescription = String::replaceAll(mvoPrescription, "####LEGAL_BASIS_CODE####", "00");
    createTaskMember();
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    ASSERT_NO_FATAL_FAILURE(taskActivate(
        task->prescriptionId(), task->accessCode(),
        toCadesBesSignature(mvoPrescription, model::Timestamp::fromXsDate("2020-02-03")), HttpStatus::BadRequest,
        model::OperationOutcome::Issue::Type::invalid, "Numerator ist größer als denominator"));
}

class MVO_A_22631 : public Mehrfachverordnung
{
public:
protected:
    void SetUp() override
    {
        createTaskMember();
        mvoPrescription = String::replaceAll(mvoPrescription, "####LEGAL_BASIS_CODE####", "00");
    }
};

TEST_F(MVO_A_22631, Step_01_Zeitraum)
{
    TestStep(A_22631,"ERP-A_22631.01 - Task aktivieren - Mehrfachverordnung - Verordnung mit Zeitraum");
    auto bundle =
        resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_mehrfachverordnung_A_22631_2.xml");
    mvoPrescription = patchVersionsInBundle(bundle);
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    ASSERT_NO_FATAL_FAILURE(taskActivate(
        task->prescriptionId(), task->accessCode(),
        toCadesBesSignature(mvoPrescription, model::Timestamp::fromXsDate("2020-02-03")), HttpStatus::BadRequest,
        model::OperationOutcome::Issue::Type::invalid, "Zeitraum darf nur bei MVO angegeben werden"));
}

TEST_F(MVO_A_22631, Step_02_Nummerierung)
{
    TestStep(A_22631,"ERP-A_22631.02 - Task aktivieren - Mehrfachverordnung - Verordnung mit Nummerierung");
    auto bundle =
        resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_mehrfachverordnung_A_22631_1.xml");
    mvoPrescription = patchVersionsInBundle(bundle);
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    ASSERT_NO_FATAL_FAILURE(taskActivate(
        task->prescriptionId(), task->accessCode(),
        toCadesBesSignature(mvoPrescription, model::Timestamp::fromXsDate("2020-02-03")), HttpStatus::BadRequest,
        model::OperationOutcome::Issue::Type::invalid, "Nummerierung darf nur bei MVO angegeben werden"));
}

class MVO_A_22632 : public Mehrfachverordnung
{
public:
    void SetUp() override
    {
        createTaskMember();
        mvoPrescription = String::replaceAll(mvoPrescription, "####NUMERATOR####", "2");
        mvoPrescription = String::replaceAll(mvoPrescription, "####DENOMINATOR####", "4");
    }
};

TEST_F(MVO_A_22632, Step_01_Code_04)
{
    TestStep(A_22632,"ERP-A_22632.01 - Task aktivieren - Mehrfachverordnung - Entlassrezept Code 04 ablehnen");
    mvoPrescription = String::replaceAll(mvoPrescription, "####LEGAL_BASIS_CODE####", "04");
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    ASSERT_NO_FATAL_FAILURE(taskActivate(
        task->prescriptionId(), task->accessCode(),
        toCadesBesSignature(mvoPrescription, model::Timestamp::fromXsDate("2020-02-03")), HttpStatus::BadRequest,
        model::OperationOutcome::Issue::Type::invalid, "Für Entlassrezepte sind keine Mehrfachverordnungen zulässig"));
}

TEST_F(MVO_A_22632, Step_02_Code_14)
{
    TestStep(A_22632,"ERP-A_22632.02 - Task aktivieren - Mehrfachverordnung - Entlassrezept Code 14 ablehnen");
    mvoPrescription = String::replaceAll(mvoPrescription, "####LEGAL_BASIS_CODE####", "14");
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    ASSERT_NO_FATAL_FAILURE(taskActivate(
        task->prescriptionId(), task->accessCode(),
        toCadesBesSignature(mvoPrescription, model::Timestamp::fromXsDate("2020-02-03")), HttpStatus::BadRequest,
        model::OperationOutcome::Issue::Type::invalid, "Für Entlassrezepte sind keine Mehrfachverordnungen zulässig"));
}

class MVO_A_22633 : public Mehrfachverordnung
{
public:
    void SetUp() override
    {
        createTaskMember();
        mvoPrescription = String::replaceAll(mvoPrescription, "####NUMERATOR####", "2");
        mvoPrescription = String::replaceAll(mvoPrescription, "####DENOMINATOR####", "4");
    }
};

TEST_F(MVO_A_22633, Step_01_Code_10)
{
    TestStep(A_22633,"ERP-A_22633.01 - Task aktivieren - Ersatzverordnung als Mehrfachverordnung ablehnen - Code 10");
    mvoPrescription = String::replaceAll(mvoPrescription, "####LEGAL_BASIS_CODE####", "10");
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    ASSERT_NO_FATAL_FAILURE(
        taskActivate(task->prescriptionId(), task->accessCode(),
                     toCadesBesSignature(mvoPrescription, model::Timestamp::fromXsDate("2020-02-03")),
                     HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid,
                     "Für Ersatzverordnung sind keine Mehrfachverordnungen zulässig"));
}
TEST_F(MVO_A_22633, Step_02_Code_11)
{
    TestStep(A_22633,"ERP-A_22633.02 - Task aktivieren - Ersatzverordnung als Mehrfachverordnung ablehnen - Code 11");
    mvoPrescription = String::replaceAll(mvoPrescription, "####LEGAL_BASIS_CODE####", "11");
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    ASSERT_NO_FATAL_FAILURE(
        taskActivate(task->prescriptionId(), task->accessCode(),
                     toCadesBesSignature(mvoPrescription, model::Timestamp::fromXsDate("2020-02-03")),
                     HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid,
                     "Für Ersatzverordnung sind keine Mehrfachverordnungen zulässig"));
}
TEST_F(MVO_A_22633, Step_03_Code_17)
{
    TestStep(A_22633,"ERP-A_22633.03 - Task aktivieren - Ersatzverordnung als Mehrfachverordnung ablehnen - Code 17");
    mvoPrescription = String::replaceAll(mvoPrescription, "####LEGAL_BASIS_CODE####", "17");
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    ASSERT_NO_FATAL_FAILURE(
        taskActivate(task->prescriptionId(), task->accessCode(),
                     toCadesBesSignature(mvoPrescription, model::Timestamp::fromXsDate("2020-02-03")),
                     HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid,
                     "Für Ersatzverordnung sind keine Mehrfachverordnungen zulässig"));
}

class MVO_A_22634 : public Mehrfachverordnung
{
public:
    MVO_A_22634()
    {
        auto bundle =
            resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_mehrfachverordnung_A_22634.xml");
        mvoPrescription = patchVersionsInBundle(bundle);
        createTaskMember();
    }
};

TEST_F(MVO_A_22634, Step_01_MissingStart)
{
    TestStep(A_22634,"ERP-A_22634.01 - Task aktivieren - Mehrfachverordnung - Beginn Einlösefrist nicht angegeben");
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    ASSERT_NO_FATAL_FAILURE(taskActivate(
        task->prescriptionId(), task->accessCode(),
        toCadesBesSignature(mvoPrescription, model::Timestamp::fromXsDate("2020-02-03")), HttpStatus::BadRequest,
        model::OperationOutcome::Issue::Type::invalid, "Beginn der Einlösefrist für MVO ist erforderlich"));
}

class A_22635Test : public Mehrfachverordnung
{
public:
    A_22635Test()
    {
        auto bundle =
            resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_mehrfachverordnung_A_22635.xml");
        mvoPrescription = patchVersionsInBundle(bundle);
        createTaskMember();
    }
};

TEST_F(A_22635Test, Step_01_FutureStartDate)
{
    TestStep(A_22635,"01 Apotheker ruft Accept Task für eine MVO mit Start-Datum in der Zukunft");
    date::year_month_day today = floor<date::days>(std::chrono::system_clock::now());
    date::year_month_day tomorrow = date::sys_days{today} + date::days(1);
    auto tomorrowStr = date::format("%F", tomorrow);
    mvoPrescription = String::replaceAll(mvoPrescription, "####START####", tomorrowStr);
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    ASSERT_NO_FATAL_FAILURE(
        taskActivate(task->prescriptionId(), task->accessCode(),
                     toCadesBesSignature(mvoPrescription, model::Timestamp::fromXsDate("2020-02-03"))));
    ASSERT_NO_FATAL_FAILURE(taskAccept(task->prescriptionId(), std::string{task->accessCode()}, HttpStatus::Forbidden,
                                       model::OperationOutcome::Issue::Type::forbidden,
                                       "Teilverordnung ab " + tomorrowStr + " einlösbar."));
}

class A_19445Test : public Mehrfachverordnung
{
public:
protected:
    void SetUp() override
    {
        Mehrfachverordnung::SetUp();
        auto bundle =
            resourceManager.getStringResource("test/EndpointHandlerTest/kbv_bundle_mehrfachverordnung_A_19445.xml");
        mvoPrescription = patchVersionsInBundle(bundle);
        createTaskMember();
    }
};

TEST_F(A_19445Test, ExpiryAcceptDate365)
{
    using namespace date;
    TestStep(A_19445_08,"01 signing date + 365 days");
    mvoPrescription = String::replaceAll(mvoPrescription, "####END_DATE####", "");
    auto signingTime = model::Timestamp::fromXsDate("2020-02-03");
    auto expectedDate =
        date::make_zoned(model::Timestamp::GermanTimezone, floor<date::days>(date::local_days{2021_y / 02 / 02}))
            .get_sys_time();
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<model::Task> activatedTask;
    ASSERT_NO_FATAL_FAILURE(activatedTask = taskActivate(task->prescriptionId(), task->accessCode(),
                                                         toCadesBesSignature(mvoPrescription, signingTime)));
    ASSERT_TRUE(activatedTask.has_value());
    EXPECT_EQ(activatedTask->acceptDate(), model::Timestamp(expectedDate));
    EXPECT_EQ(activatedTask->expiryDate(), model::Timestamp(expectedDate));
}

TEST_F(A_19445Test, ExpiryAcceptDateEndDate)
{
    TestStep(A_19445_08,"02 signing date given");
    mvoPrescription = String::replaceAll(mvoPrescription, "####END_DATE####", "<end value=\"2021-03-01\"/>");
    auto signingTime = model::Timestamp::fromXsDate("2020-02-03");
    auto expectedDate = model::Timestamp::fromGermanDate("2021-03-01");
    RecordProperty("Prescription", Base64::encode(mvoPrescription));
    std::optional<model::Task> activatedTask;
    ASSERT_NO_FATAL_FAILURE(activatedTask = taskActivate(task->prescriptionId(), task->accessCode(),
                                                         toCadesBesSignature(mvoPrescription, signingTime)));
    ASSERT_TRUE(activatedTask.has_value());
    EXPECT_EQ(activatedTask->acceptDate(), expectedDate);
    EXPECT_EQ(activatedTask->expiryDate(), expectedDate);
}

#endif// ERP_PROCESSING_CONTEXT_A_22068_MEHRFACHVERORDNUNGABLEHNEN_H
