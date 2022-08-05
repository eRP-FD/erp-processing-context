/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/erp/database/PostgresDatabaseTest.hxx"
#include "test_config.h"
#include "erp/model/Binary.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/model/Task.hxx"
#include "erp/model/AuditData.hxx"
#include "erp/ErpRequirements.hxx"

#include "erp/util/FileHelper.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "erp/util/Uuid.hxx"
#include "test/util/ErpMacros.hxx"


class PostgresDatabaseTaskTest : public PostgresDatabaseTest, public testing::WithParamInterface<model::PrescriptionType>
{
public:
    PostgresDatabaseTaskTest()
    {
        if (usePostgres())
        {
            auto deleteTxn = createTransaction();
            deleteTxn.exec("DELETE FROM " + taskTableName());
            deleteTxn.commit();
        }
    }

    void cleanup() override
    {
        if (usePostgres())
        {
            clearTables();
        }
    }

    std::string taskTableName() const
    {
        switch(GetParam())
        {
            case model::PrescriptionType::apothekenpflichigeArzneimittel:
                return "erp.task";
            case model::PrescriptionType::direkteZuweisung:
                return "erp.task_169";
            case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
            case model::PrescriptionType::direkteZuweisungPkv:
                Fail("Not yet implemented"); // TODO implement
        }
        Fail("invalid prescription type: " + std::to_string(uintmax_t(GetParam())));
    }

    model::PrescriptionType prescriptionType() const
    {
        return GetParam();
    }

    void retrieveTaskData(pqxx::result& result, const model::PrescriptionId& prescriptionId)
    {
        const Query retrieveTask{
            "retrieveTask",
            "SELECT * "
            "FROM " + taskTableName() +
            " WHERE prescription_id = $1"};
        prepare(retrieveTask);

        auto &&txn = createTransaction();
        ASSERT_NO_THROW(result = txn.exec_prepared(retrieveTask.name, prescriptionId.toDatabaseId()));
        txn.commit();
        ASSERT_FALSE(result.empty());
    }

    UrlArguments searchForStatus(const std::string& status)
    {
        auto search = UrlArguments({{"status", SearchParameter::Type::TaskStatus}});
        auto request = ServerRequest(Header());
        request.setQueryParameters({{"status", status}});
        search.parse(request, getKeyDerivation());
        return search;
    }

    UrlArguments searchForLastModified(const fhirtools::Timestamp& timestamp, const std::string& operation)
    {
        auto search = UrlArguments({{"modified", "last_modified", SearchParameter::Type::Date}});
        auto request = ServerRequest(Header());
        request.setQueryParameters({{"modified", operation + timestamp.toXsDateTimeWithoutFractionalSeconds()}});
        search.parse(request, getKeyDerivation());
        return search;
    }

    UrlArguments searchForAuthoredOn(const fhirtools::Timestamp& timestamp, const std::string& operation)
    {
        auto search = UrlArguments({{"authored-on", "authored_on", SearchParameter::Type::Date}});
        auto request = ServerRequest(Header());
        request.setQueryParameters({{"authored-on", operation + timestamp.toXsDateTimeWithoutFractionalSeconds()}});
        search.parse(request, getKeyDerivation());
        return search;
    }

    UrlArguments sortByAuthoredOn(bool reverse)
    {
        auto search = UrlArguments({{"authored-on", "authored_on", SearchParameter::Type::Date}});
        auto request = ServerRequest(Header());
        request.setQueryParameters({{"_sort", reverse?"-authored-on":"authored-on"}});
        search.parse(request, getKeyDerivation());
        return search;
    }

    UrlArguments paging(int perPage, int offset)
    {
        auto search = UrlArguments({});
        auto request = ServerRequest(Header());
        request.setQueryParameters({{"_count", std::to_string(perPage)}, {"__offset", std::to_string(offset)}});
        search.parse(request, getKeyDerivation());
        return search;
    }

    void setupTasks(size_t nTasks, std::string_view kvnr, std::vector<model::Task>& outTasks,
                    model::PrescriptionType prescriptionType = GetParam())
    {
        for (size_t i = 0; i < nTasks; ++i)
        {
            model::Task task1(prescriptionType, "access_code");
            const auto id1 = database().storeTask(task1);
            // The database generates the ID
            task1.setPrescriptionId(id1);

            // assign KVNRs
            task1.setKvnr(kvnr);
            task1.setExpiryDate(fhirtools::Timestamp::now());
            task1.setAcceptDate(fhirtools::Timestamp::now());
            task1.setStatus(model::Task::Status::ready);
            database().activateTask(
                task1, model::Binary(Uuid().toString(),
                                     model::Bundle(model::BundleType::document, ::model::ResourceBase::NoProfile)
                                         .serializeToJsonString()));
            database().commitTransaction();

            outTasks.emplace_back(std::move(task1));
        }
    }
};


