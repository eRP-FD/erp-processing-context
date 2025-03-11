/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */


#if defined (__GNUC__) && __GNUC__ >= 12
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#include <pqxx/pqxx>
#pragma GCC diagnostic pop
#else
#include <pqxx/pqxx>
#endif

#include "erp/model/Binary.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/model/GemErpPrMedication.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/MedicationDispenseBundle.hxx"
#include "erp/model/Task.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/model/AuditData.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/util/Uuid.hxx"
#include "test_config.h"
#include "test/erp/database/PostgresDatabaseTest.hxx"
#include "test/util/CryptoHelper.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/ResourceTemplates.hxx"


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

            prepare(mRetrieveTask);
        }
    }

    void cleanup() override
    {
        if (usePostgres())
        {
            clearTables();
        }
    }

    using PostgresDatabaseTest::taskTableName;
    std::string taskTableName() const
    {
        return taskTableName(GetParam());
    }

    model::PrescriptionType prescriptionType() const
    {
        return GetParam();
    }

    void retrieveTaskData(pqxx::result& result, const model::PrescriptionId& prescriptionId)
    {
        auto &&txn = createTransaction();
        ASSERT_NO_THROW(result = txn.exec_prepared(mRetrieveTask.name, prescriptionId.toDatabaseId()));
        txn.commit();
        ASSERT_FALSE(result.empty());
        ASSERT_EQ(result.size(), 1);
        ASSERT_FALSE(result.front().empty());
        ASSERT_EQ(result.front().size(), 25);
    }

    UrlArguments searchForStatus(const std::string& status)
    {
        auto search = UrlArguments({{"status", SearchParameter::Type::TaskStatus}});
        auto request = ServerRequest(Header());
        request.setQueryParameters({{"status", status}});
        search.parse(request, getKeyDerivation());
        return search;
    }

    UrlArguments searchForLastModified(const model::Timestamp& timestamp, const std::string& operation)
    {
        auto search = UrlArguments({{"modified", "last_modified", SearchParameter::Type::Date}});
        auto request = ServerRequest(Header());
        request.setQueryParameters({{"modified", operation + timestamp.toXsDateTimeWithoutFractionalSeconds()}});
        search.parse(request, getKeyDerivation());
        return search;
    }

    UrlArguments searchForAuthoredOn(const model::Timestamp& timestamp, const std::string& operation)
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

    void setupTasks(size_t nTasks, const model::Kvnr& kvnr, std::vector<model::Task>& outTasks,
                    model::PrescriptionType prescriptionType = GetParam())
    {
        for (size_t i = 0; i < nTasks; ++i)
        {
            model::Task task1(prescriptionType, "access_code");
            const auto id1 = database().storeTask(task1);
            // The database generates the ID
            task1.setPrescriptionId(id1);
            const auto type = id1.isPkv() ? model::Kvnr::Type::pkv : model::Kvnr::Type::gkv;
            // assign KVNRs
            task1.setKvnr(model::Kvnr{kvnr.id(), type});
            task1.setExpiryDate(model::Timestamp::now());
            task1.setAcceptDate(model::Timestamp::now());
            task1.setStatus(model::Task::Status::ready);
            database().activateTask(
                task1, model::Binary(Uuid().toString(),
                                     model::Bundle(model::BundleType::document, ::model::FhirResourceBase::NoProfile)
                                         .serializeToJsonString()), mJwtBuilder.makeJwtArzt());
            database().commitTransaction();

            outTasks.emplace_back(std::move(task1));
        }
    }

