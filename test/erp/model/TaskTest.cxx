#include "erp/model/Task.hxx"

#include <date/date.h>
#include <erp/model/Composition.hxx>
#include <gtest/gtest.h>
#include <rapidjson/pointer.h>
#include <thread>// for std::this_thread::sleep_for

#include "erp/model/ModelException.hxx"
#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Uuid.hxx"
#include "test_config.h"

class TaskTest : public testing::Test
{
public:

    // definition of JSON pointers:
    static constexpr std::string_view p_authoredOn           = "/authoredOn";
    static constexpr std::string_view p_lastModified         = "/lastModified";
    static constexpr std::string_view p_status               = "/status";
    static constexpr std::string_view p_taskId               = "/id";
    static constexpr std::string_view p_prescriptionId       = "/identifier/0/value";
    static constexpr std::string_view p_accessCode           = "/identifier/1/value";
    static constexpr std::string_view p_secret               = "/identifier/2/value";
    static constexpr std::string_view p_flowtypeCode         = "/extension/0/valueCoding/code";
    static constexpr std::string_view p_flowtypeDisplay      = "/extension/0/valueCoding/display";
    static constexpr std::string_view p_performerType        = "/performerType/0/coding/0/code";
    static constexpr std::string_view p_performerTypeDisplay = "/performerType/0/coding/0/display";
    static constexpr std::string_view p_performerTypeText    = "/performerType/0/text";
    static constexpr std::string_view p_kvnr                 = "/for/identifier/value";
    static constexpr std::string_view p_kvnrSystem           = "/for/identifier/system";
    static constexpr std::string_view p_inputArray           = "/input";
    static constexpr std::string_view p_outputArray          = "/output";
};

TEST_F(TaskTest, ConstructNewTask)
{
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_status.data())), "draft");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_taskId.data())), "");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_prescriptionId.data())), "");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_accessCode.data())), "access_code");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeCode.data())), "160");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_flowtypeDisplay.data())), "Muster 16 (Apothekenpflichtige Arzneimittel)");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_performerType.data())),"urn:oid:1.2.276.0.76.4.54");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_performerTypeDisplay.data())), "Öffentliche Apotheke");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_performerTypeText.data())), "Öffentliche Apotheke");
    ASSERT_FALSE(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_lastModified.data())).empty());
    ASSERT_FALSE(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_authoredOn.data())).empty());
}

TEST_F(TaskTest, SetId)
{
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    const auto lastModified = task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_lastModified.data()));
    task.setPrescriptionId(model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1));
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_taskId.data())), "160.000.000.000.001.54");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_prescriptionId.data())), "160.000.000.000.001.54");
    task.setPrescriptionId(model::PrescriptionId::fromString("160.100.000.000.000.42"));
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_taskId.data())), "160.100.000.000.000.42");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_prescriptionId.data())), "160.100.000.000.000.42");
    // setting the PrescriptionId does not change the lastModification time or the version.
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_lastModified.data())), lastModified);
}

TEST_F(TaskTest, UpdateLastUpdated)
{
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    const auto lastModified = task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_lastModified.data()));

    // sleep to enforce that the timestamps change. The resolution is limited to 1sec.
    std::this_thread::sleep_for(std::chrono::seconds(1));

    task.updateLastUpdate();
    ASSERT_NE(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_lastModified.data())), lastModified);
}

TEST_F(TaskTest, SetStatus)
{
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_status.data())), "draft");
    ASSERT_EQ(task.status(), model::Task::Status::draft);

    task.setStatus(model::Task::Status::ready);
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_status.data())), "ready");

    task.setStatus(model::Task::Status::cancelled);
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_status.data())), "cancelled");
    ASSERT_EQ(task.status(), model::Task::Status::cancelled);

    task.setStatus(model::Task::Status::completed);
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_status.data())), "completed");
    ASSERT_EQ(task.status(), model::Task::Status::completed);

    task.setStatus(model::Task::Status::inprogress);
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_status.data())), "in-progress");
    ASSERT_EQ(task.status(), model::Task::Status::inprogress);

    task.setStatus(model::Task::Status::draft);
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_status.data())), "draft");
    ASSERT_EQ(task.status(), model::Task::Status::draft);
}

TEST_F(TaskTest, SetAndDeleteKvnr)
{
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    ASSERT_EQ(rapidjson::Pointer(p_kvnr.data()).Get(task.jsonDocument()), nullptr);
    ASSERT_FALSE(task.kvnr().has_value());

    task.setKvnr("X123456789");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_kvnr.data())), "X123456789");
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_kvnrSystem.data())), "http://fhir.de/NamingSystem/gkv/kvid-10");
    ASSERT_EQ(task.kvnr(), "X123456789");

    task.deleteKvnr();
    ASSERT_EQ(rapidjson::Pointer(p_kvnr.data()).Get(task.jsonDocument()), nullptr);
    ASSERT_FALSE(task.kvnr().has_value());
}

TEST_F(TaskTest, FromJson)
{
    const auto task1 = FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/task1.json");
    ASSERT_NO_THROW(model::Task::fromJson(task1));
    const auto task = model::Task::fromJson(task1);
    ASSERT_EQ(task.status(), model::Task::Status::ready);
    const auto prescriptionId = task.prescriptionId();
    ASSERT_EQ(prescriptionId.toString(), "160.000.000.004.711.86");
    ASSERT_EQ(task.type(), model::PrescriptionType::apothekenpflichigeArzneimittel);
}

