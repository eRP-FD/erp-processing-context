/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/database/PostgresBackend.hxx"
#include "erp/database/PostgresConnection.hxx"
#include "erp/hsm/ErpTypes.hxx"
#include "test/util/TestConfiguration.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"
#include "tools/ResourceManager.hxx"

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <gtest/gtest.h>
#include <gtest/internal/gtest-internal.h>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <memory>
#include <pqxx/result>
#include <pqxx/transaction>
#include <string>
#include <utility>

#include "tools/EnrolmentApiClient.hxx"

using namespace ::std::literals::chrono_literals;
using namespace ::std::string_view_literals;

class TestBlob
{
public:
    explicit TestBlob(const ::BlobType blobType)
        : mBlobType{blobType}
    {
    }

    void enrol(const ::std::string& blobName, const uint32_t generation)
    {
        mKeyId = (::boost::format{"KeyUpdateTest_%1%_%2%_%3%"} % static_cast<uint32_t>(mBlobType) % blobName %
                  ::std::chrono::duration_cast<::std::chrono::milliseconds>(
                      ::std::chrono::system_clock::now().time_since_epoch())
                      .count())
                     .str();

        createClient();

        mEnrolmentApiClient->storeBlob(
            mBlobType, mKeyId,
            ::ErpBlob{::ResourceManager::instance().getStringResource(::std::string{TEST_DATA_DIR} + "/enrolment/" +
                                                                      blobName + ".blob"),
                      generation},
            ::EnrolmentApiClient::ValidityPeriod{::std::chrono::system_clock::now(),
                                                 ::std::chrono::system_clock::now() + (24h * 365)});
    }

    void remove()
    {
        if (! mKeyId.empty())
        {
            createClient();

            ::std::cout << "Removing key with Id " << mKeyId << ::std::endl;
            mEnrolmentApiClient->deleteBlob(mBlobType, mKeyId);
            mKeyId = {};
        }
    }

private:
    static void createClient()
    {
        if (! mEnrolmentApiClient)
        {
            const auto& configuration = ::Configuration::instance();

            mEnrolmentApiClient = ::std::make_unique<::EnrolmentApiClient>(
                configuration.serverHost(),
                static_cast<uint16_t>(configuration.getIntValue(::ConfigurationKey::ENROLMENT_SERVER_PORT)),
                static_cast<uint16_t>(
                    configuration.getIntValue(::ConfigurationKey::HTTPCLIENT_CONNECT_TIMEOUT_SECONDS)),
                false);
        }
    }

    static ::std::unique_ptr<::EnrolmentApiClient> mEnrolmentApiClient;
    ::BlobType mBlobType;
    ::std::string mKeyId;
};

::std::unique_ptr<::EnrolmentApiClient> TestBlob::mEnrolmentApiClient = {};

struct BlobSet {
    ::TestBlob task = ::TestBlob{::BlobType::TaskKeyDerivation};
    ::TestBlob communication = ::TestBlob{::BlobType::CommunicationKeyDerivation};
    ::TestBlob audit = ::TestBlob{::BlobType::AuditLogKeyDerivation};

    void enrol(const ::std::string& blobName, const uint32_t generation)
    {
        ASSERT_NO_THROW(task.enrol(blobName, generation));
        ASSERT_NO_THROW(communication.enrol(blobName, generation));
        ASSERT_NO_THROW(audit.enrol(blobName, generation));
    }
};

class DerivationKeyUpdateTest : public ErpWorkflowTestTemplate<::testing::TestWithParam<bool>>
{
public:
    DerivationKeyUpdateTest()
    {
        if (! runsInCloudEnv() &&
            ::TestConfiguration::instance().getOptionalBoolValue(::TestConfigurationKey::TEST_USE_POSTGRES, false))
        {
            mConnection.connectIfNeeded();
            mTransaction = mConnection.createTransaction();
        }
    }

    static void forAllTaskTypes(::std::function<void(::model::PrescriptionType)>&& action)
    {
        for (auto taskType :
             {::model::PrescriptionType::apothekenpflichigeArzneimittel, ::model::PrescriptionType::direkteZuweisung})
        {
            ASSERT_NO_FATAL_FAILURE(action(taskType));
        }
    }