TEST_P(PostgresDatabaseTaskTest, InsertTask)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    model::Task task1(prescriptionType(), "access_code");

    const auto id1 = database().storeTask(task1);
    database().commitTransaction();
    ASSERT_GT(id1.toDatabaseId(), 0);

    const auto task1FromDb = std::get<0>(database().retrieveTaskAndPrescription(id1));
    ASSERT_TRUE(task1FromDb.has_value());


    model::Task task2(prescriptionType(), "access_code");
    const auto id2 = database().storeTask(task2);
    database().commitTransaction();
    ASSERT_GT(id2.toDatabaseId(), id1.toDatabaseId());

    const auto task2FromDb = std::get<0>(database().retrieveTaskAndPrescription(id2));
    database().commitTransaction();
    ASSERT_TRUE(task2FromDb.has_value());

    pqxx::result result;
    ASSERT_NO_FATAL_FAILURE(retrieveTaskData(result, id2));
    pqxx::row::size_type row = 0;
    ASSERT_EQ(result.front().at(row++).as<int64_t>(), id2.toDatabaseId()); // prescription_id
    ASSERT_TRUE(result.front().at(row++).is_null()); // kvnr
    ASSERT_TRUE(result.front().at(row++).is_null()); // kvnr_hashed
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // last_modified
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // authored_on
    ASSERT_TRUE(result.front().at(row++).is_null()); // expiry_date
    ASSERT_TRUE(result.front().at(row++).is_null()); // accept_date
    ASSERT_EQ(result.front().at(row++).as<int>(), 0); // status
    ASSERT_FALSE(result.front().at(row++).is_null()); // key_generation_id
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // salt
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // access_code
    ASSERT_TRUE(result.front().at(row++).is_null()); // secret
    ASSERT_TRUE(result.front().at(row++).is_null()); // healthcare_provider_prescription
    ASSERT_TRUE(result.front().at(row++).is_null()); // receipt
    ASSERT_TRUE(result.front().at(row++).is_null()); // when_handed_over
    ASSERT_TRUE(result.front().at(row++).is_null()); // when_prepared
    ASSERT_TRUE(result.front().at(row++).is_null()); // performer
    ASSERT_TRUE(result.front().at(row++).is_null()); // medication_dispense_bundle
}

TEST_P(PostgresDatabaseTaskTest, updateTaskActivate)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    model::Task task1(prescriptionType(), "access_code");
    task1.setKvnr(InsurantA);

    // does not store the KVNR:
    task1.setPrescriptionId(database().storeTask(task1));
    database().commitTransaction();
    ASSERT_TRUE(database().retrieveAllTasksForPatient(InsurantA, {}).empty());

    task1.setStatus(model::Task::Status::ready);
    task1.setExpiryDate(fhirtools::Timestamp::now());
    task1.setAcceptDate(fhirtools::Timestamp::now());
    database().activateTask(
        task1,
        model::Binary(
            Uuid().toString(),
            model::Bundle(model::BundleType::document, ::model::ResourceBase::NoProfile).serializeToJsonString()));
    database().commitTransaction();

    // KVNR is now set
    ASSERT_FALSE(database().retrieveAllTasksForPatient(InsurantA, {}).empty());
    database().commitTransaction();

    pqxx::result result;
    pqxx::row::size_type row = 0;
    ASSERT_NO_FATAL_FAILURE(retrieveTaskData(result, task1.prescriptionId()));
    ASSERT_EQ(result.front().at(row++).as<int64_t>(), task1.prescriptionId().toDatabaseId()); // prescription_id
    ASSERT_FALSE(result.front().at(row++).is_null()); // kvnr
    ASSERT_FALSE(result.front().at(row++).is_null()); // kvnr_hashed
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // last_modified
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // authored_on
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // expiry_date
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // accept_date
    ASSERT_EQ(result.front().at(row++).as<int>(), 1); // status
    ASSERT_FALSE(result.front().at(row++).is_null()); // key_generation_id
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // salt
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // access_code
    ASSERT_TRUE(result.front().at(row++).is_null()); // secret
    ASSERT_TRUE(!result.front().at(row++).as<std::string>().empty()); // healthcare_provider_prescription
    ASSERT_TRUE(result.front().at(row++).is_null()); // receipt
    ASSERT_TRUE(result.front().at(row++).is_null()); // when_handed_over
    ASSERT_TRUE(result.front().at(row++).is_null()); // when_prepared
    ASSERT_TRUE(result.front().at(row++).is_null()); // performer
    ASSERT_TRUE(result.front().at(row++).is_null()); // medication_dispense_bundle
}

