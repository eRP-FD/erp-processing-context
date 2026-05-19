/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/Task.hxx"

#include "shared/ErpRequirements.hxx"
#include "shared/model/Composition.hxx"
#include "shared/model/ModelException.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/util/FileHelper.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test_config.h"


#include <gtest/gtest.h>
#include <rapidjson/pointer.h>
#include <date/tz.h>
#include <thread>// for std::this_thread::sleep_for


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
    if (task.getProfileVersionChecked() < model::version::GEM_ERP_1_6)
    {
        ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeDisplay)), "Muster 16 (Apothekenpflichtige Arzneimittel)");
    }
    else
    {
        ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeDisplay)), "Flowtype für Apothekenpflichtige Arzneimittel");
    }
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
    if (task.getProfileVersionChecked() < model::version::GEM_ERP_1_6)
    {
        ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeDisplay)),
                  "Muster 16 (Digitale Gesundheitsanwendungen)");
    }
    else
    {
        ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeDisplay)),
                  "Flowtype für Digitale Gesundheitsanwendungen");
    }
}

TEST_F(TaskTest, ConstructNewTaskType169)
{
    model::Task task(model::PrescriptionType::direkteZuweisung, "access_code");
    ASSERT_NO_FATAL_FAILURE(checkCommonTaskFields(task));
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeCode)), "169");
    if (task.getProfileVersionChecked() < model::version::GEM_ERP_1_6)
    {
        ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeDisplay)), "Muster 16 (Direkte Zuweisung)");
    }
    else
    {
        ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeDisplay)), "Flowtype zur Workflow-Steuerung durch Leistungserbringer");
    }
}

TEST_F(TaskTest, ConstructNewPkvTask)
{
    model::Task task(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, "access_code");
    ASSERT_NO_FATAL_FAILURE(checkCommonTaskFields(task));
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeCode)), "200");
    if (task.getProfileVersionChecked() < model::version::GEM_ERP_1_6)
    {
        ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeDisplay)), "PKV (Apothekenpflichtige Arzneimittel)");
    }
    else
    {
        ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeDisplay)), "Flowtype für Apothekenpflichtige Arzneimittel (PKV)");
    }
}

TEST_F(TaskTest, ConstructNewPkvTaskType209)
{
    model::Task task(model::PrescriptionType::direkteZuweisungPkv, "access_code");
    ASSERT_NO_FATAL_FAILURE(checkCommonTaskFields(task));
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeCode)), "209");
    if (task.getProfileVersionChecked() < model::version::GEM_ERP_1_6)
    {
        ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeDisplay)), "PKV (Direkte Zuweisung)");
    }
    else
    {
        ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeDisplay)), "Flowtype zur Workflow-Steuerung durch Leistungserbringer (PKV)");
    }
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

    task.setKvnr(model::Kvnr{"X123456788"s});
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
                     model::Task::Status::completed, model::Timestamp::now(), false);
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
    A_27844.test("Task.AcceptDate = <Date of QES Creation + 28 days");
    using namespace date;
    using namespace date::literals;
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    auto baseDate = date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / April / 23});
    task.setAcceptDate(model::Timestamp(baseDate.get_sys_time()), model::KbvStatusKennzeichen::asvKennzeichen, 3);
    auto expected = baseDate.get_sys_time() + days{28};
    ASSERT_EQ(task.acceptDate().toChronoTimePoint(), expected);
    A_27844.finish();
}

TEST_F(TaskTest, AcceptDate6DaysType166)
{
    A_27846.test("Task.AcceptDate = <Datum der QES.Erstellung im Signaturobjekt> +6 Kalendertage");
    using namespace date;
    using namespace date::literals;
    model::Task task(model::PrescriptionType::tRezept, "access_code");
    auto baseDate = date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / April / 23});
    task.setAcceptDate(model::Timestamp(baseDate.get_sys_time()), model::KbvStatusKennzeichen::asvKennzeichen, 3);
    auto expected = baseDate.get_sys_time() + days{6};
    ASSERT_EQ(task.acceptDate().toChronoTimePoint(), expected);
    A_27846.finish();
}

