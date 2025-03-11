/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Task.hxx"

#include <date/tz.h>
#include <erp/model/Composition.hxx>
#include <gtest/gtest.h>
#include <rapidjson/pointer.h>
#include <thread>// for std::this_thread::sleep_for

#include "shared/ErpConstants.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/model/ModelException.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/util/Uuid.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test_config.h"

namespace {
// definition of JSON pointers:
const std::string p_authoredOn           = "/authoredOn";
const std::string p_lastModified         = "/lastModified";
const std::string p_status               = "/status";
const std::string p_taskId               = "/id";
const std::string p_prescriptionId       = "/identifier/0/value";
const std::string p_accessCode           = "/identifier/1/value";
const std::string p_secret               = "/identifier/2/value";
const std::string p_ownerIdentifierSystem= "/owner/identifier/system";
const std::string p_ownerIdentifierValue = "/owner/identifier/value";
const std::string p_flowtypeCode         = "/extension/0/valueCoding/code";
const std::string p_flowtypeDisplay      = "/extension/0/valueCoding/display";
const std::string p_performerType        = "/performerType/0/coding/0/code";
const std::string p_performerTypeDisplay = "/performerType/0/coding/0/display";
const std::string p_performerTypeText    = "/performerType/0/text";
const std::string p_kvnr                 = "/for/identifier/value";
const std::string p_kvnrSystem           = "/for/identifier/system";
const std::string p_inputArray           = "/input";
const std::string p_outputArray          = "/output";
}

class TaskTest : public testing::Test
{
public:
    static void checkCommonTaskFields1(const model::Task& task)
    {
        ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_status)), "draft");
        ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_taskId)), "");
        ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_prescriptionId)), "");
        ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_accessCode)), "access_code");
        ASSERT_FALSE(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_lastModified)).empty());
        ASSERT_FALSE(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_authoredOn)).empty());
    }
    static void checkCommonTaskFields(const model::Task& task)
    {
        checkCommonTaskFields1(task);
        ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_performerType)),"urn:oid:1.2.276.0.76.4.54");
        ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_performerTypeDisplay)), "Öffentliche Apotheke");
        ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_performerTypeText)), "Öffentliche Apotheke");
    }
};

TEST_F(TaskTest, ConstructNewTask)
{
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    ASSERT_NO_FATAL_FAILURE(checkCommonTaskFields(task));
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeCode)), "160");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeDisplay)), "Muster 16 (Apothekenpflichtige Arzneimittel)");
}

TEST_F(TaskTest, ConstructNewTaskType162)
{
    model::Task task(model::PrescriptionType::digitaleGesundheitsanwendungen, "access_code");
    ASSERT_NO_FATAL_FAILURE(checkCommonTaskFields1(task));
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_performerType)),
              "urn:oid:1.2.276.0.76.4.59");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_performerTypeDisplay)),
              "Kostenträger");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_performerTypeText)), "Kostenträger");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeCode)), "162");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeDisplay)),
              "Muster 16 (Digitale Gesundheitsanwendungen)");
}

TEST_F(TaskTest, ConstructNewTaskType169)
{
    model::Task task(model::PrescriptionType::direkteZuweisung, "access_code");
    ASSERT_NO_FATAL_FAILURE(checkCommonTaskFields(task));
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeCode)), "169");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeDisplay)), "Muster 16 (Direkte Zuweisung)");
}

TEST_F(TaskTest, ConstructNewPkvTask)
{
    model::Task task(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, "access_code");
    ASSERT_NO_FATAL_FAILURE(checkCommonTaskFields(task));
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeCode)), "200");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeDisplay)), "PKV (Apothekenpflichtige Arzneimittel)");
}

TEST_F(TaskTest, ConstructNewPkvTaskType209)
{
    model::Task task(model::PrescriptionType::direkteZuweisungPkv, "access_code");
    ASSERT_NO_FATAL_FAILURE(checkCommonTaskFields(task));
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeCode)), "209");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeDisplay)), "PKV (Direkte Zuweisung)");
}