TEST_P(PostgresDatabaseTaskTest, updateTaskStatusAndSecret)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    model::Task task1(prescriptionType(), "access_code");
    task1.setKvnr(InsurantA);

    // does not store the KVNR:
    task1.setPrescriptionId(database().storeTask(task1));
    database().commitTransaction();

    {
        const auto taskFromDb = std::get<0>(database().retrieveTaskAndPrescription(task1.prescriptionId()));
        ASSERT_EQ(taskFromDb->status(), model::Task::Status::draft);
        const auto task2FromDb = database().retrieveTaskForUpdate(task1.prescriptionId());
        ASSERT_EQ(task2FromDb->status(), model::Task::Status::draft);
    }

    task1.setStatus(model::Task::Status::ready);
    task1.setSecret("secret");
    database().updateTaskStatusAndSecret(task1);
    database().commitTransaction();

    {
        const auto taskFromDb = std::get<0>(database().retrieveTaskAndReceipt(task1.prescriptionId()));
        ASSERT_EQ(taskFromDb->status(), model::Task::Status::ready);
        ASSERT_EQ(taskFromDb->secret(), "secret");
    }
    database().commitTransaction();
}

TEST_P(PostgresDatabaseTaskTest, retrieveHealthCareProviderPrescription)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    model::Task task(prescriptionType(), "access_code");
    const auto id = database().storeTask(task);
    database().commitTransaction();
    task.setPrescriptionId(id);
    task.setKvnr("X123456789");
    task.setAcceptDate(fhirtools::Timestamp::now());
    task.setExpiryDate(fhirtools::Timestamp::now());

    database().activateTask(task, model::Binary{Uuid().toString(), "HealthCareProviderPrescription"});
    database().commitTransaction();

    const auto [task1, healthCareProviderPrescription] = database().retrieveTaskAndPrescription(id);
    database().commitTransaction();
    ASSERT_TRUE(task1.has_value());
    ASSERT_TRUE(healthCareProviderPrescription.has_value());
    ASSERT_EQ(std::string(healthCareProviderPrescription.value().data().value()), "HealthCareProviderPrescription");
}