private:
    const Query mRetrieveTask{
        "retrieveTask",
        "SELECT * "
        "FROM " + taskTableName() +
            " WHERE prescription_id = $1"};
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
    ASSERT_FALSE(result.front().at(row++).is_null()); // task_key_blob_id
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // salt
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // access_code
    ASSERT_TRUE(result.front().at(row++).is_null()); // secret
    ASSERT_TRUE(result.front().at(row++).is_null()); // healthcare_provider_prescription
    ASSERT_TRUE(result.front().at(row++).is_null()); // receipt
    ASSERT_TRUE(result.front().at(row++).is_null()); // when_handed_over
    ASSERT_TRUE(result.front().at(row++).is_null()); // when_prepared
    ASSERT_TRUE(result.front().at(row++).is_null()); // performer
    ASSERT_TRUE(result.front().at(row++).is_null()); // medication_dispense_blob_id
    ASSERT_TRUE(result.front().at(row++).is_null()); // medication_dispense_bundle
    ASSERT_TRUE(result.front().at(row++).is_null()); // owner
    ASSERT_TRUE(result.front().at(row++).is_null()); // last_medication_dispense
    ASSERT_TRUE(result.front().at(row++).is_null()); // medication_dispense_salt
}

TEST_P(PostgresDatabaseTaskTest, updateTaskActivate)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    model::Task task1(prescriptionType(), "access_code");
    const auto kvnrType = static_cast<int>(prescriptionType()) < 200 ? model::Kvnr::Type::gkv : model::Kvnr::Type::pkv;
    const auto kvnr = model::Kvnr{InsurantA, kvnrType};
    task1.setKvnr(kvnr);

    // does not store the KVNR:
    task1.setPrescriptionId(database().storeTask(task1));
    database().commitTransaction();
    ASSERT_TRUE(database().retrieveAllTasksForPatient(kvnr, {}).empty());

    task1.setStatus(model::Task::Status::ready);
    task1.setExpiryDate(model::Timestamp::now());
    task1.setAcceptDate(model::Timestamp::now());
    database().activateTask(
        task1,
        model::Binary(
            Uuid().toString(),
            model::Bundle(model::BundleType::document, ::model::FhirResourceBase::NoProfile).serializeToJsonString()),
        mJwtBuilder.makeJwtArzt());
    database().commitTransaction();

    // KVNR is now set
    ASSERT_FALSE(database().retrieveAllTasksForPatient(kvnr, {}).empty());
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
    ASSERT_FALSE(result.front().at(row++).is_null()); // task_key_blob_id
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // salt
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // access_code
    ASSERT_TRUE(result.front().at(row++).is_null()); // secret
    ASSERT_TRUE(!result.front().at(row++).as<std::string>().empty()); // healthcare_provider_prescription
    ASSERT_TRUE(result.front().at(row++).is_null()); // receipt
    ASSERT_TRUE(result.front().at(row++).is_null()); // when_handed_over
    ASSERT_TRUE(result.front().at(row++).is_null()); // when_prepared
    ASSERT_TRUE(result.front().at(row++).is_null()); // performer
    ASSERT_TRUE(result.front().at(row++).is_null()); // medication_dispense_blob_id
    ASSERT_TRUE(result.front().at(row++).is_null()); // medication_dispense_bundle
    ASSERT_TRUE(result.front().at(row++).is_null()); // owner
    ASSERT_TRUE(result.front().at(row++).is_null()); // last_medication_dispense
}