TEST_F(TaskTest, AcceptDate28DaysType169)
{
    A_27847.test("Task.AcceptDate = <Date of QES Creation + 28 days");
    using namespace date;
    using namespace date::literals;
    model::Task task(model::PrescriptionType::direkteZuweisung, "access_code");
    auto baseDate = date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / April / 23});
    task.setAcceptDate(model::Timestamp(baseDate.get_sys_time()), model::KbvStatusKennzeichen::asvKennzeichen, 3);
    auto expected = baseDate.get_sys_time() + days{28};
    ASSERT_EQ(task.acceptDate().toChronoTimePoint(), expected);
    A_27847.finish();
}

TEST_F(TaskTest, AcceptDate3Months)
{
    A_27848.test("Task.AcceptDate = <Date of QES Creation + 3 months");
    using namespace date;
    using namespace date::literals;
    model::Task task(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, "access_code");
    auto baseDate = date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / August / 31});
    task.setAcceptDate(model::Timestamp(baseDate.get_sys_time()), model::KbvStatusKennzeichen::asvKennzeichen, 3);
    auto expected = model::Timestamp(
        date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / November / 30}).get_sys_time());
    ASSERT_EQ(task.acceptDate().toXsDateTime(), expected.toXsDateTime());
    A_27848.finish();
}

TEST_F(TaskTest, AcceptDate3MonthsType209)
{
    A_27849.test("Task.AcceptDate = <Date of QES Creation + 3 months");
    using namespace date;
    using namespace date::literals;
    model::Task task(model::PrescriptionType::direkteZuweisungPkv, "access_code");
    auto baseDate = date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / August / 31});
    task.setAcceptDate(model::Timestamp(baseDate.get_sys_time()), model::KbvStatusKennzeichen::asvKennzeichen, 3);
    auto expected = model::Timestamp(
        date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / November / 30}).get_sys_time());
    ASSERT_EQ(task.acceptDate().toXsDateTime(), expected.toXsDateTime());
    A_27849.finish();
}

TEST_F(TaskTest, AcceptDate3MonthsType162)
{
    A_27845.test("Task.AcceptDate = <Date of QES Creation + 3 months");
    using namespace date;
    using namespace date::literals;
    model::Task task(model::PrescriptionType::digitaleGesundheitsanwendungen, "access_code");
    auto baseDate = date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / August / 31});
    task.setAcceptDate(model::Timestamp(baseDate.get_sys_time()), model::KbvStatusKennzeichen::asvKennzeichen, 3);
    auto expected = model::Timestamp(
        date::make_zoned(model::Timestamp::GermanTimezone, date::local_days{2021_y / November / 30}).get_sys_time());
    ASSERT_EQ(task.acceptDate().toXsDateTime(), expected.toXsDateTime());
    A_27845.finish();
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

struct TaskAcceptDateTestParam {
    std::string name;
    std::string baseTime;
    std::string acceptDate;
    std::source_location where{std::source_location::current()};
};

class TaskAcceptDateTest
    : public TaskTest,
      public testing::WithParamInterface<std::tuple<model::PrescriptionType, TaskAcceptDateTestParam>>
{
public:
    static std::string name(const testing::TestParamInfo<ParamType>& info)
    {
        std::string result{magic_enum::enum_name(get<model::PrescriptionType>(info.param))};
        result.append(1, '_').append(get<TaskAcceptDateTestParam>(info.param).name);
        return result;
    }
};
void PrintTo(const TaskAcceptDateTestParam& p, std::ostream* out)
{
    (*out) << R"({ "name": ")" << p.name << R"(", )";
    (*out) << R"("baseTime": ")" << p.baseTime << R"(", )";
    (*out) << R"("acceptDate": ")" << p.acceptDate << R"(", )";
    (*out) << R"("where": ")" << std::filesystem::path{p.where.file_name()}.filename().native() << ":" << p.where.line()
           << ":" << p.where.column() << R"(" })";
}