TEST_P(PostgresDatabaseTaskTest, retrieveAllTasksForPatient)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }


    A_19115.test("Ensure only own tasks can be retrieved by KVNR");

    const std::string kvnr1 = "XYZABC1234";
    const std::string kvnr2 = "XYZABC5678";

    // cleanup
    {
        auto&& txn = createTransaction();
        txn.exec_params("DELETE FROM " + taskTableName() + " WHERE kvnr_hashed=$1", getKeyDerivation().hashKvnr(kvnr1).binarystring());
        txn.exec_params("DELETE FROM " + taskTableName() + " WHERE kvnr_hashed=$1", getKeyDerivation().hashKvnr(kvnr2).binarystring());
        txn.commit();
    }

    // store three tasks, they contain no KVNR yet
    model::Task task1(prescriptionType(), "access_code");
    model::Task task2(prescriptionType(), "access_code");
    model::Task task3(prescriptionType(), "access_code");
    const auto id1 = database().storeTask(task1);
    const auto id2 = database().storeTask(task2);
    const auto id3 = database().storeTask(task3);
    database().commitTransaction();
    // The database generates the ID
    task1.setPrescriptionId(id1);
    task2.setPrescriptionId(id2);
    task3.setPrescriptionId(id3);

    // nothing can be retrieved, because KVNRs are not set yet
    const auto shouldBeEmptyResult1 = database().retrieveAllTasksForPatient(kvnr1, {});
    ASSERT_TRUE(shouldBeEmptyResult1.empty());
    const auto shouldBeEmptyResult2 = database().retrieveAllTasksForPatient(kvnr2, {});
    ASSERT_TRUE(shouldBeEmptyResult2.empty());

    // assign KVNRs
    task1.setKvnr(kvnr1);
    task1.setExpiryDate(fhirtools::Timestamp::now());
    task1.setAcceptDate(fhirtools::Timestamp::now());
    database().activateTask(
        task1,
        model::Binary(
            Uuid().toString(),
            model::Bundle(model::BundleType::document, ::model::ResourceBase::NoProfile).serializeToJsonString()));
    task2.setKvnr(kvnr2);
    task2.setExpiryDate(fhirtools::Timestamp::now());
    task2.setAcceptDate(fhirtools::Timestamp::now());
    database().activateTask(
        task2,
        model::Binary(
            Uuid().toString(),
            model::Bundle(model::BundleType::document, ::model::ResourceBase::NoProfile).serializeToJsonString()));
    task3.setKvnr(kvnr2);
    task3.setExpiryDate(fhirtools::Timestamp::now());
    task3.setAcceptDate(fhirtools::Timestamp::now());
    database().activateTask(
        task3,
        model::Binary(
            Uuid().toString(),
            model::Bundle(model::BundleType::document, ::model::ResourceBase::NoProfile).serializeToJsonString()));
    database().commitTransaction();


    // Now exactly one task can be retrieved for each KVNR
    const auto result1 =  database().retrieveAllTasksForPatient(kvnr1, {});
    ASSERT_EQ(result1.size(), 1);
    const auto& retrievedTask1 = result1[0];
    ASSERT_EQ(retrievedTask1.prescriptionId().toDatabaseId(), id1.toDatabaseId());
    ASSERT_EQ(retrievedTask1.kvnr().value(), kvnr1);

    const auto result2 =  database().retrieveAllTasksForPatient(kvnr2, {});
    database().commitTransaction();

    ASSERT_EQ(result2.size(), 2);
    const auto& retrievedTask2 = result2[0];
    ASSERT_EQ(retrievedTask2.kvnr().value(), kvnr2);
    const auto& retrievedTask3 = result2[1];
    ASSERT_EQ(retrievedTask3.kvnr().value(), kvnr2);
    ASSERT_TRUE((retrievedTask2.prescriptionId().toDatabaseId() == id2.toDatabaseId() &&
                 retrievedTask3.prescriptionId().toDatabaseId() == id3.toDatabaseId()) ||
                (retrievedTask2.prescriptionId().toDatabaseId() == id3.toDatabaseId() &&
                 retrievedTask3.prescriptionId().toDatabaseId() == id2.toDatabaseId()));


    // cleanup
    {
        auto&& txn = createTransaction();
        txn.exec_params("DELETE FROM " + taskTableName() + " WHERE kvnr_hashed=$1", getKeyDerivation().hashKvnr(kvnr1).binarystring());
        txn.exec_params("DELETE FROM " + taskTableName() + " WHERE kvnr_hashed=$1", getKeyDerivation().hashKvnr(kvnr2).binarystring());
        txn.commit();
    }
}


TEST_P(PostgresDatabaseTaskTest, updateTaskMedicationDispenseReceipt)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }
    model::Task task(prescriptionType(), "access_code");
    task.setPrescriptionId(database().storeTask(task));
    task.setKvnr("X123456789");

    const auto medicationDispenseJson =
        FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/medication_dispense_output1.json");
    model::MedicationDispense medicationDispense =
        model::MedicationDispense::fromJsonNoValidation(medicationDispenseJson);

    const auto receiptJson =
        FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/erx_receipt1_1_1.json");
    const model::ErxReceipt receipt = model::ErxReceipt::fromJsonNoValidation(receiptJson);

    task.setStatus(model::Task::Status::completed);

    std::vector<model::MedicationDispense> medicationDispenses;
    medicationDispenses.emplace_back(std::move(medicationDispense));
    database().updateTaskMedicationDispenseReceipt(task, medicationDispenses, receipt);
    database().commitTransaction();

    pqxx::result result;
    pqxx::row::size_type row = 0;
    ASSERT_NO_FATAL_FAILURE(retrieveTaskData(result, task.prescriptionId()));
    ASSERT_EQ(result.front().at(row++).as<int64_t>(), task.prescriptionId().toDatabaseId()); // prescription_id
    ASSERT_TRUE(result.front().at(row++).is_null()); // kvnr
    ASSERT_TRUE(result.front().at(row++).is_null()); // kvnr_hashed
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // last_modified
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // authored_on
    ASSERT_TRUE(result.front().at(row++).is_null()); // expiry_date
    ASSERT_TRUE(result.front().at(row++).is_null()); // accept_date
    ASSERT_EQ(result.front().at(row++).as<int>(), 3); // status
    ASSERT_FALSE(result.front().at(row++).is_null()); // key_generation_id
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // salt
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // access_code
    ASSERT_TRUE(result.front().at(row++).is_null()); // secret
    ASSERT_TRUE(result.front().at(row++).is_null()); // healthcare_provider_prescription
    ASSERT_FALSE(result.front().at(row++).is_null()); // receipt
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // when_handed_over
    ASSERT_TRUE(result.front().at(row++).is_null()); // when_prepared
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // performer
    ASSERT_FALSE(result.front().at(row++).is_null()); // medication_dispense_bundle
}