TEST_P(PostgresDatabaseTaskTest, updateTaskStatusAndSecret)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    model::Task task1(prescriptionType(), "access_code");
    auto kvnrType = static_cast<int>(prescriptionType()) < 200 ? model::Kvnr::Type::gkv : model::Kvnr::Type::pkv;
    task1.setKvnr(model::Kvnr{std::string{InsurantA}, kvnrType});

    // does not store the KVNR:
    task1.setPrescriptionId(database().storeTask(task1));
    database().commitTransaction();

    {
        const auto taskFromDb = std::get<0>(database().retrieveTaskAndPrescription(task1.prescriptionId()));
        ASSERT_EQ(taskFromDb->task.status(), model::Task::Status::draft);
        const auto task2FromDb = database().retrieveTaskForUpdate(task1.prescriptionId());
        ASSERT_EQ(task2FromDb->task.status(), model::Task::Status::draft);
    }

    task1.setStatus(model::Task::Status::ready);
    task1.setSecret("secret");
    task1.setOwner("owner");
    database().updateTaskStatusAndSecret(task1);
    database().commitTransaction();

    {
        const auto taskFromDb = std::get<0>(database().retrieveTaskAndReceipt(task1.prescriptionId()));
        ASSERT_EQ(taskFromDb->status(), model::Task::Status::ready);
        ASSERT_EQ(taskFromDb->secret(), "secret");
        A_24174.test("owner has been stored in Database");
        // GEMREQ-start A_24174#test1
        ASSERT_EQ(taskFromDb->owner(), "owner");
        // GEMREQ-end A_24174#test1
    }
    {
        const auto taskFromDb = std::get<0>(database().retrieveTaskAndPrescription(task1.prescriptionId()));
        ASSERT_EQ(taskFromDb->task.status(), model::Task::Status::ready);
        ASSERT_EQ(taskFromDb->task.secret(), std::nullopt);
        ASSERT_EQ(taskFromDb->task.owner(), "owner");
    }
    {
        const auto taskFromDb = std::get<0>(database().retrieveTaskForUpdateAndPrescription(task1.prescriptionId()));
        ASSERT_EQ(taskFromDb->task.status(), model::Task::Status::ready);
        ASSERT_EQ(taskFromDb->task.secret(), "secret");
        ASSERT_EQ(taskFromDb->task.owner(), "owner");
    }
    {
        const auto taskFromDb = std::get<0>(database().retrieveTaskAndPrescriptionAndReceipt(task1.prescriptionId()));
        ASSERT_EQ(taskFromDb->status(), model::Task::Status::ready);
        ASSERT_EQ(taskFromDb->secret(), "secret");
        ASSERT_EQ(taskFromDb->owner(), "owner");
    }
    {
        const auto taskFromDb = database().retrieveTaskForUpdate(task1.prescriptionId());
        ASSERT_EQ(taskFromDb->task.status(), model::Task::Status::ready);
        ASSERT_EQ(taskFromDb->task.secret(), "secret");
        ASSERT_EQ(taskFromDb->task.owner(), "owner");
    }
    database().commitTransaction();
}