TEST_P(TaskAcceptDateTest, run)
{
    const auto& [prescriptionType, p] = GetParam();
    A_27844.test("Task.AcceptDate = <Date of QES Creation + 28 days");
    std::istringstream baseTimeStream{p.baseTime};
    auto baseTime = model::Timestamp{model::Timestamp::timepoint_t::max()};
    if (p.baseTime.find('+') == std::string::npos)
    {
        date::local_time<model::Timestamp::duration_t> parsedTime;
        date::from_stream(baseTimeStream, "%Y-%m-%dT%H:%M", parsedTime);
        ASSERT_FALSE(baseTimeStream.fail()) << p.baseTime;
        baseTime = model::Timestamp{date::make_zoned(model::Timestamp::GermanTimezone, parsedTime).get_sys_time()};
    }
    else
    {
        date::sys_time<model::Timestamp::duration_t> parsedTime;
        date::from_stream(baseTimeStream, "%Y-%m-%dT%H:%M:%S%z", parsedTime);
        ASSERT_FALSE(baseTimeStream.fail()) << p.baseTime;
        baseTime = model::Timestamp{parsedTime};
    }

    model::Task task(prescriptionType, "access_code");
    task.setAcceptDate(baseTime, model::KbvStatusKennzeichen::asvKennzeichen, 3);
    ASSERT_EQ(task.acceptDate().toGermanDate(), p.acceptDate);
    A_27844.finish();
}

INSTANTIATE_TEST_SUITE_P(in6Days, TaskAcceptDateTest,
                         testing::Combine(//
                             ::testing::Values(model::PrescriptionType::tRezept),
                             ::testing::ValuesIn<std::list<TaskAcceptDateTestParam>>({
                                 {"yearChange_midnight", "2025-12-29T00:00", "2026-01-04"},
                                 {"yearChange_early", "2025-12-29T00:30", "2026-01-04"},
                                 {"yearChange_late", "2025-12-29T23:30", "2026-01-04"},

                                 {"winter_midnight", "2026-01-01T00:00", "2026-01-07"},
                                 {"winter_early", "2026-01-01T00:30", "2026-01-07"},
                                 {"winter_late", "2026-01-01T23:30", "2026-01-07"},

                                 {"winter_summer_midnight", "2026-03-24T00:00", "2026-03-30"},
                                 {"winter_summer_early", "2026-03-24T00:30", "2026-03-30"},
                                 {"winter_summer_late", "2026-03-24T23:30", "2026-03-30"},

                                 {"summer_midnight", "2026-06-23T00:00", "2026-06-29"},
                                 {"summer_early", "2026-06-23T00:30", "2026-06-29"},
                                 {"summer_late", "2026-06-23T23:30", "2026-06-29"},

                                 {"summer_winter_midnight", "2026-10-23T00:00", "2026-10-29"},
                                 {"summer_winter_early", "2026-10-23T00:30", "2026-10-29"},
                                 {"summer_winter_late", "2026-10-23T23:30", "2026-10-29"},
                             }))//
                         ,
                         &TaskAcceptDateTest::name);

INSTANTIATE_TEST_SUITE_P(in6Days_DaylightSaving, TaskAcceptDateTest,
                         testing::Combine(//
                             ::testing::Values(model::PrescriptionType::tRezept),
                             ::testing::ValuesIn<std::list<TaskAcceptDateTestParam>>({
                                 {"toAtChange_winter_summer", "2026-03-23T02:30", "2026-03-29"},
                                 {"toBeforeChange_winter_summer", "2026-03-23T02:00", "2026-03-29"},
                                 {"toAfterChange_winter_summer", "2026-03-23T03:00", "2026-03-29"},

                                 {"fromBeforeChange_winter_summer", "2026-03-29T02:00:00+01:00", "2026-04-04"},
                                 {"fromAfterChange_winter_summer", "2026-03-29T03:00", "2026-04-04"},

                                 {"toAtChange_summer_winter", "2026-10-19T02:30", "2026-10-25"},
                                 {"toBeforeChange_summer_winter", "2026-10-19T02:00", "2026-10-25"},
                                 {"toAfterChange_summer_winter", "2026-10-19T03:00", "2026-10-25"},

                                 {"fromChange_summer_winter_before", "2026-10-25T02:30:00+02:00", "2026-10-31"},
                                 {"fromChange_summer_winter_after", "2026-10-25T02:30:00+01:00", "2026-10-31"},
                                 {"fromBeforeChange_summer_winter", "2026-10-25T02:00:00+02:00", "2026-10-31"},
                                 {"fromAfterChange_summer_winter", "2026-10-25T03:00:00+01:00", "2026-10-31"},
                             }))//
                         ,
                         &TaskAcceptDateTest::name);