TEST_P(PostgresDatabaseTaskTest, updateTaskClearPersonalData)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    model::Task task(prescriptionType(), "access_code");
    task.setStatus(model::Task::Status::ready);
    const auto id = database().storeTask(task);
    database().commitTransaction();
    task.setPrescriptionId(id);

    {
        const char* const kvnr = InsurantA;
        const char* const healthCareProviderPrescription = "HealthCareProviderPrescription";
        const char* const receipt = "Receipt";
        const char* const performer = "Performer";
        const char* const medicationDispense = "MedicationDispense";

        const Query updateTaskComplete{
                "updateTaskComplete",
                "UPDATE " + taskTableName() +
                " SET status = $2, kvnr = $3, healthcare_provider_prescription = $4, "
                "    receipt = $5, performer = $6, medication_dispense_bundle = $7 "
                "WHERE prescription_id = $1"};
        prepare(updateTaskComplete);

        pqxx::result result;
        auto &&txn = createTransaction();
        ASSERT_NO_THROW(result = txn.exec_prepared(
            updateTaskComplete.name, id.toDatabaseId(), 1, kvnr, healthCareProviderPrescription,
            receipt, performer, medicationDispense));
        txn.commit();
        ASSERT_TRUE(result.empty());
    }

    task.setStatus(model::Task::Status::cancelled);
    database().updateTaskClearPersonalData(task);
    database().commitTransaction();

    {
        pqxx::row::size_type row = 0;
        pqxx::result result;
        ASSERT_NO_FATAL_FAILURE(retrieveTaskData(result, task.prescriptionId()));
        ASSERT_EQ(result.front().at(row++).as<int64_t>(), task.prescriptionId().toDatabaseId()); // prescription_id
        ASSERT_TRUE(result.front().at(row++).is_null()); // kvnr
        ASSERT_TRUE(result.front().at(row++).is_null()); // kvnr_hashed
        ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // last_modified
        ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // authored_on
        ASSERT_TRUE(result.front().at(row++).is_null()); // expiry_date
        ASSERT_TRUE(result.front().at(row++).is_null()); // accept_date
        ASSERT_EQ(result.front().at(row++).as<int>(), 4); // status
        ASSERT_FALSE(result.front().at(row++).is_null()); // key_generation_id
        ASSERT_TRUE(result.front().at(row++).is_null()); // salt
        ASSERT_TRUE(result.front().at(row++).is_null()); // access_code
        ASSERT_TRUE(result.front().at(row++).is_null()); // secret
        ASSERT_TRUE(result.front().at(row++).is_null()); // healthcare_provider_prescription
        ASSERT_TRUE(result.front().at(row++).is_null()); // receipt
        ASSERT_TRUE(result.front().at(row++).is_null()); // when_handed_over
        ASSERT_TRUE(result.front().at(row++).is_null()); // when_prepared
        ASSERT_TRUE(result.front().at(row++).is_null()); // performer
        ASSERT_TRUE(result.front().at(row++).is_null()); // medication_dispense_bundle
    }

    const auto updatedTask = std::get<0>(database().retrieveTaskAndPrescription(task.prescriptionId()));
    database().commitTransaction();
    ASSERT_TRUE(updatedTask);
    ASSERT_EQ(updatedTask->status(), model::Task::Status::cancelled);
}