TEST_F(TaskTest, SetId)
{
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    const auto lastModified = task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_lastModified));
    task.setPrescriptionId(model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1));
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_taskId)), "160.000.000.000.001.54");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_prescriptionId)), "160.000.000.000.001.54");
    task.setPrescriptionId(model::PrescriptionId::fromString("160.100.000.000.000.42"));
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_taskId)), "160.100.000.000.000.42");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_prescriptionId)), "160.100.000.000.000.42");
    // setting the PrescriptionId does not change the lastModification time or the version.
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_lastModified)), lastModified);
}

TEST_F(TaskTest, UpdateLastUpdated)
{
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    const auto lastModified = task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_lastModified));

    // sleep to enforce that the timestamps change. The resolution is limited to 1sec.
    std::this_thread::sleep_for(std::chrono::seconds(1));

    task.updateLastUpdate();
    ASSERT_NE(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_lastModified)), lastModified);
}

TEST_F(TaskTest, SetStatus)//NOLINT(readability-function-cognitive-complexity)
{
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_status)), "draft");
    ASSERT_EQ(task.status(), model::Task::Status::draft);

    task.setStatus(model::Task::Status::ready);
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_status)), "ready");

    task.setStatus(model::Task::Status::cancelled);
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_status)), "cancelled");
    ASSERT_EQ(task.status(), model::Task::Status::cancelled);

    task.setStatus(model::Task::Status::completed);
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_status)), "completed");
    ASSERT_EQ(task.status(), model::Task::Status::completed);

    task.setStatus(model::Task::Status::inprogress);
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_status)), "in-progress");
    ASSERT_EQ(task.status(), model::Task::Status::inprogress);

    task.setStatus(model::Task::Status::draft);
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_status)), "draft");
    ASSERT_EQ(task.status(), model::Task::Status::draft);
}

TEST_F(TaskTest, SetKvnr)//NOLINT(readability-function-cognitive-complexity)
{
    using namespace std::string_literals;
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    ASSERT_EQ(rapidjson::Pointer(p_kvnr).Get(task.jsonDocument()), nullptr);
    ASSERT_FALSE(task.kvnr().has_value());

    task.setKvnr(model::Kvnr{"X123456788"s, model::Kvnr::Type::gkv});
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_kvnr)), "X123456788");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_kvnrSystem)),
              model::resource::naming_system::gkvKvid10);
    ASSERT_EQ(task.kvnr(), "X123456788");
}

TEST_F(TaskTest, FromJson)//NOLINT(readability-function-cognitive-complexity)
{
    const auto taskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4711);
    const auto task1 = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId, .kvnr = "X123456788"});
    ASSERT_NO_THROW((void)model::Task::fromJsonNoValidation(task1));
    const auto task = model::Task::fromJsonNoValidation(task1);
    ASSERT_EQ(task.status(), model::Task::Status::ready);
    const auto prescriptionId = task.prescriptionId();
    ASSERT_EQ(prescriptionId.toString(), "160.000.000.004.711.86");
    ASSERT_EQ(task.type(), model::PrescriptionType::apothekenpflichigeArzneimittel);
}

TEST_F(TaskTest, SetUuids)//NOLINT(readability-function-cognitive-complexity)
{
    auto id = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4711);
    model::Task task(id, model::PrescriptionType::apothekenpflichigeArzneimittel,
                     model::Timestamp::now(), model::Timestamp::now(),
                     model::Task::Status::completed, model::Timestamp::now());
    const auto uuid1 = id.deriveUuid(1);
    const auto uuid2 = id.deriveUuid(2);
    const auto uuid3 = id.deriveUuid(3);

    // no input and output
    ASSERT_EQ(rapidjson::Pointer(p_inputArray).Get(task.jsonDocument()), nullptr);
    ASSERT_TRUE(!task.patientConfirmationUuid().has_value());
    ASSERT_TRUE(!task.healthCarePrescriptionUuid().has_value());
    ASSERT_EQ(rapidjson::Pointer(p_outputArray).Get(task.jsonDocument()), nullptr);
    ASSERT_TRUE(!task.receiptUuid().has_value());

    // Set and check:
    task.setHealthCarePrescriptionUuid();
    task.setPatientConfirmationUuid();
    task.setReceiptUuid();
    ASSERT_EQ(task.healthCarePrescriptionUuid().value_or(""), uuid1);
    ASSERT_EQ(task.patientConfirmationUuid().value_or(""), uuid2);
    ASSERT_EQ(task.receiptUuid().value_or(""), uuid3);
    ASSERT_EQ(rapidjson::Pointer(p_inputArray).Get(task.jsonDocument())->GetArray().Size(), static_cast<rapidjson::SizeType>(2));
    ASSERT_EQ(rapidjson::Pointer(p_outputArray).Get(task.jsonDocument())->GetArray().Size(), static_cast<rapidjson::SizeType>(1));
}