INSTANTIATE_TEST_SUITE_P(in6Days_LeapYear, TaskAcceptDateTest,
                         testing::Combine(//
                             ::testing::Values(model::PrescriptionType::tRezept),
                             ::testing::ValuesIn<std::list<TaskAcceptDateTestParam>>({
                                 {"onto_LeapDay_midnight", "2028-02-23T00:00", "2028-02-29"},
                                 {"onto_LeapDay_early", "2028-02-23T00:30", "2028-02-29"},
                                 {"onto_LeapDay_late", "2028-02-23T23:30", "2028-02-29"},

                                 {"over_LeapDay_midnight", "2028-02-24T00:00", "2028-03-01"},
                                 {"over_LeapDay_early", "2028-02-24T00:30", "2028-03-01"},
                                 {"over_LeapDay_late", "2028-02-24T23:30", "2028-03-01"},

                                 {"from_LeapDay_midnight", "2028-02-29T00:00", "2028-03-06"},
                                 {"from_LeapDay_early", "2028-02-29T00:30", "2028-03-06"},
                                 {"from_LeapDay_late", "2028-02-29T23:30", "2028-03-06"},

                                 {"noLeapDay_midnight", "2026-02-23T00:00", "2026-03-01"},
                                 {"noLeapDay_early", "2026-02-23T00:30", "2026-03-01"},
                                 {"noLeapDay_late", "2026-02-23T23:30", "2026-03-01"},
                             }))//
                         ,
                         &TaskAcceptDateTest::name);

INSTANTIATE_TEST_SUITE_P(in6Days_daysPerMonth, TaskAcceptDateTest,
                         testing::Combine(//
                             ::testing::Values(model::PrescriptionType::tRezept),
                             ::testing::ValuesIn<std::list<TaskAcceptDateTestParam>>({
                                 {"28days_to_last_midnight", "2026-02-22T00:00", "2026-02-28"},
                                 {"28days_to_last_early", "2026-02-22T00:30", "2026-02-28"},
                                 {"28days_to_last_late", "2026-02-22T23:30", "2026-02-28"},

                                 {"28days_over_last_midnight", "2026-02-23T00:00", "2026-03-01"},
                                 {"28days_over_last_early", "2026-02-23T00:30", "2026-03-01"},
                                 {"28days_over_last_late", "2026-02-23T23:30", "2026-03-01"},

                                 {"29days_to_last_midnight", "2028-02-23T00:00", "2028-02-29"},
                                 {"29days_to_last_early", "2028-02-23T00:30", "2028-02-29"},
                                 {"29days_to_last_late", "2028-02-23T23:30", "2028-02-29"},

                                 {"29days_over_last_midnight", "2028-02-24T00:00", "2028-03-01"},
                                 {"29days_over_last_early", "2028-02-24T00:30", "2028-03-01"},
                                 {"29days_over_last_late", "2028-02-24T23:30", "2028-03-01"},

                                 {"30days_to_last_midnight", "2026-04-24T00:00", "2026-04-30"},
                                 {"30days_to_last_early", "2026-04-24T00:30", "2026-04-30"},
                                 {"30days_to_last_late", "2026-04-24T23:30", "2026-04-30"},

                                 {"30days_over_last_midnight", "2026-04-25T00:00", "2026-05-01"},
                                 {"30days_over_last_early", "2026-04-25T00:30", "2026-05-01"},
                                 {"30days_over_last_late", "2026-04-25T23:30", "2026-05-01"},

                                 {"31days_to_last_midnight", "2026-05-25T00:00", "2026-05-31"},
                                 {"31days_to_last_early", "2026-05-25T00:30", "2026-05-31"},
                                 {"31days_to_last_late", "2026-05-25T23:30", "2026-05-31"},

                                 {"31days_over_last_midnight", "2026-05-26T00:00", "2026-06-01"},
                                 {"31days_over_last_early", "2026-05-26T00:30", "2026-06-01"},
                                 {"31days_over_last_late", "2026-05-26T23:30", "2026-06-01"},
                             }))//
                         ,
                         &TaskAcceptDateTest::name);