TEST_F(TaskTest, SetAndDeleteUuids)
{
    auto id = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4711);
    model::Task task(id, model::PrescriptionType::apothekenpflichigeArzneimittel,
                     model::Timestamp::now(), model::Timestamp::now(),
                     model::Task::Status::completed);
    const auto uuid1 = id.deriveUuid(1);
    const auto uuid2 = id.deriveUuid(2);
    const auto uuid3 = id.deriveUuid(3);

    ASSERT_EQ(task.healthCarePrescriptionUuid().value_or(""), uuid1);
    ASSERT_EQ(task.patientConfirmationUuid().value_or(""), uuid2);
    ASSERT_EQ(task.receiptUuid().value_or(""), uuid3);

    ASSERT_EQ(rapidjson::Pointer(p_inputArray.data()).Get(task.jsonDocument())->GetArray().Size(), static_cast<rapidjson::SizeType>(2));
    task.deleteInput();
    ASSERT_EQ(rapidjson::Pointer(p_inputArray.data()).Get(task.jsonDocument()), nullptr);
    ASSERT_TRUE(!task.patientConfirmationUuid().has_value());
    ASSERT_TRUE(!task.healthCarePrescriptionUuid().has_value());
    ASSERT_EQ(rapidjson::Pointer(p_outputArray.data()).Get(task.jsonDocument())->GetArray().Size(), static_cast<rapidjson::SizeType>(1));
    task.deleteOutput();
    ASSERT_EQ(rapidjson::Pointer(p_outputArray.data()).Get(task.jsonDocument()), nullptr);
    ASSERT_TRUE(!task.receiptUuid().has_value());

    // Set and check again:
    task.setHealthCarePrescriptionUuid();
    task.setPatientConfirmationUuid();
    task.setReceiptUuid();
    ASSERT_EQ(task.healthCarePrescriptionUuid().value_or(""), uuid1);
    ASSERT_EQ(task.patientConfirmationUuid().value_or(""), uuid2);
    ASSERT_EQ(task.receiptUuid().value_or(""), uuid3);
    ASSERT_EQ(rapidjson::Pointer(p_inputArray.data()).Get(task.jsonDocument())->GetArray().Size(), static_cast<rapidjson::SizeType>(2));
    ASSERT_EQ(rapidjson::Pointer(p_outputArray.data()).Get(task.jsonDocument())->GetArray().Size(), static_cast<rapidjson::SizeType>(1));
}

TEST_F(TaskTest, ExpiryDate)
{
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    model::Timestamp timestamp = model::Timestamp::now();
    ASSERT_NO_THROW(task.setExpiryDate(timestamp));
    ASSERT_EQ(task.expiryDate(), model::Timestamp::fromXsDate(timestamp.toXsDate()));
}

TEST_F(TaskTest, AcceptDate)
{
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    model::Timestamp timestamp = model::Timestamp::now();
    ASSERT_NO_THROW(task.setAcceptDate(timestamp));
    ASSERT_EQ(task.acceptDate(), model::Timestamp::fromXsDate(timestamp.toXsDate()));
}


TEST_F(TaskTest, AcceptDate30Days)
{
    using namespace date;
    using namespace date::literals;
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    auto baseDate = 2021_y/April/23;
    task.setAcceptDate(model::Timestamp(sys_days{baseDate}), model::KbvStatusKennzeichen::asvKennzeichen, 3);
    auto expected = sys_days{baseDate} + days{30};
    ASSERT_EQ(task.acceptDate().toChronoTimePoint(), expected);
}


TEST_F(TaskTest, AcceptDate3WorkDays)
{
    using namespace date;
    using namespace date::literals;
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    auto baseDate = 2021_y/April/23;
    task.setAcceptDate(model::Timestamp(sys_days{baseDate}), model::KbvStatusKennzeichen::entlassmanagementKennzeichen,
                       3);
    auto expected = sys_days{2021_y/April/26};
    ASSERT_EQ(task.acceptDate().toChronoTimePoint(), expected);
}

TEST_F(TaskTest, AcceptDate6WorkDaysOverHolidays)
{
    using namespace date;
    using namespace date::literals;
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    auto baseDate = 2021_y/December/23;
    task.setAcceptDate(model::Timestamp(sys_days{baseDate}), model::KbvStatusKennzeichen::entlassmanagementKennzeichen,
        6);
    auto expected = sys_days{2021_y/December/30};
    ASSERT_EQ(task.acceptDate().toChronoTimePoint(), expected);
}


TEST_F(TaskTest, DeleteAccessCode)
{
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");
    ASSERT_EQ(task.accessCode(), "access_code");
    task.deleteAccessCode();
    ASSERT_EQ(rapidjson::Pointer(p_accessCode.data()).Get(task.jsonDocument()), nullptr);
    std::string_view accessCode;
    ASSERT_THROW(accessCode = task.accessCode(), model::ModelException);
}

TEST_F(TaskTest, SetAndDeleteSecret)
{
    model::Task task(model::PrescriptionType::apothekenpflichigeArzneimittel, "access_code");

    ASSERT_EQ(rapidjson::Pointer(p_secret.data()).Get(task.jsonDocument()), nullptr);
    ASSERT_FALSE(task.secret().has_value());

    const std::string_view secret = "Secret";
    task.setSecret(secret);
    ASSERT_EQ(task.jsonDocument().getStringValueFromPointer(rapidjson::Pointer(p_secret.data())), secret);
    ASSERT_EQ(task.secret(), secret);

    task.deleteSecret();
    ASSERT_EQ(rapidjson::Pointer(p_secret.data()).Get(task.jsonDocument()), nullptr);
    ASSERT_FALSE(task.secret().has_value());
}