TEST_P(PostgresDatabaseTaskTest, SearchTasksStatus)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    const std::string kvnr1 = "XYZABC1234";

    cleanKvnr(kvnr1, taskTableName());

    std::vector<model::Task> tasks;

    setupTasks(3, kvnr1, tasks);

    const auto& id1 = tasks[0].prescriptionId();
    const auto& id2 = tasks[1].prescriptionId();
    const auto& id3 = tasks[2].prescriptionId();

    {
        auto&& txn = createTransaction();
        txn.exec("UPDATE " + taskTableName() + " SET status = 3 WHERE prescription_id='" + std::to_string(id1.toDatabaseId()) + "'");
        txn.exec("UPDATE " + taskTableName() + " SET status = 3 WHERE prescription_id='" + std::to_string(id3.toDatabaseId()) + "'");
        txn.commit();
    }

    // we now have one task in status=ready and two in status=completed

    A_19569_02.test("SearchParameter: Task.status");
    {
        auto result = database().retrieveAllTasksForPatient(kvnr1, searchForStatus("ready"));
        ASSERT_EQ(result.size(), 1);
        const auto& task1 = result[0];
        EXPECT_EQ(task1.prescriptionId().toDatabaseId() , id2.toDatabaseId());
    }

    {
        auto result = database().retrieveAllTasksForPatient(kvnr1, searchForStatus("in-progress"));
        ASSERT_EQ(result.size(), 0);
    }

    {
        auto result = database().retrieveAllTasksForPatient(kvnr1, searchForStatus("draft"));
        ASSERT_EQ(result.size(), 0);
    }

    {
        auto result = database().retrieveAllTasksForPatient(kvnr1, searchForStatus("completed"));
        ASSERT_EQ(result.size(), 2);

        const auto& task1 = result[0];
        const auto& task2 = result[1];
        EXPECT_TRUE((task1.prescriptionId().toDatabaseId() == id1.toDatabaseId() &&
                     task2.prescriptionId().toDatabaseId() == id3.toDatabaseId()) ||
                    (task1.prescriptionId().toDatabaseId() == id3.toDatabaseId() &&
                     task2.prescriptionId().toDatabaseId() == id1.toDatabaseId()));
    }
    database().commitTransaction();

    cleanKvnr(kvnr1, taskTableName());
}

TEST_P(PostgresDatabaseTaskTest, SearchTasksLastModified)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    const std::string kvnr1 = "XYZABC1234";

    cleanKvnr(kvnr1, taskTableName());

    std::vector<model::Task> tasks;

    setupTasks(3, kvnr1, tasks);

    using namespace std::chrono_literals;
    auto lastModified1 = tasks[0].lastModifiedDate();
    auto lastModified2 = lastModified1 + 5s;
    auto lastModified3 = lastModified1 + 10s;

    const auto& id2 = tasks[1].prescriptionId();
    const auto& id3 = tasks[2].prescriptionId();

    {
        auto&& txn = createTransaction();
        txn.exec("UPDATE " + taskTableName() + " SET last_modified = '" +
                 lastModified2.toXsDateTimeWithoutFractionalSeconds() + "' WHERE prescription_id='" +
                 std::to_string(id2.toDatabaseId()) + "'");
        txn.exec("UPDATE " + taskTableName() + " SET last_modified = '" +
                 lastModified3.toXsDateTimeWithoutFractionalSeconds() + "' WHERE prescription_id='" +
                 std::to_string(id3.toDatabaseId()) + "'");
        txn.commit();
    }

    A_19569_02.test("SearchParameter: Task.lastModified");
    {
        auto result =
            database().retrieveAllTasksForPatient(kvnr1, searchForLastModified(lastModified1, ""));
        EXPECT_EQ(result.size(), 1);
    }

    {
        auto result =
            database().retrieveAllTasksForPatient(kvnr1, searchForLastModified(lastModified1, "eq"));
        EXPECT_EQ(result.size(), 1);
    }

    {
        auto result =
            database().retrieveAllTasksForPatient(kvnr1, searchForLastModified(lastModified1, "lt"));
        EXPECT_EQ(result.size(), 0);
    }

    {
        auto result =
            database().retrieveAllTasksForPatient(kvnr1, searchForLastModified(lastModified1, "ne"));
        EXPECT_EQ(result.size(), 2);
    }

    {
        auto result =
            database().retrieveAllTasksForPatient(kvnr1, searchForLastModified(lastModified1, "gt"));
        EXPECT_EQ(result.size(), 2);
    }

    {
        auto result =
            database().retrieveAllTasksForPatient(kvnr1, searchForLastModified(lastModified1, "ge"));
        EXPECT_EQ(result.size(), 3);
    }

    {
        auto result =
            database().retrieveAllTasksForPatient(kvnr1, searchForLastModified(lastModified1, "le"));
        EXPECT_EQ(result.size(), 1);
    }
    database().commitTransaction();

    cleanKvnr(kvnr1, taskTableName());
}