TEST_F(TaskTest, ExpiryDate)
{
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    model::Timestamp timestamp = model::Timestamp::now();
    ASSERT_NO_THROW(task.setExpiryDate(timestamp));
    ASSERT_EQ(task.expiryDate(), model::Timestamp::fromGermanDate(timestamp.toGermanDate()));
}

TEST_F(TaskTest, AcceptDate)
{
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    model::Timestamp timestamp = model::Timestamp::now();
    ASSERT_NO_THROW(task.setAcceptDate(timestamp));
    ASSERT_EQ(task.acceptDate(), model::Timestamp::fromGermanDate(timestamp.toGermanDate()));
}


TEST_F(TaskTest, AcceptDate28Days)
{
    A_19445_10.test("Task.AcceptDate = <Date of QES Creation + 28 days");
    using namespace date;
    using namespace date::literals;
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    auto baseDate = date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / April / 23});
    task.setAcceptDate(model::Timestamp(baseDate.get_sys_time()), model::KbvStatusKennzeichen::asvKennzeichen, 3);
    auto expected = baseDate.get_sys_time() + days{28};
    ASSERT_EQ(task.acceptDate().toChronoTimePoint(), expected);
    A_19445_10.finish();
}

TEST_F(TaskTest, AcceptDate28DaysType169)
{
    A_19445_10.test("Task.AcceptDate = <Date of QES Creation + 28 days");
    using namespace date;
    using namespace date::literals;
    model::Task task(model::PrescriptionType::direkteZuweisung, "access_code");
    auto baseDate = date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / April / 23});
    task.setAcceptDate(model::Timestamp(baseDate.get_sys_time()), model::KbvStatusKennzeichen::asvKennzeichen, 3);
    auto expected = baseDate.get_sys_time() + days{28};
    ASSERT_EQ(task.acceptDate().toChronoTimePoint(), expected);
    A_19445_10.finish();
}

TEST_F(TaskTest, AcceptDate3Months)
{
    A_19445_10.test("Task.AcceptDate = <Date of QES Creation + 3 months");
    using namespace date;
    using namespace date::literals;
    model::Task task(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, "access_code");
    auto baseDate = date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / August / 31});
    task.setAcceptDate(model::Timestamp(baseDate.get_sys_time()), model::KbvStatusKennzeichen::asvKennzeichen, 3);
    auto expected = model::Timestamp(
        date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / November / 30}).get_sys_time());
    ASSERT_EQ(task.acceptDate().toXsDateTime(), expected.toXsDateTime());
    A_19445_10.finish();
}

TEST_F(TaskTest, AcceptDate3MonthsType209)
{
    A_19445_10.test("Task.AcceptDate = <Date of QES Creation + 3 months");
    using namespace date;
    using namespace date::literals;
    model::Task task(model::PrescriptionType::direkteZuweisungPkv, "access_code");
    auto baseDate = date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / August / 31});
    task.setAcceptDate(model::Timestamp(baseDate.get_sys_time()), model::KbvStatusKennzeichen::asvKennzeichen, 3);
    auto expected = model::Timestamp(
        date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / November / 30}).get_sys_time());
    ASSERT_EQ(task.acceptDate().toXsDateTime(), expected.toXsDateTime());
    A_19445_10.finish();
}