TEST_P(PostgresDatabaseTaskTest, updateTaskStatusAndSecretNoOwner)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    model::Task task1(prescriptionType(), "access_code");
    auto kvnrType = static_cast<int>(prescriptionType()) < 200 ? model::Kvnr::Type::gkv : model::Kvnr::Type::pkv;
    task1.setKvnr(model::Kvnr{std::string{InsurantA}, kvnrType});

    // does not store the KVNR:
    task1.setPrescriptionId(database().storeTask(task1));
    database().commitTransaction();

    {
        const auto taskFromDb = std::get<0>(database().retrieveTaskAndPrescription(task1.prescriptionId()));
        ASSERT_EQ(taskFromDb->task.status(), model::Task::Status::draft);
        const auto task2FromDb = database().retrieveTaskForUpdate(task1.prescriptionId());
        ASSERT_EQ(task2FromDb->task.status(), model::Task::Status::draft);
    }

    task1.setStatus(model::Task::Status::ready);
    task1.setSecret("secret");
    database().updateTaskStatusAndSecret(task1);
    database().commitTransaction();

    {
        const auto taskFromDb = std::get<0>(database().retrieveTaskAndReceipt(task1.prescriptionId()));
        ASSERT_EQ(taskFromDb->status(), model::Task::Status::ready);
        ASSERT_EQ(taskFromDb->secret(), "secret");
        ASSERT_EQ(taskFromDb->owner(), std::nullopt);
        database().commitTransaction();
    }
    {
        const auto taskFromDb = std::get<0>(database().retrieveTaskAndPrescription(task1.prescriptionId()));
        ASSERT_EQ(taskFromDb->task.status(), model::Task::Status::ready);
        ASSERT_EQ(taskFromDb->task.secret(), std::nullopt);
        ASSERT_EQ(taskFromDb->task.owner(), std::nullopt);
        database().commitTransaction();
    }
    {
        const auto taskFromDb = std::get<0>(database().retrieveTaskForUpdateAndPrescription(task1.prescriptionId()));
        ASSERT_EQ(taskFromDb->task.status(), model::Task::Status::ready);
        ASSERT_EQ(taskFromDb->task.secret(), "secret");
        ASSERT_EQ(taskFromDb->task.owner(), std::nullopt);
        database().commitTransaction();
    }
    {
        const auto taskFromDb = std::get<0>(database().retrieveTaskAndPrescriptionAndReceipt(task1.prescriptionId()));
        ASSERT_EQ(taskFromDb->status(), model::Task::Status::ready);
        ASSERT_EQ(taskFromDb->secret(), "secret");
        ASSERT_EQ(taskFromDb->owner(), std::nullopt);
        database().commitTransaction();
    }
    {
        const auto taskFromDb = database().retrieveTaskForUpdate(task1.prescriptionId());
        ASSERT_EQ(taskFromDb->task.status(), model::Task::Status::ready);
        ASSERT_EQ(taskFromDb->task.secret(), "secret");
        ASSERT_EQ(taskFromDb->task.owner(), std::nullopt);
        database().commitTransaction();
    }
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
    task.setKvnr(model::Kvnr{std::string{"X123456788"}, id.isPkv() ? model::Kvnr::Type::pkv : model::Kvnr::Type::gkv});
    task.setAcceptDate(model::Timestamp::now());
    task.setExpiryDate(model::Timestamp::now());

    database().activateTask(task, model::Binary{Uuid().toString(), "HealthCareProviderPrescription"},
                            mJwtBuilder.makeJwtArzt());
    database().commitTransaction();

    const auto [task1, healthCareProviderPrescription] = database().retrieveTaskAndPrescription(id);
    database().commitTransaction();
    ASSERT_TRUE(task1.has_value());
    ASSERT_TRUE(healthCareProviderPrescription.has_value());
    ASSERT_EQ(std::string(healthCareProviderPrescription.value().data().value()), "HealthCareProviderPrescription");
}

// GEMREQ-start A_19115-01
TEST_P(PostgresDatabaseTaskTest, retrieveAllTasksForPatient)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    A_19115_01.test("Ensure only own tasks can be retrieved by KVNR");

    const auto kvnrType = static_cast<int>(prescriptionType()) < 200 ? model::Kvnr::Type::gkv : model::Kvnr::Type::pkv;
    const model::Kvnr kvnr1{"X678901238", kvnrType};
    const model::Kvnr kvnr2{"X012345676", kvnrType};

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
    task1.setExpiryDate(model::Timestamp::now());
    task1.setAcceptDate(model::Timestamp::now());
    database().activateTask(
        task1,
        model::Binary(
            Uuid().toString(),
            model::Bundle(model::BundleType::document, ::model::FhirResourceBase::NoProfile).serializeToJsonString()),
        mJwtBuilder.makeJwtArzt());
    task2.setKvnr(kvnr2);
    task2.setExpiryDate(model::Timestamp::now());
    task2.setAcceptDate(model::Timestamp::now());
    database().activateTask(
        task2,
        model::Binary(
            Uuid().toString(),
            model::Bundle(model::BundleType::document, ::model::FhirResourceBase::NoProfile).serializeToJsonString()),
        mJwtBuilder.makeJwtArzt());
    task3.setKvnr(kvnr2);
    task3.setExpiryDate(model::Timestamp::now());
    task3.setAcceptDate(model::Timestamp::now());
    database().activateTask(
        task3,
        model::Binary(
            Uuid().toString(),
            model::Bundle(model::BundleType::document, ::model::FhirResourceBase::NoProfile).serializeToJsonString()),
        mJwtBuilder.makeJwtArzt());
    database().commitTransaction();

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
// GEMREQ-end A_19115-01