    ::std::optional<int32_t> getLatestBlobId(const ::BlobType blobType, const bool manualMode = false,
                                             bool skip = false)
    {
        const auto query =
            (::boost::format{"select blob_id from erp.blob where type=%1% order by blob_id desc limit 1;"} %
             static_cast<int32_t>(blobType))
                .str();

        return queryDatabase(query, manualMode, skip);
    }

    ::std::optional<int32_t> getTaskCountForBlobId(const int32_t blobId, const bool manualMode = false,
                                                   bool skip = false)
    {
        const auto query =
            (::boost::format{"select count(task_key_blob_id) as tasks from erp.task where task_key_blob_id = %1%;"} %
             blobId)
                .str();

        return queryDatabase(query, manualMode, skip);
    }

protected:
    ::std::optional<int32_t> queryDatabase(const ::std::string& query, const bool manualMode, bool skip)
    {
        if (manualMode)
        {
            if (! skip)
            {
                ::std::string input;
                ::std::cout << "\nPlease run the following query on your database\n"
                            << query << "\nand enter the result (leave empty to skip this check): ";
                ::std::getline(::std::cin, input);

                resetClient();

                try
                {
                    return ::boost::lexical_cast<int32_t>(input);
                }
                catch (...)
                {
                }
            }
        }
        else
        {
            if (mTransaction)
            {
                const auto result = mTransaction->exec(query);
                if (result.size() == 1u)
                {
                    return result.begin().at(0).as<int32_t>();
                }
            }
        }

        return {};
    }

    ::PostgresConnection mConnection{PostgresBackend::defaultConnectString()};
    ::std::unique_ptr<::pqxx::work> mTransaction;
};

class TestTask
{
public:
    TestTask(::DerivationKeyUpdateTest& test, const ::model::PrescriptionType prescriptionType)
        : mTest{test}
        , mPrescriptionType{prescriptionType}
    {
        mTest.generateNewRandomKVNR(mKvnr);
    }

    void create()
    {
        ::std::cout << "Creating Task for KVNr " << mKvnr << ::std::endl;
        ASSERT_NO_FATAL_FAILURE(mTest.checkTaskCreate(mPrescriptionId, mAccessCode, mPrescriptionType));
    }

    void activate()
    {
        ::std::cout << "Activating Task for KVNr " << mKvnr << ::std::endl;
        ASSERT_NO_FATAL_FAILURE(
            mTest.checkTaskActivate(mQesBundle, mCommunications, *mPrescriptionId, mKvnr, mAccessCode));
    }

    void accept()
    {
        ::std::cout << "Accepting Task for KVNr " << mKvnr << ::std::endl;
        ASSERT_NO_FATAL_FAILURE(
            mTest.checkTaskAccept(mSecret, mLastModifiedDate, *mPrescriptionId, mKvnr, mAccessCode, mQesBundle));
    }

    void reject()
    {
        ::std::cout << "Rejecting Task for KVNr " << mKvnr << ::std::endl;
        ASSERT_NO_FATAL_FAILURE(mTest.checkTaskReject(*mPrescriptionId, mKvnr, mAccessCode, mSecret));
    }

    void close()
    {
        ::std::cout << "Closing Task for KVNr " << mKvnr << ::std::endl;
        ASSERT_NO_FATAL_FAILURE(
            mTest.checkTaskClose(*mPrescriptionId, mKvnr, mSecret, *mLastModifiedDate, mCommunications));

        ::std::optional<::model::Bundle> taskBundle;
        ASSERT_NO_FATAL_FAILURE(taskBundle = mTest.taskGet(mKvnr));
        ASSERT_TRUE(taskBundle);
        ::std::optional<::model::Task> task;
        for (auto&& resource : taskBundle->getResourcesByType<::model::Task>("Task"))
        {
            ASSERT_TRUE(resource.kvnr().has_value());
            ASSERT_EQ(resource.kvnr().value(), mKvnr);
            ASSERT_NO_FATAL_FAILURE(mTest.checkTask(resource.jsonDocument()););
            if (resource.prescriptionId().toDatabaseId() == mPrescriptionId->toDatabaseId())
            {
                task = std::move(resource);
                break;
            }
        }
        ASSERT_TRUE(task);
        ASSERT_ANY_THROW((void) task->accessCode());
        ASSERT_FALSE(task->secret().has_value());

        ASSERT_NO_FATAL_FAILURE(mTest.checkAuditEventsFrom(mKvnr));
    }