TEST_F(TaskTest, AcceptDate3MonthsType162)
{
    A_19445_10.test("Task.AcceptDate = <Date of QES Creation + 3 months");
    using namespace date;
    using namespace date::literals;
    model::Task task(model::PrescriptionType::digitaleGesundheitsanwendungen, "access_code");
    auto baseDate = date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / August / 31});
    task.setAcceptDate(model::Timestamp(baseDate.get_sys_time()), model::KbvStatusKennzeichen::asvKennzeichen, 3);
    auto expected = model::Timestamp(
        date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / November / 30}).get_sys_time());
    ASSERT_EQ(task.acceptDate().toXsDateTime(), expected.toXsDateTime());
    A_19445_10.finish();
}

TEST_F(TaskTest, AcceptDate3WorkDays)
{
    using namespace date;
    using namespace date::literals;
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    auto baseDate = date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / April / 23});
    task.setAcceptDate(model::Timestamp(baseDate.get_sys_time()),
                       model::KbvStatusKennzeichen::entlassmanagementKennzeichen, 3);
    auto expected = model::Timestamp(
        date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / April / 26}).get_sys_time());
    ASSERT_EQ(task.acceptDate().toXsDateTime(), expected.toXsDateTime());
}

TEST_F(TaskTest, AcceptDate6WorkDaysOverHolidays)
{
    using namespace date;
    using namespace date::literals;
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    auto baseDate = date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / December / 23});
    task.setAcceptDate(model::Timestamp(baseDate.get_sys_time()),
                       model::KbvStatusKennzeichen::entlassmanagementKennzeichen, 6);
    auto expected = model::Timestamp(
        date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / December / 30})
            .get_sys_time());
    ASSERT_EQ(task.acceptDate().toXsDateTime(), expected.toXsDateTime());
}


TEST_F(TaskTest, DeleteAccessCode)//NOLINT(readability-function-cognitive-complexity)
{
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    ASSERT_EQ(task.accessCode(), "access_code");
    task.deleteAccessCode();
    ASSERT_EQ(rapidjson::Pointer(p_accessCode).Get(task.jsonDocument()), nullptr);
    std::string_view accessCode;
    ASSERT_THROW(accessCode = task.accessCode(), model::ModelException);
}

TEST_F(TaskTest, SetAndDeleteSecret)//NOLINT(readability-function-cognitive-complexity)
{
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");

    ASSERT_EQ(rapidjson::Pointer(p_secret).Get(task.jsonDocument()), nullptr);
    ASSERT_FALSE(task.secret().has_value());

    const std::string_view secret = "Secret";
    task.setSecret(secret);
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_secret)), secret);
    ASSERT_EQ(task.secret(), secret);

    task.deleteSecret();
    ASSERT_EQ(rapidjson::Pointer(p_secret).Get(task.jsonDocument()), nullptr);
    ASSERT_FALSE(task.secret().has_value());
}

TEST_F(TaskTest, SetAndDeleteOwner)//NOLINT(readability-function-cognitive-complexity)
{
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");

    ASSERT_EQ(rapidjson::Pointer(p_ownerIdentifierSystem).Get(task.jsonDocument()), nullptr);
    ASSERT_EQ(rapidjson::Pointer(p_ownerIdentifierValue).Get(task.jsonDocument()), nullptr);
    ASSERT_FALSE(task.owner().has_value());

    const std::string_view owner = "Owner";
    task.setOwner(owner);
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_ownerIdentifierSystem)),
              model::resource::naming_system::telematicID);
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_ownerIdentifierValue)), owner);
    ASSERT_EQ(task.owner(), owner);

    task.deleteOwner();
    ASSERT_EQ(rapidjson::Pointer(p_ownerIdentifierSystem).Get(task.jsonDocument()), nullptr);
    ASSERT_EQ(rapidjson::Pointer(p_ownerIdentifierValue).Get(task.jsonDocument()), nullptr);
    ASSERT_FALSE(task.owner().has_value());
}