TEST_P(PostgresDatabaseTaskTest, SearchTasksAuthoredOn)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    const std::string kvnr1 = "XYZABC1234";

    cleanKvnr(kvnr1, taskTableName());

    std::vector<model::Task> tasks;

    setupTasks(3, kvnr1, tasks);

    using namespace std::chrono_literals;
    auto authoredOn1 = tasks[0].authoredOn();
    auto authoredOn2 = authoredOn1 + 5s;
    auto authoredOn3 = authoredOn1 + 10s;

    const auto& id2 = tasks[1].prescriptionId();
    const auto& id3 = tasks[2].prescriptionId();

    {
        auto&& txn = createTransaction();
        txn.exec("UPDATE " + taskTableName() + " SET authored_on = '" +
            authoredOn2.toXsDateTimeWithoutFractionalSeconds() + "' WHERE prescription_id='" +
            std::to_string(id2.toDatabaseId()) + "'");
        txn.exec("UPDATE " + taskTableName() + " SET authored_on = '" +
            authoredOn3.toXsDateTimeWithoutFractionalSeconds() + "' WHERE prescription_id='" +
            std::to_string(id3.toDatabaseId()) + "'");
        txn.commit();
    }

    A_19569_02.test("SearchParameter: Task.authoredOn");
    {
        auto result =
            database().retrieveAllTasksForPatient(kvnr1, searchForAuthoredOn(authoredOn1, ""));
        EXPECT_EQ(result.size(), 1);
    }

    {
        auto result =
            database().retrieveAllTasksForPatient(kvnr1, searchForAuthoredOn(authoredOn1, "eq"));
        EXPECT_EQ(result.size(), 1);
    }

    {
        auto result =
            database().retrieveAllTasksForPatient(kvnr1, searchForAuthoredOn(authoredOn1, "lt"));
        EXPECT_EQ(result.size(), 0);
    }

    {
        auto result =
            database().retrieveAllTasksForPatient(kvnr1, searchForAuthoredOn(authoredOn1, "ne"));
        EXPECT_EQ(result.size(), 2);
    }

    {
        auto result =
            database().retrieveAllTasksForPatient(kvnr1, searchForAuthoredOn(authoredOn1, "gt"));
        EXPECT_EQ(result.size(), 2);
    }

    {
        auto result =
            database().retrieveAllTasksForPatient(kvnr1, searchForAuthoredOn(authoredOn1, "ge"));
        EXPECT_EQ(result.size(), 3);
    }

    {
        auto result =
            database().retrieveAllTasksForPatient(kvnr1, searchForAuthoredOn(authoredOn1, "le"));
        EXPECT_EQ(result.size(), 1);
    }
    database().commitTransaction();

    cleanKvnr(kvnr1, taskTableName());
}

TEST_P(PostgresDatabaseTaskTest, SearchTasksSort)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    const std::string kvnr1 = "XYZABC1234";

    cleanKvnr(kvnr1, taskTableName());

    std::vector<model::Task> tasks;

    setupTasks(3, kvnr1, tasks);

    using namespace std::chrono_literals;
    auto authoredOn1 = tasks[0].authoredOn();
    auto authoredOn2 = authoredOn1 + -5s;
    auto authoredOn3 = authoredOn1 + 10s;

    const auto& id1 = tasks[0].prescriptionId();
    const auto& id2 = tasks[1].prescriptionId();
    const auto& id3 = tasks[2].prescriptionId();

    {
        auto&& txn = createTransaction();
        txn.exec("UPDATE " + taskTableName() + " SET authored_on = '" +
            authoredOn2.toXsDateTimeWithoutFractionalSeconds() + "' WHERE prescription_id='" +
            std::to_string(id2.toDatabaseId()) + "'");
        txn.exec("UPDATE " + taskTableName() + " SET authored_on = '" +
            authoredOn3.toXsDateTimeWithoutFractionalSeconds() + "' WHERE prescription_id='" +
            std::to_string(id3.toDatabaseId()) + "'");
        txn.commit();
    }

    {
        auto result =
            database().retrieveAllTasksForPatient(kvnr1, sortByAuthoredOn(false));
        EXPECT_EQ(result.size(), 3);
        EXPECT_EQ(result.at(0).prescriptionId().toDatabaseId(), id2.toDatabaseId());
        EXPECT_EQ(result.at(1).prescriptionId().toDatabaseId(), id1.toDatabaseId());
        EXPECT_EQ(result.at(2).prescriptionId().toDatabaseId(), id3.toDatabaseId());
    }

    {
        auto result =
            database().retrieveAllTasksForPatient(kvnr1, sortByAuthoredOn(true));
        EXPECT_EQ(result.size(), 3);
        EXPECT_EQ(result.at(0).prescriptionId().toDatabaseId(), id3.toDatabaseId());
        EXPECT_EQ(result.at(1).prescriptionId().toDatabaseId(), id1.toDatabaseId());
        EXPECT_EQ(result.at(2).prescriptionId().toDatabaseId(), id2.toDatabaseId());
    }
    database().commitTransaction();

    cleanKvnr(kvnr1, taskTableName());
}