INSTANTIATE_TEST_SUITE_P(in28Days, TaskAcceptDateTest,
                         testing::Combine(//
                             ::testing::Values(model::PrescriptionType::apothekenpflichigeArzneimittel,
                                               model::PrescriptionType::direkteZuweisung),
                             ::testing::ValuesIn<std::list<TaskAcceptDateTestParam>>({
                                 {"yearChange_midnight", "2025-12-15T00:00", "2026-01-12"},
                                 {"yearChange_early", "2025-12-15T00:30", "2026-01-12"},
                                 {"yearChange_late", "2025-12-15T23:30", "2026-01-12"},

                                 {"winter_midnight", "2026-01-01T00:00", "2026-01-29"},
                                 {"winter_early", "2026-01-01T00:30", "2026-01-29"},
                                 {"winter_late", "2026-01-01T23:30", "2026-01-29"},

                                 {"winter_summer_midnight", "2026-03-02T00:00", "2026-03-30"},
                                 {"winter_summer_early", "2026-03-02T00:30", "2026-03-30"},
                                 {"winter_summer_late", "2026-03-02T23:30", "2026-03-30"},

                                 {"summer_midnight", "2026-06-01T00:00", "2026-06-29"},
                                 {"summer_early", "2026-06-01T00:30", "2026-06-29"},
                                 {"summer_late", "2026-06-01T23:30", "2026-06-29"},

                                 {"summer_winter_midnight", "2026-10-01T00:00", "2026-10-29"},
                                 {"summer_winter_early", "2026-10-01T00:30", "2026-10-29"},
                                 {"summer_winter_late", "2026-10-01T23:30", "2026-10-29"},
                             }))//
                         ,
                         &TaskAcceptDateTest::name);

INSTANTIATE_TEST_SUITE_P(in28Days_DaylightSaving, TaskAcceptDateTest,
                         testing::Combine(//
                             ::testing::Values(model::PrescriptionType::apothekenpflichigeArzneimittel,
                                               model::PrescriptionType::direkteZuweisung),
                             ::testing::ValuesIn<std::list<TaskAcceptDateTestParam>>({
                                 {"toAtChange_winter_summer", "2026-03-01T02:30", "2026-03-29"},
                                 {"toBeforeChange_winter_summer", "2026-03-01T02:00", "2026-03-29"},
                                 {"toAfterChange_winter_summer", "2026-03-01T03:00", "2026-03-29"},

                                 {"fromBeforeChange_winter_summer", "2026-03-29T02:00:00+01:00", "2026-04-26"},
                                 {"fromAfterChange_winter_summer", "2026-03-29T03:00", "2026-04-26"},

                                 {"toAtChange_summer_winter", "2026-09-27T02:30", "2026-10-25"},
                                 {"toBeforeChange_summer_winter", "2026-09-27T02:00", "2026-10-25"},
                                 {"toAfterChange_summer_winter", "2026-09-27T03:00", "2026-10-25"},

                                 {"fromChange_summer_winter_before", "2026-10-25T02:30:00+02:00", "2026-11-22"},
                                 {"fromChange_summer_winter_after", "2026-10-25T02:30:00+01:00", "2026-11-22"},
                                 {"fromBeforeChange_summer_winter", "2026-10-25T02:00:00+02:00", "2026-11-22"},
                                 {"fromAfterChange_summer_winter", "2026-10-25T03:00:00+01:00", "2026-11-22"},
                             }))//
                         ,
                         &TaskAcceptDateTest::name);