    void abort()
    {
        ::std::cout << "Aborting Task for KVNr " << mKvnr << ::std::endl;
        ASSERT_NO_FATAL_FAILURE(
            mTest.taskAbort(*mPrescriptionId, JwtBuilder::testBuilder().makeJwtArzt(), mAccessCode, {}));
    }

private:
    ::DerivationKeyUpdateTest& mTest;
    ::model::PrescriptionType mPrescriptionType = ::model::PrescriptionType::apothekenpflichigeArzneimittel;
    ::std::string mKvnr = {};
    ::std::optional<::model::PrescriptionId> mPrescriptionId = {};
    ::std::string mAccessCode = {};
    ::std::string mQesBundle = {};
    ::std::vector<::model::Communication> mCommunications = {};
    ::std::string mSecret;
    ::std::optional<model::Timestamp> mLastModifiedDate;
};

TEST_F(DerivationKeyUpdateTest, Delete)
{
    if (runsInCloudEnv())
    {
        GTEST_SKIP();
    }

    const auto initialLastTaskBlobId = getLatestBlobId(::BlobType::TaskKeyDerivation);
    const auto initialLastCommunicationBlobId = getLatestBlobId(::BlobType::CommunicationKeyDerivation);
    const auto initialLastAuditBlobId = getLatestBlobId(::BlobType::AuditLogKeyDerivation);

    BlobSet blobs;
    ASSERT_NO_FATAL_FAILURE(blobs.enrol("DerivationKey_Gen0x43", 0x43));

    ASSERT_GE(getLatestBlobId(::BlobType::TaskKeyDerivation).value_or(3), initialLastTaskBlobId.value_or(0) + 3)
        << "No new key found.";
    ASSERT_GE(getLatestBlobId(::BlobType::CommunicationKeyDerivation).value_or(3),
              initialLastCommunicationBlobId.value_or(0) + 3)
        << "No new key found.";
    ASSERT_GE(getLatestBlobId(::BlobType::AuditLogKeyDerivation).value_or(3), initialLastAuditBlobId.value_or(0) + 3)
        << "No new key found.";

    EXPECT_NO_THROW(blobs.task.remove());
    EXPECT_NO_THROW(blobs.communication.remove());
    EXPECT_NO_THROW(blobs.audit.remove());

    ASSERT_NO_FATAL_FAILURE(forAllTaskTypes([this](::model::PrescriptionType taskType) {
        TestTask task(*this, taskType);
        ASSERT_NO_FATAL_FAILURE(task.create());
        ASSERT_NO_FATAL_FAILURE(task.activate());
        ASSERT_NO_FATAL_FAILURE(task.accept());
        ASSERT_NO_FATAL_FAILURE(task.reject());
        ASSERT_NO_FATAL_FAILURE(task.accept());
        ASSERT_NO_FATAL_FAILURE(task.close());
    }));

    EXPECT_EQ(getLatestBlobId(::BlobType::TaskKeyDerivation).value_or(0), initialLastTaskBlobId.value_or(0))
        << "Key was not deleted.";
    EXPECT_EQ(getLatestBlobId(::BlobType::CommunicationKeyDerivation).value_or(0),
              initialLastCommunicationBlobId.value_or(0))
        << "Key was not deleted.";
    EXPECT_EQ(getLatestBlobId(::BlobType::AuditLogKeyDerivation).value_or(0), initialLastAuditBlobId.value_or(0))
        << "Key was not deleted.";
}