// GEMREQ-start A_23452-02
TEST_P(PostgresDatabaseTaskTest, retrieveAllTasksForPatientAsPharmacy)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    const model::Kvnr kvnr{"X012345676"};
    std::vector<model::Task> tasks;
    setupTasks(1, kvnr, tasks);

    A_23452_02.test("Ensure only tasks for workflow 160 can be retrieved by KVNR");
    {
        auto result = database().retrieveAll160TasksWithAccessCode(kvnr, searchForStatus("ready"));
        if(prescriptionType() == ::model::PrescriptionType::apothekenpflichigeArzneimittel)
        {
            ASSERT_EQ(result.size(), 1);
            EXPECT_NO_THROW(auto accessCode [[maybe_unused]] = result[0].accessCode());
            EXPECT_EQ(result[0].status(), model::Task::Status::ready);
            EXPECT_EQ(result[0].kvnr(), kvnr);
        }
        else
        {
            ASSERT_EQ(result.size(), 0);
        }
    }
    database().commitTransaction();
}
// GEMREQ-end A_23452-02

TEST_P(PostgresDatabaseTaskTest, updateTaskMedicationDispense)//NOLINT(readability-function-cognitive-complexity)
{
    using namespace ResourceTemplates;
    if (!usePostgres() || Versions::GEM_ERP_current() < Versions::GEM_ERP_1_3)
    {
        GTEST_SKIP();
    }
    model::Task task(prescriptionType(), "access_code");
    task.setPrescriptionId(database().storeTask(task));
    auto kvnrType = static_cast<int>(prescriptionType()) < 200 ? model::Kvnr::Type::gkv : model::Kvnr::Type::pkv;
    task.setKvnr(model::Kvnr{std::string{"X123456788"}, kvnrType});

    const auto medicationDispenseJson =
        FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/medication_dispense_output1.json");
    model::MedicationDispense medicationDispense =
        model::MedicationDispense::fromJsonNoValidation(medicationDispenseJson);

    const auto receiptJson =
        FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/erx_receipt1_1_1.json");
    const model::ErxReceipt receipt = model::ErxReceipt::fromJsonNoValidation(receiptJson);

    task.updateLastMedicationDispense();

    std::vector<model::MedicationDispense> medicationDispenses;
    medicationDispenses.emplace_back(std::move(medicationDispense));
    database().updateTaskMedicationDispense(task, model::MedicationDispenseBundle{"", medicationDispenses, {}});
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
    ASSERT_EQ(result.front().at(row++).as<int>(), 0); // status
    ASSERT_FALSE(result.front().at(row++).is_null()); // task_key_blob_id
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // salt
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // access_code
    ASSERT_TRUE(result.front().at(row++).is_null()); // secret
    ASSERT_TRUE(result.front().at(row++).is_null()); // healthcare_provider_prescription
    ASSERT_TRUE(result.front().at(row++).is_null()); // receipt
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // when_handed_over
    ASSERT_TRUE(result.front().at(row++).is_null()); // when_prepared
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // performer
    ASSERT_FALSE(result.front().at(row++).is_null()); // medication_dispense_blob_id
    ASSERT_FALSE(result.front().at(row++).is_null()); // medication_dispense_bundle
    ASSERT_TRUE(result.front().at(row++).is_null()); // owner
    if (task.lastMedicationDispense())
    {
        ASSERT_FALSE(result.front().at(row++).is_null()); // last_medication_dispense
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
    auto kvnrType = static_cast<int>(prescriptionType()) < 200 ? model::Kvnr::Type::gkv : model::Kvnr::Type::pkv;
    task.setKvnr(model::Kvnr{std::string{"X123456788"}, kvnrType});

    const auto medicationDispenseJson =
        FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/medication_dispense_output1.json");
    model::MedicationDispense medicationDispense =
        model::MedicationDispense::fromJsonNoValidation(medicationDispenseJson);

    const auto receiptJson =
        FileHelper::readFileAsString(std::string(TEST_DATA_DIR) + "/EndpointHandlerTest/erx_receipt1_1_1.json");
    const model::ErxReceipt receipt = model::ErxReceipt::fromJsonNoValidation(receiptJson);

    task.setStatus(model::Task::Status::completed);
    task.updateLastMedicationDispense();

    std::vector<model::MedicationDispense> medicationDispenses;
    medicationDispenses.emplace_back(std::move(medicationDispense));
    database().updateTaskMedicationDispenseReceipt(task, model::MedicationDispenseBundle{"", medicationDispenses, {}}, receipt, mJwtBuilder.makeJwtApotheke());
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
    ASSERT_FALSE(result.front().at(row++).is_null()); // task_key_blob_id
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // salt
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // access_code
    ASSERT_TRUE(result.front().at(row++).is_null()); // secret
    ASSERT_TRUE(result.front().at(row++).is_null()); // healthcare_provider_prescription
    ASSERT_FALSE(result.front().at(row++).is_null()); // receipt
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // when_handed_over
    ASSERT_TRUE(result.front().at(row++).is_null()); // when_prepared
    ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // performer
    ASSERT_FALSE(result.front().at(row++).is_null()); // medication_dispense_blob_id
    ASSERT_FALSE(result.front().at(row++).is_null()); // medication_dispense_bundle
    ASSERT_TRUE(result.front().at(row++).is_null()); // owner
    if (task.lastMedicationDispense())
    {
        ASSERT_FALSE(result.front().at(row++).is_null()); // last_medication_dispense
    }
    else
    {
        ASSERT_TRUE(result.front().at(row++).is_null()); // last_medication_dispense
    }
}

// GEMREQ-start A_19027-06
TEST_P(PostgresDatabaseTaskTest, updateTaskClearPersonalData)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    A_19027_06.test("Deletion of personal data from database");

    model::Task task(prescriptionType(), "access_code");
    task.setStatus(model::Task::Status::ready);
    const auto id = database().storeTask(task);
    database().commitTransaction();
    task.setPrescriptionId(id);

    {
        const auto kvnrType = static_cast<int>(prescriptionType()) < 200 ? model::Kvnr::Type::gkv : model::Kvnr::Type::pkv;
        const model::Kvnr kvnr{InsurantA, kvnrType};
        const auto hashedKvnr = getKeyDerivation().hashKvnr(kvnr);
        const char* const healthCareProviderPrescription = "HealthCareProviderPrescription";
        const char* const receipt = "Receipt";
        const char* const performer = "Performer";
        const char* const medicationDispense = "MedicationDispense";
        const char* const secret = "Secret";
        const char* const owner = "Owner";
        const auto whenHandedOver = model::Timestamp::now();
        const auto whenPrepared = model::Timestamp::now();
        const auto last_medication_dispense = model::Timestamp::now();
        SafeString key;
        OptionalDeriveKeyData secondCallData;
        std::tie(key, secondCallData) = getKeyDerivation().initialMedicationDispenseKey(hashedKvnr);

        const Query updateTaskComplete{
                "updateTaskComplete",
                "UPDATE " + taskTableName() +
                " SET status = $2, kvnr = $3, healthcare_provider_prescription = $4, "
                "    receipt = $5, performer = $6, medication_dispense_blob_id = $7, medication_dispense_bundle = $8, "
                "    kvnr_hashed = $9, secret = $10, when_handed_over = $11, when_prepared = $12, owner = $13, "
                "    last_medication_dispense = $14 "
                "WHERE prescription_id = $1"};
        prepare(updateTaskComplete);

        pqxx::result result;
        auto &&txn = createTransaction();
        // secondCallData.blobId causes a maybe-uninitialized warning in pqxx in gcc 12 when building with release build options
        ASSERT_NO_THROW(result = txn.exec_prepared(
            updateTaskComplete.name, id.toDatabaseId(), 1, kvnr.id(), healthCareProviderPrescription,
            receipt, performer, secondCallData.blobId, medicationDispense, hashedKvnr.binarystring(),
            secret, whenHandedOver.toXsDateTime(), whenPrepared.toXsDateTime(), owner,
            last_medication_dispense.toXsDateTime()));
        txn.commit();
        ASSERT_TRUE(result.empty());
    }
    {
        // make sure that all relevant database fields are filled
        pqxx::row::size_type row = 0;
        pqxx::result result;
        ASSERT_NO_FATAL_FAILURE(retrieveTaskData(result, task.prescriptionId()));
        ASSERT_EQ(result.front().at(row++).as<int64_t>(), task.prescriptionId().toDatabaseId()); // prescription_id
        ASSERT_FALSE(result.front().at(row++).is_null()); // kvnr
        ASSERT_FALSE(result.front().at(row++).is_null()); // kvnr_hashed
        ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // last_modified
        row++; // authored_on
        row++; // expiry_date
        row++; // accept_date
        ASSERT_EQ(result.front().at(row++).as<int>(), 1); // status
        row++; // task_key_blob_id
        ASSERT_FALSE(result.front().at(row++).is_null()); // salt
        ASSERT_FALSE(result.front().at(row++).is_null()); // access_code
        ASSERT_FALSE(result.front().at(row++).is_null()); // secret
        ASSERT_FALSE(result.front().at(row++).is_null()); // owner
        ASSERT_FALSE(result.front().at(row++).is_null()); // healthcare_provider_prescription
        ASSERT_FALSE(result.front().at(row++).is_null()); // receipt
        ASSERT_FALSE(result.front().at(row++).is_null()); // when_handed_over
        ASSERT_FALSE(result.front().at(row++).is_null()); // when_prepared
        ASSERT_FALSE(result.front().at(row++).is_null()); // performer
        ASSERT_FALSE(result.front().at(row++).is_null()); // medication_dispense_blob_id
        ASSERT_FALSE(result.front().at(row++).is_null()); // medication_dispense_bundle
        ASSERT_FALSE(result.front().at(row++).is_null()); // last_medication_dispense
    }
    task.setStatus(model::Task::Status::cancelled);
    database().updateTaskClearPersonalData(task);
    database().commitTransaction();
    {
        // make sure that relevant database fields are deleted
        pqxx::row::size_type row = 0;
        pqxx::result result;
        ASSERT_NO_FATAL_FAILURE(retrieveTaskData(result, task.prescriptionId()));
        ASSERT_EQ(result.front().at(row++).as<int64_t>(), task.prescriptionId().toDatabaseId()); // prescription_id
        ASSERT_TRUE(result.front().at(row++).is_null()); // kvnr
        ASSERT_FALSE(result.front().at(row++).is_null()); // kvnr_hashed
        ASSERT_NE(result.front().at(row++).as<std::string>(), ""); // last_modified
        row++; // authored_on
        row++; // expiry_date
        row++; // accept_date
        ASSERT_EQ(result.front().at(row++).as<int>(), 4); // status
        row++; // task_key_blob_id
        ASSERT_TRUE(result.front().at(row++).is_null()); // salt
        ASSERT_TRUE(result.front().at(row++).is_null()); // access_code
        ASSERT_TRUE(result.front().at(row++).is_null()); // secret
        ASSERT_TRUE(result.front().at(row++).is_null()); // owner
        ASSERT_TRUE(result.front().at(row++).is_null()); // healthcare_provider_prescription
        ASSERT_TRUE(result.front().at(row++).is_null()); // receipt
        ASSERT_TRUE(result.front().at(row++).is_null()); // when_handed_over
        ASSERT_TRUE(result.front().at(row++).is_null()); // when_prepared
        ASSERT_TRUE(result.front().at(row++).is_null()); // performer
        ASSERT_TRUE(result.front().at(row++).is_null()); // medication_dispense_blob_id
        ASSERT_TRUE(result.front().at(row++).is_null()); // medication_dispense_bundle
        ASSERT_TRUE(result.front().at(row++).is_null()); // last_medication_dispense
    }
}
// GEMREQ-end A_19027-06