INSTANTIATE_TEST_SUITE_P(in28Days_LeapYear, TaskAcceptDateTest,
                         testing::Combine(//
                             ::testing::Values(model::PrescriptionType::apothekenpflichigeArzneimittel,
                                               model::PrescriptionType::direkteZuweisung),
                             ::testing::ValuesIn<std::list<TaskAcceptDateTestParam>>({
                                 {"onto_LeapDay_midnight", "2028-02-01T00:00", "2028-02-29"},
                                 {"onto_LeapDay_early", "2028-02-01T00:30", "2028-02-29"},
                                 {"onto_LeapDay_late", "2028-02-01T23:30", "2028-02-29"},

                                 {"over_LeapDay_midnight", "2028-02-02T00:00", "2028-03-01"},
                                 {"over_LeapDay_early", "2028-02-02T00:30", "2028-03-01"},
                                 {"over_LeapDay_late", "2028-02-02T23:30", "2028-03-01"},
                                 // in 2036 daylight saving is on 31.March.
                                 // This avoids to have both leap year and daylight saving in the test-period
                                 {"from_LeapDay_midnight", "2036-02-29T00:00", "2036-03-28"},
                                 {"from_LeapDay_early", "2036-02-29T00:30", "2036-03-28"},
                                 {"from_LeapDay_late", "2036-02-29T23:30", "2036-03-28"},

                                 {"noLeapDay_midnight", "2026-02-01T00:00", "2026-03-01"},
                                 {"noLeapDay_early", "2026-02-01T00:30", "2026-03-01"},
                                 {"noLeapDay_late", "2026-02-01T23:30", "2026-03-01"},
                             }))//
                         ,
                         &TaskAcceptDateTest::name);

INSTANTIATE_TEST_SUITE_P(in28Days_daysPerMonth, TaskAcceptDateTest,
                         testing::Combine(//
                             ::testing::Values(model::PrescriptionType::apothekenpflichigeArzneimittel,
                                               model::PrescriptionType::direkteZuweisung),
                             ::testing::ValuesIn<std::list<TaskAcceptDateTestParam>>({
                                 {"28days_to_last_midnight", "2026-01-31T00:00", "2026-02-28"},
                                 {"28days_to_last_early", "2026-01-31T00:30", "2026-02-28"},
                                 {"28days_to_last_late", "2026-01-31T23:30", "2026-02-28"},

                                 {"28days_over_last_midnight", "2026-02-01T00:00", "2026-03-01"},
                                 {"28days_over_last_early", "2026-02-01T00:30", "2026-03-01"},
                                 {"28days_over_last_late", "2026-02-01T23:30", "2026-03-01"},

                                 {"29days_to_last_midnight", "2028-02-01T00:00", "2028-02-29"},
                                 {"29days_to_last_early", "2028-02-01T00:30", "2028-02-29"},
                                 {"29days_to_last_late", "2028-02-01T23:30", "2028-02-29"},

                                 {"29days_over_last_midnight", "2028-02-02T00:00", "2028-03-01"},
                                 {"29days_over_last_early", "2028-02-02T00:30", "2028-03-01"},
                                 {"29days_over_last_late", "2028-02-02T23:30", "2028-03-01"},

                                 {"30days_to_last_midnight", "2026-04-02T00:00", "2026-04-30"},
                                 {"30days_to_last_early", "2026-04-02T00:30", "2026-04-30"},
                                 {"30days_to_last_late", "2026-04-02T23:30", "2026-04-30"},

                                 {"30days_over_last_midnight", "2026-04-03T00:00", "2026-05-01"},
                                 {"30days_over_last_early", "2026-04-03T00:30", "2026-05-01"},
                                 {"30days_over_last_late", "2026-04-03T23:30", "2026-05-01"},

                                 {"31days_to_last_midnight", "2026-05-03T00:00", "2026-05-31"},
                                 {"31days_to_last_early", "2026-05-03T00:30", "2026-05-31"},
                                 {"31days_to_last_late", "2026-05-03T23:30", "2026-05-31"},

                                 {"31days_over_last_midnight", "2026-05-04T00:00", "2026-06-01"},
                                 {"31days_over_last_early", "2026-05-04T00:30", "2026-06-01"},
                                 {"31days_over_last_late", "2026-05-04T23:30", "2026-06-01"},
                             }))//
                         ,
                         &TaskAcceptDateTest::name);