TEST_F(DerivationKeyUpdateTest, NoDeleteInUse)
{
    if (runsInCloudEnv() ||
        ! ::TestConfiguration::instance().getOptionalBoolValue(::TestConfigurationKey::TEST_USE_POSTGRES, false))
    {
        // This test requires access to the enrolemnt API and checks a database constraint and thus only works on a local, non-mocked database.
        GTEST_SKIP();
    }

    const auto initialLastTaskBlobId = getLatestBlobId(::BlobType::TaskKeyDerivation);
    const auto initialLastCommunicationBlobId = getLatestBlobId(::BlobType::CommunicationKeyDerivation);
    const auto initialLastAuditBlobId = getLatestBlobId(::BlobType::AuditLogKeyDerivation);

    BlobSet blobs;
    ASSERT_NO_FATAL_FAILURE(blobs.enrol("DerivationKey_Gen0x43", 0x43));

    ASSERT_GE(getLatestBlobId(::BlobType::TaskKeyDerivation).value_or(3), initialLastTaskBlobId.value_or(0) + 3)
        << "No new key found.";
    ASSERT_GE(getLatestBlobId(::BlobType::CommunicationKeyDerivation).value_or(3),
              initialLastCommunicationBlobId.value_or(0) + 3)
        << "No new key found.";
    ASSERT_GE(getLatestBlobId(::BlobType::AuditLogKeyDerivation).value_or(3), initialLastAuditBlobId.value_or(0) + 3)
        << "No new key found.";

    ASSERT_NO_FATAL_FAILURE(forAllTaskTypes([this](::model::PrescriptionType taskType) {
        TestTask task(*this, taskType);
        ASSERT_NO_FATAL_FAILURE(task.create());
        ASSERT_NO_FATAL_FAILURE(task.activate());
    }));

    EXPECT_ANY_THROW(blobs.task.remove());
    EXPECT_ANY_THROW(blobs.communication.remove());
    EXPECT_ANY_THROW(blobs.audit.remove());

    EXPECT_GE(getLatestBlobId(::BlobType::TaskKeyDerivation).value_or(3), initialLastTaskBlobId.value_or(0) + 3)
        << "Key was wrongfully deleted.";
    EXPECT_GE(getLatestBlobId(::BlobType::CommunicationKeyDerivation).value_or(3),
              initialLastCommunicationBlobId.value_or(0) + 3)
        << "Key was wrongfully deleted.";
    EXPECT_GE(getLatestBlobId(::BlobType::AuditLogKeyDerivation).value_or(3), initialLastAuditBlobId.value_or(0) + 3)
        << "Key was wrongfully deleted.";
}