TEST_P(PostgresDatabaseTaskTest, SearchTasksPaging)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    const std::string kvnr1 = "XYZABC1234";

    cleanKvnr(kvnr1, taskTableName());

    std::vector<model::Task> tasks;

    setupTasks(3, kvnr1, tasks, model::PrescriptionType::apothekenpflichigeArzneimittel);
    setupTasks(2, kvnr1, tasks, model::PrescriptionType::direkteZuweisung);

    {
        auto result = database().retrieveAllTasksForPatient(kvnr1, paging(2, 0));
        EXPECT_EQ(result.size(), 2);
    }

    {
        auto result = database().retrieveAllTasksForPatient(kvnr1, paging(5, 0));
        EXPECT_EQ(result.size(), 5);
    }


    {
        auto result = database().retrieveAllTasksForPatient(kvnr1, paging(4, 1));
        EXPECT_EQ(result.size(), 4);
    }

    {
        auto result = database().retrieveAllTasksForPatient(kvnr1, paging(1, 5));
        EXPECT_EQ(result.size(), 0);
    }

    {
        auto result = database().retrieveAllTasksForPatient(kvnr1, paging(10, 0));
        EXPECT_EQ(result.size(), 5);
    }

    {
        auto result = database().retrieveAllTasksForPatient(kvnr1, paging(3, 3));
        EXPECT_EQ(result.size(), 2);
    }

    {
        EXPECT_ERP_EXCEPTION(database().retrieveAllTasksForPatient(kvnr1, paging(0, -1)),
                             HttpStatus::BadRequest);
        EXPECT_ERP_EXCEPTION(database().retrieveAllTasksForPatient(kvnr1, paging(-1, -1)),
                             HttpStatus::BadRequest);
        EXPECT_ERP_EXCEPTION(database().retrieveAllTasksForPatient(kvnr1, paging(1, -1)),
                             HttpStatus::BadRequest);
    }
    database().commitTransaction();

    cleanKvnr(kvnr1, taskTableName());
}

TEST_P(PostgresDatabaseTaskTest, createAndReadAuditEventData)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    cleanKvnr(InsurantA, taskTableName());

    const char* const kvnr = InsurantA;
    const char* const telematicId = "3-SMC-XXXX-883110000120312";
    const auto deviceId = 42;

    model::Task task(prescriptionType(), "access_code");
    task.setStatus(model::Task::Status::ready);

    const auto id = database().storeTask(task);
    task.setPrescriptionId(id);
    task.setKvnr(kvnr);

    const std::string_view agentName = "Apotheke Am Grünen Baum";
    model::AuditData auditDataOrig(
        model::AuditEventId::GET_Task_id_pharmacy,
        model::AuditMetaData(agentName, telematicId),
        model::AuditEvent::Action::read, model::AuditEvent::AgentType::human, kvnr, deviceId, id, std::nullopt);
    const std::string createdUuid = database().storeAuditEventData(auditDataOrig);
    database().commitTransaction();

    const std::vector<model::AuditData> savedAuditData = database().retrieveAuditEventData(kvnr, {}, {}, {});
    database().commitTransaction();

    ASSERT_EQ(savedAuditData.size(), 1);
    const auto& entry = savedAuditData.front();

    EXPECT_EQ(entry.id(), createdUuid);
    EXPECT_EQ(entry.eventId(), auditDataOrig.eventId());
    EXPECT_EQ(entry.insurantKvnr(), auditDataOrig.insurantKvnr());
    EXPECT_EQ(entry.action(), auditDataOrig.action());
    EXPECT_EQ(entry.metaData().agentWho(), auditDataOrig.metaData().agentWho());
    EXPECT_EQ(entry.metaData().agentName(), auditDataOrig.metaData().agentName());
    EXPECT_EQ(entry.agentType(), auditDataOrig.agentType());
    EXPECT_TRUE(entry.prescriptionId().has_value());
    EXPECT_EQ(entry.prescriptionId().value().toString(), auditDataOrig.prescriptionId().value().toString());
    EXPECT_EQ(entry.deviceId(), auditDataOrig.deviceId());
    EXPECT_EQ(entry.id(), auditDataOrig.id());
    EXPECT_EQ(entry.recorded(), auditDataOrig.recorded());
    EXPECT_FALSE(entry.consentId().has_value());

    cleanKvnr(InsurantA, taskTableName());
}

INSTANTIATE_TEST_SUITE_P(PostgresDatabaseTaskTestInst, PostgresDatabaseTaskTest,
                         testing::Values(model::PrescriptionType::apothekenpflichigeArzneimittel,
                                         model::PrescriptionType::direkteZuweisung));