INSTANTIATE_TEST_SUITE_P(in3Months, TaskAcceptDateTest,
                         testing::Combine(//
                             ::testing::Values(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
                                               model::PrescriptionType::direkteZuweisungPkv,
                                               model::PrescriptionType::digitaleGesundheitsanwendungen),
                             ::testing::ValuesIn<std::list<TaskAcceptDateTestParam>>({
                                 {"winter_midnight", "2025-11-15T00:00:00", "2026-02-15"},
                                 {"winter_early", "2025-11-15T00:30:00", "2026-02-15"},
                                 {"winter_late", "2025-11-15T23:30:00", "2026-02-15"},

                                 {"winter_summer_midnight", "2026-03-01T00:00:00", "2026-06-01"},
                                 {"winter_summer_early", "2026-03-01T00:30:00", "2026-06-01"},
                                 {"winter_summer_late", "2026-03-01T23:30:00", "2026-06-01"},

                                 {"yearChange_winter_summer_midnight", "2022-12-28T00:00:00", "2023-03-28"},
                                 {"yearChange_winter_summer_early", "2022-12-28T00:30:00", "2023-03-28"},
                                 {"yearChange_winter_summer_late", "2022-12-28T23:30:00", "2023-03-28"},

                                 {"summer_midnight", "2026-06-01T00:00:00", "2026-09-01"},
                                 {"summer_early", "2026-06-01T00:30:00", "2026-09-01"},
                                 {"summer_late", "2026-06-01T23:30:00", "2026-09-01"},

                                 {"summer_winter_midnight", "2026-09-01T00:00:00", "2026-12-01"},
                                 {"summer_winter_early", "2026-09-01T00:30:00", "2026-12-01"},
                                 {"summer_winter_late", "2026-09-01T23:30:00", "2026-12-01"},

                                 {"yearChange_summer_winter_midnight", "2026-10-01T00:00:00", "2027-01-01"},
                                 {"yearChange_summer_winter_early", "2026-10-01T00:30:00", "2027-01-01"},
                                 {"yearChange_summer_winter_late", "2026-10-01T23:30:00", "2027-01-01"},
                             }))//
                         ,
                         &TaskAcceptDateTest::name);

INSTANTIATE_TEST_SUITE_P(in3Months_DaylightSaving, TaskAcceptDateTest,
                         testing::Combine(//
                             ::testing::Values(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
                                               model::PrescriptionType::direkteZuweisungPkv,
                                               model::PrescriptionType::digitaleGesundheitsanwendungen),
                             ::testing::ValuesIn<std::list<TaskAcceptDateTestParam>>({
                                 {"toAtChange_winter_summer", "2025-12-29T02:30", "2026-03-29"},
                                 {"toBeforeChange_winter_summer", "2025-12-29T02:00", "2026-03-29"},
                                 {"toAfterChange_winter_summer", "2025-12-29T03:00", "2026-03-29"},

                                 {"fromBeforeChange_winter_summer", "2026-03-29T02:00:00+01:00", "2026-06-29"},
                                 {"fromAfterChange_winter_summer", "2026-03-29T03:00:00+02:00", "2026-06-29"},

                                 {"toBeforeChange_summer_winter", "2026-07-25T02:00:00+02:00", "2026-10-25"},
                                 {"toBeforeChange_summer_winter_middel", "2026-07-25T02:30:00+02:00", "2026-10-25"},
                                 {"toAfterChange_summer_winter_middel", "2026-07-25T02:30:00+01:00", "2026-10-25"},
                                 {"toAfterChange_summer_winter", "2026-07-25T03:00:00+01:00", "2026-10-25"},

                                 {"fromChange_summer_winter_before", "2026-10-25T02:30:00+02:00", "2027-01-25"},
                                 {"fromChange_summer_winter_after", "2026-10-25T02:30:00+01:00", "2027-01-25"},
                                 {"fromBeforeChange_summer_winter", "2026-10-25T02:00:00+02:00", "2027-01-25"},
                                 {"fromAfterChange_summer_winter", "2026-10-25T03:00:00+01:00", "2027-01-25"},
                             }))//
                         ,
                         &TaskAcceptDateTest::name);