TEST_F(DerivationKeyUpdateTest, DeleteOldUnused)
{
    if (runsInCloudEnv() ||
        ! ::TestConfiguration::instance().getOptionalBoolValue(::TestConfigurationKey::TEST_USE_POSTGRES, false))
    {
        // This test requires access to the enrolemnt API and checks a database constraint and thus only works on a local, non-mocked database.
        GTEST_SKIP();
    }

    const auto initialLastTaskBlobId = getLatestBlobId(::BlobType::TaskKeyDerivation);
    const auto initialLastCommunicationBlobId = getLatestBlobId(::BlobType::CommunicationKeyDerivation);
    const auto initialLastAuditBlobId = getLatestBlobId(::BlobType::AuditLogKeyDerivation);

    BlobSet blobsUnused;
    ASSERT_NO_FATAL_FAILURE(blobsUnused.enrol("DerivationKey_Gen0x43", 0x43));

    ASSERT_GE(getLatestBlobId(::BlobType::TaskKeyDerivation).value_or(3), initialLastTaskBlobId.value_or(0) + 3)
        << "No new key found.";
    ASSERT_GE(getLatestBlobId(::BlobType::CommunicationKeyDerivation).value_or(3),
              initialLastCommunicationBlobId.value_or(0) + 3)
        << "No new key found.";
    ASSERT_GE(getLatestBlobId(::BlobType::AuditLogKeyDerivation).value_or(3), initialLastAuditBlobId.value_or(0) + 3)
        << "No new key found.";

    BlobSet blobs;
    ASSERT_NO_FATAL_FAILURE(blobs.enrol("DerivationKey_Gen0x44", 0x44));

    const auto intermediateLastTaskBlobId = getLatestBlobId(::BlobType::TaskKeyDerivation);
    ASSERT_GE(intermediateLastTaskBlobId.value_or(6), initialLastTaskBlobId.value_or(0) + 6) << "No new key found.";
    const auto intermediateLastCommunicationBlobId = getLatestBlobId(::BlobType::CommunicationKeyDerivation);
    ASSERT_GE(intermediateLastCommunicationBlobId.value_or(6), initialLastCommunicationBlobId.value_or(0) + 6)
        << "No new key found.";
    const auto intermediateLastAuditBlobId = getLatestBlobId(::BlobType::AuditLogKeyDerivation);
    ASSERT_GE(intermediateLastAuditBlobId.value_or(6), initialLastAuditBlobId.value_or(0) + 6) << "No new key found.";

    ASSERT_NO_FATAL_FAILURE(forAllTaskTypes([this](::model::PrescriptionType taskType) {
        TestTask task(*this, taskType);
        ASSERT_NO_FATAL_FAILURE(task.create());
        ASSERT_NO_FATAL_FAILURE(task.activate());
    }));

    // blobsUnused were added, but immediately superseded by blobs and thus may be deleted
    EXPECT_NO_THROW(blobsUnused.task.remove());
    EXPECT_NO_THROW(blobsUnused.communication.remove());
    EXPECT_NO_THROW(blobsUnused.audit.remove());
    // blobs were used for tasks and may not be deleted
    EXPECT_ANY_THROW(blobs.task.remove());
    EXPECT_ANY_THROW(blobs.communication.remove());
    EXPECT_ANY_THROW(blobs.audit.remove());

    EXPECT_EQ(getLatestBlobId(::BlobType::TaskKeyDerivation).value_or(6), intermediateLastTaskBlobId.value_or(6))
        << "Key was wrongfully deleted.";
    EXPECT_EQ(getLatestBlobId(::BlobType::CommunicationKeyDerivation).value_or(6),
              intermediateLastCommunicationBlobId.value_or(6))
        << "Key was wrongfully deleted.";
    EXPECT_EQ(getLatestBlobId(::BlobType::AuditLogKeyDerivation).value_or(6), intermediateLastAuditBlobId.value_or(6))
        << "Key was wrongfully deleted.";
}