TEST_P(PostgresDatabaseTaskTest, SearchTasksStatus)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }

    const auto kvnr1 = model::Kvnr{"X012341234"};

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

    A_19569_03.test("SearchParameter: Task.status");
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

    const auto kvnr1 = model::Kvnr{"X012341234"};

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

    A_19569_03.test("SearchParameter: Task.lastModified");
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

    const auto kvnr1 = model::Kvnr{"X012341234"};

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

    A_19569_03.test("SearchParameter: Task.authoredOn");
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

    const auto kvnr1 = model::Kvnr{"X012341234"};

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

    const auto kvnr1 = model::Kvnr{"X012341234"};

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
    const auto kvnrType = static_cast<int>(prescriptionType()) < 200 ? model::Kvnr::Type::gkv : model::Kvnr::Type::pkv;
    const model::Kvnr kvnr{InsurantA, kvnrType};

    cleanKvnr(kvnr, taskTableName());

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

    cleanKvnr(kvnr, taskTableName());
}

TEST_P(PostgresDatabaseTaskTest, retrieveTaskWithSecretAndPrescription)
{
    using namespace std::chrono_literals;
    if (!usePostgres())
    {
        GTEST_SKIP();
    }
    std::optional<model::Timestamp> authoredOn;
    std::optional<model::PrescriptionId> prescriptionId;
    std::string secret;
    std::string accessCode;
    std::string kvnr = "X234567891";
    { // create in-progress task
        auto task = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = model::PrescriptionId::fromDatabaseId(GetParam(), 0)}));
        prescriptionId.emplace(database().storeTask(task));
        task = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = *prescriptionId, .kvnr = kvnr}));
        authoredOn.emplace(task.authoredOn());
        const auto bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = *prescriptionId, .kvnr = kvnr});
        model::Binary bin{task.healthCarePrescriptionUuid().value() , CryptoHelper::toCadesBesSignature(bundle), model::Binary::Type::PKCS7};
        database().activateTask(task, bin, mJwtBuilder.makeJwtArzt());
        task = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::InProgress, .prescriptionId = *prescriptionId, .kvnr = kvnr}));
        task.updateLastUpdate(*authoredOn + 1s);
        accessCode = task.accessCode();
        secret = task.secret().value();
        database().updateTaskStatusAndSecret(task);
        database().commitTransaction();
    }
    auto expect = model::Task::fromJsonNoValidation(ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::InProgress, .prescriptionId = *prescriptionId, .kvnr = kvnr}));
    auto [task, prescriptionBin] = database().retrieveTaskWithSecretAndPrescription(*prescriptionId);
    database().commitTransaction();
    ASSERT_TRUE(task.has_value());
    EXPECT_EQ(task->status(), model::Task::Status::inprogress);
    EXPECT_EQ(task->kvnr(), kvnr);
    EXPECT_EQ(task->prescriptionId(), prescriptionId);
    EXPECT_EQ(task->authoredOn(), authoredOn);
    EXPECT_EQ(task->type(), GetParam());
    EXPECT_EQ(task->lastModifiedDate(), *authoredOn + 1s);
    EXPECT_EQ(task->accessCode(), expect.accessCode());
    EXPECT_EQ(task->expiryDate().toGermanDate(), expect.expiryDate().toGermanDate());
    EXPECT_EQ(task->acceptDate().toGermanDate(), expect.acceptDate().toGermanDate());
    EXPECT_EQ(task->secret(), secret);
    // not stored in db:
    // task->healthCarePrescriptionUuid()
    // task->patientConfirmationUuid()
    // task->receiptUuid()
    EXPECT_EQ(task->owner(), expect.owner());

    ASSERT_TRUE(prescriptionBin.has_value());
    EXPECT_EQ(prescriptionBin->id(), expect.healthCarePrescriptionUuid());
}


INSTANTIATE_TEST_SUITE_P(PostgresDatabaseTaskTestInst, PostgresDatabaseTaskTest,
                         testing::ValuesIn(magic_enum::enum_values<model::PrescriptionType>()));