INSTANTIATE_TEST_SUITE_P(in3Months_LeapYear, TaskAcceptDateTest,
                         testing::Combine(//
                             ::testing::Values(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
                                               model::PrescriptionType::direkteZuweisungPkv,
                                               model::PrescriptionType::digitaleGesundheitsanwendungen),
                             ::testing::ValuesIn<std::list<TaskAcceptDateTestParam>>({
                                 {"onto_LeapDay_midnight", "2027-11-29T00:00", "2028-02-29"},
                                 {"onto_LeapDay_early", "2027-11-29T00:30", "2028-02-29"},
                                 {"onto_LeapDay_late", "2027-11-29T23:30", "2028-02-29"},

                                 {"from_LeapDay_midnight", "2028-02-29T00:00", "2028-05-29"},
                                 {"from_LeapDay_early", "2028-02-29T00:30", "2028-05-29"},
                                 {"from_LeapDay_late", "2028-02-29T23:30", "2028-05-29"},

                                 {"noLeapDay_midnight", "2026-11-29T00:00", "2027-02-28"},
                                 {"noLeapDay_early", "2026-11-29T00:30", "2027-02-28"},
                                 {"noLeapDay_late", "2026-11-29T23:30", "2027-02-28"},
                             }))//
                         ,
                         &TaskAcceptDateTest::name);


INSTANTIATE_TEST_SUITE_P(in3Months_daysPerMonth, TaskAcceptDateTest,
                         testing::Combine(//
                             ::testing::Values(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
                                               model::PrescriptionType::direkteZuweisungPkv,
                                               model::PrescriptionType::digitaleGesundheitsanwendungen),
                             ::testing::ValuesIn<std::list<TaskAcceptDateTestParam>>({
                                 {"28days_shortened_midnight", "2026-11-30T00:00", "2027-02-28"},
                                 {"28days_shortened_early", "2026-11-30T00:30", "2027-02-28"},
                                 {"28days_shortened_late", "2026-11-30T23:30", "2027-02-28"},

                                 {"28days_exact_midnight", "2026-11-28T00:00", "2027-02-28"},
                                 {"28days_exact_early", "2026-11-28T00:30", "2027-02-28"},
                                 {"28days_exact_late", "2026-11-28T23:30", "2027-02-28"},

                                 {"29days_shortened_midnight", "2027-11-30T00:00", "2028-02-29"},
                                 {"29days_shortened_early", "2027-11-30T00:30", "2028-02-29"},
                                 {"29days_shortened_late", "2027-11-30T23:30", "2028-02-29"},

                                 {"29days_exact_midnight", "2027-11-29T00:00", "2028-02-29"},
                                 {"29days_exact_early", "2027-11-29T00:30", "2028-02-29"},
                                 {"29days_exact_late", "2027-11-29T23:30", "2028-02-29"},

                                 {"30days_shortened_midnight", "2026-01-31T00:00", "2026-04-30"},
                                 {"30days_shortened_early", "2026-01-31T00:30", "2026-04-30"},
                                 {"30days_shortened_late", "2026-01-31T23:30", "2026-04-30"},

                                 {"30days_exact_midnight", "2026-01-30T00:00", "2026-04-30"},
                                 {"30days_exact_early", "2026-01-30T00:30", "2026-04-30"},
                                 {"30days_exact_late", "2026-01-30T23:30", "2026-04-30"},

                                 {"31days_midnight", "2026-05-31T00:00", "2026-08-31"},
                                 {"31days_early", "2026-05-31T00:30", "2026-08-31"},
                                 {"31days_late", "2026-05-31T23:30", "2026-08-31"},

                             }))//
                         ,
                         &TaskAcceptDateTest::name);

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