TEST_F(DerivationKeyUpdateTest, DeleteMultiple)
{
    if (runsInCloudEnv() ||
        ! ::TestConfiguration::instance().getOptionalBoolValue(::TestConfigurationKey::TEST_USE_POSTGRES, false))
    {
        // This test requires access to the enrolemnt API and checks a database constraint and thus only works on a local, non-mocked database.
        GTEST_SKIP();
    }

    const auto initialLastTaskBlobId = getLatestBlobId(::BlobType::TaskKeyDerivation);
    const auto initialLastCommunicationBlobId = getLatestBlobId(::BlobType::CommunicationKeyDerivation);
    const auto initialLastAuditBlobId = getLatestBlobId(::BlobType::AuditLogKeyDerivation);

    BlobSet blobsUnused;
    ASSERT_NO_FATAL_FAILURE(blobsUnused.enrol("DerivationKey_Gen0x43", 0x43));
    BlobSet blobs;
    ASSERT_NO_FATAL_FAILURE(blobs.enrol("DerivationKey_Gen0x44", 0x44));

    const auto intermediateLastTaskBlobId = getLatestBlobId(::BlobType::TaskKeyDerivation);
    ASSERT_GE(intermediateLastTaskBlobId.value_or(6), initialLastTaskBlobId.value_or(0) + 6) << "No new key found.";
    const auto intermediateLastCommunicationBlobId = getLatestBlobId(::BlobType::CommunicationKeyDerivation);
    ASSERT_GE(intermediateLastCommunicationBlobId.value_or(6), initialLastCommunicationBlobId.value_or(0) + 6)
        << "No new key found.";
    const auto intermediateLastAuditBlobId = getLatestBlobId(::BlobType::AuditLogKeyDerivation);
    ASSERT_GE(intermediateLastAuditBlobId.value_or(6), initialLastAuditBlobId.value_or(0) + 6) << "No new key found.";

    BlobSet blobsImmediatelyDeleted;
    ASSERT_NO_FATAL_FAILURE(blobsImmediatelyDeleted.enrol("DerivationKey_Gen0x45", 0x45));

    ASSERT_GE(getLatestBlobId(::BlobType::TaskKeyDerivation).value_or(9), intermediateLastTaskBlobId.value_or(6) + 3)
        << "No new key found.";
    ASSERT_GE(getLatestBlobId(::BlobType::CommunicationKeyDerivation).value_or(9),
              intermediateLastCommunicationBlobId.value_or(6) + 3)
        << "No new key found.";
    ASSERT_GE(getLatestBlobId(::BlobType::AuditLogKeyDerivation).value_or(9),
              intermediateLastAuditBlobId.value_or(6) + 3)
        << "No new key found.";

    EXPECT_NO_THROW(blobsImmediatelyDeleted.task.remove());
    EXPECT_NO_THROW(blobsImmediatelyDeleted.communication.remove());
    EXPECT_NO_THROW(blobsImmediatelyDeleted.audit.remove());

    ASSERT_EQ(getLatestBlobId(::BlobType::TaskKeyDerivation).value_or(6), intermediateLastTaskBlobId.value_or(6));
    ASSERT_EQ(getLatestBlobId(::BlobType::CommunicationKeyDerivation).value_or(6),
              intermediateLastCommunicationBlobId.value_or(6));
    ASSERT_EQ(getLatestBlobId(::BlobType::AuditLogKeyDerivation).value_or(6), intermediateLastAuditBlobId.value_or(6));

    ASSERT_NO_FATAL_FAILURE(forAllTaskTypes([this](::model::PrescriptionType taskType) {
        TestTask task(*this, taskType);
        ASSERT_NO_FATAL_FAILURE(task.create());
        ASSERT_NO_FATAL_FAILURE(task.activate());
        ASSERT_NO_FATAL_FAILURE(task.accept());
        ASSERT_NO_FATAL_FAILURE(task.reject());
        ASSERT_NO_FATAL_FAILURE(task.accept());
    }));

    // blobsUnused were added, but immediately superseded by blobs and thus may be deleted
    EXPECT_NO_THROW(blobsUnused.task.remove());
    EXPECT_NO_THROW(blobsUnused.communication.remove());
    EXPECT_NO_THROW(blobsUnused.audit.remove());
    // blobs were used for tasks and may not be deleted
    EXPECT_ANY_THROW(blobs.task.remove());
    EXPECT_ANY_THROW(blobs.communication.remove());
    EXPECT_ANY_THROW(blobs.audit.remove());

    EXPECT_EQ(getLatestBlobId(::BlobType::TaskKeyDerivation).value_or(6), intermediateLastTaskBlobId.value_or(6))
        << "Key was wrongfully deleted.";
    EXPECT_EQ(getLatestBlobId(::BlobType::CommunicationKeyDerivation).value_or(6),
              intermediateLastCommunicationBlobId.value_or(6))
        << "Key was wrongfully deleted.";
    EXPECT_EQ(getLatestBlobId(::BlobType::AuditLogKeyDerivation).value_or(6), intermediateLastAuditBlobId.value_or(6))
        << "Key was wrongfully deleted.";
}

TEST_P(DerivationKeyUpdateTest, TaskWorkflows)
{
    const auto isManual = GetParam();

    if (! isManual && runsInCloudEnv())
    {
        // Automatic test requires enrolment API access.
        GTEST_SKIP();
    }

    const auto initialLastTaskBlobId = getLatestBlobId(::BlobType::TaskKeyDerivation, isManual);
    const auto initialLastCommunicationBlobId =
        getLatestBlobId(::BlobType::CommunicationKeyDerivation, isManual, ! initialLastTaskBlobId.has_value());
    const auto initialLastAuditBlobId =
        getLatestBlobId(::BlobType::AuditLogKeyDerivation, isManual, ! initialLastCommunicationBlobId.has_value());

    BlobSet blobsA;
    BlobSet blobsB;

    if (isManual)
    {
        ::std::cout << R"(
Please add 3 new sets of derivation keys (a, b, c), then remove the last set (c) and press any key...

A set includes one of each
 - task derivation key
 - communication derivation key
 - audit derivation key
)";
        ::std::cin.get();
        resetClient();
    }
    else
    {
        ::std::cout << "Updating blobs" << ::std::endl;

        ASSERT_NO_FATAL_FAILURE(blobsA.enrol("DerivationKey_Gen0x43", 0x43));
        ASSERT_NO_FATAL_FAILURE(blobsB.enrol("DerivationKey_Gen0x44", 0x44));

        BlobSet blobsC;
        ASSERT_NO_FATAL_FAILURE(blobsC.enrol("DerivationKey_Gen0x45", 0x45));

        EXPECT_NO_THROW(blobsC.task.remove());
        EXPECT_NO_THROW(blobsC.communication.remove());
        EXPECT_NO_THROW(blobsC.audit.remove());
    }

    const auto intermediateLastTaskBlobId =
        getLatestBlobId(::BlobType::TaskKeyDerivation, isManual, ! initialLastAuditBlobId.has_value());
    ASSERT_GE(intermediateLastTaskBlobId.value_or(6), initialLastTaskBlobId.value_or(0) + 6) << "No new key found.";
    const auto intermediateLastCommunicationBlobId =
        getLatestBlobId(::BlobType::CommunicationKeyDerivation, isManual, ! intermediateLastTaskBlobId.has_value());
    ASSERT_GE(intermediateLastCommunicationBlobId.value_or(6), initialLastCommunicationBlobId.value_or(0) + 6)
        << "No new key found.";
    const auto intermediateLastAuditBlobId =
        getLatestBlobId(::BlobType::AuditLogKeyDerivation, isManual, ! intermediateLastCommunicationBlobId.has_value());
    ASSERT_GE(intermediateLastAuditBlobId.value_or(6), initialLastAuditBlobId.value_or(0) + 6) << "No new key found.";

    // Tasks with full workflow
    ::std::array fullWorkflowActions = {&TestTask::create, &TestTask::activate, &TestTask::accept,
                                        &TestTask::reject, &TestTask::accept,   &TestTask::close};

    ::std::vector<::std::pair<TestTask, ::std::size_t>> fullWorkflowTasks;
    fullWorkflowTasks.reserve(3u * fullWorkflowActions.size());

    for (auto index = 0u; index <= fullWorkflowActions.size(); ++index)
    {
        forAllTaskTypes([this, &fullWorkflowTasks, index](::model::PrescriptionType taskType) {
            fullWorkflowTasks.emplace_back(::std::make_pair(TestTask{*this, taskType}, index));
        });
    }

    ASSERT_NO_FATAL_FAILURE(for (auto& task
                                 : fullWorkflowTasks) {
        ASSERT_NO_FATAL_FAILURE(::std::for_each(fullWorkflowActions.begin(), fullWorkflowActions.begin() + task.second,
                                                [&task](auto& action) {
                                                    (task.first.*action)();
                                                }));
    });

    // Tasks to abort
    ::std::vector<TestTask> abortTasks;
    forAllTaskTypes([this, &abortTasks](::model::PrescriptionType taskType) {
        abortTasks.emplace_back(TestTask{*this, taskType});
    });

    ASSERT_NO_FATAL_FAILURE(::std::for_each(abortTasks.begin(), abortTasks.end(), [](auto& task) {
        ASSERT_NO_FATAL_FAILURE(task.create());
        ASSERT_NO_FATAL_FAILURE(task.activate());
    }));

    if (intermediateLastTaskBlobId.has_value())
    {
        ASSERT_GT(getTaskCountForBlobId(intermediateLastTaskBlobId.value(), isManual).value_or(1), 0)
            << "No new tasks found.";
    }

    // Update blobs
    if (isManual)
    {
        ::std::cout << "\nPlease add a new set of derivation keys and press any key...";
        ::std::cin.get();
        resetClient();
    }
    else
    {
        ::std::cout << "Updating blobs" << ::std::endl;

        BlobSet blobs;
        ASSERT_NO_FATAL_FAILURE(blobs.enrol("DerivationKey_Gen0x46", 0x46));
    }

    const auto newLastTaskBlobId =
        getLatestBlobId(::BlobType::TaskKeyDerivation, isManual, ! intermediateLastAuditBlobId.has_value());
    ASSERT_GE(newLastTaskBlobId.value_or(12), intermediateLastTaskBlobId.value_or(9) + 3) << "No new key found.";
    const auto newLastCommunicationBlobId =
        getLatestBlobId(::BlobType::CommunicationKeyDerivation, isManual, ! newLastTaskBlobId.has_value());
    ASSERT_GE(newLastCommunicationBlobId.value_or(12), intermediateLastCommunicationBlobId.value_or(9) + 3)
        << "No new key found.";
    const auto newLastAuditBlobId =
        getLatestBlobId(::BlobType::AuditLogKeyDerivation, isManual, ! newLastCommunicationBlobId.has_value());
    ASSERT_GE(newLastAuditBlobId.value_or(12), intermediateLastAuditBlobId.value_or(9) + 3) << "No new key found.";

    // Finish tasks
    ASSERT_NO_FATAL_FAILURE(
        ::std::for_each(fullWorkflowTasks.begin(), fullWorkflowTasks.end(), [&fullWorkflowActions](auto& task) {
            ASSERT_NO_FATAL_FAILURE(::std::for_each(fullWorkflowActions.begin() + task.second,
                                                    fullWorkflowActions.end(), [&task](auto& action) {
                                                        (task.first.*action)();
                                                    }));
        }));

    ASSERT_NO_FATAL_FAILURE(::std::for_each(abortTasks.begin(), abortTasks.end(), [](auto& task) {
        ASSERT_NO_FATAL_FAILURE(task.abort());
    }));

    if (newLastTaskBlobId.has_value())
    {
        ASSERT_GT(getTaskCountForBlobId(newLastTaskBlobId.value(), isManual).value_or(1), 0) << "No new tasks found.";
    }

    if (isManual)
    {
        ::std::cout << R"(
Please remove the following derivation key sets:
- (a) this operation should succeed
- (b) this operation should fail
and press any key...)";
        ::std::cin.get();
    }
    else
    {
        EXPECT_NO_THROW(blobsA.task.remove());
        EXPECT_NO_THROW(blobsA.communication.remove());
        EXPECT_NO_THROW(blobsA.audit.remove());

        EXPECT_ANY_THROW(blobsB.task.remove());
        EXPECT_ANY_THROW(blobsB.communication.remove());
        EXPECT_ANY_THROW(blobsB.audit.remove());
    }

    EXPECT_EQ(
        getLatestBlobId(::BlobType::TaskKeyDerivation, isManual, ! newLastCommunicationBlobId.has_value()).value_or(12),
        newLastTaskBlobId.value_or(12))
        << "Key was wrongfully deleted.";
    EXPECT_EQ(
        getLatestBlobId(::BlobType::CommunicationKeyDerivation, isManual, ! newLastCommunicationBlobId.has_value())
            .value_or(12),
        newLastCommunicationBlobId.value_or(12))
        << "Key was wrongfully deleted.";
    EXPECT_EQ(getLatestBlobId(::BlobType::AuditLogKeyDerivation, isManual, ! newLastCommunicationBlobId.has_value())
                  .value_or(12),
              newLastAuditBlobId.value_or(12))
        << "Key was wrongfully deleted.";
}

::std::string instanceName(const ::testing::TestParamInfo<DerivationKeyUpdateTest::ParamType>& info)
{
    if (info.param)
    {
        return "Manual";
    }
    else
    {
        return "Automatic";
    }
}

INSTANTIATE_TEST_SUITE_P(KeyUpdate, DerivationKeyUpdateTest, ::testing::Values(false), &instanceName);
// The manual version is disabled by default because it requires user interaction.
INSTANTIATE_TEST_SUITE_P(DISABLED_KeyUpdateManual, DerivationKeyUpdateTest, ::testing::Values(true), &instanceName);
