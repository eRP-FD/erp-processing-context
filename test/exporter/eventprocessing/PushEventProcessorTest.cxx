/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/eventprocessing/push/PushEventProcessor.hxx"
#include "erp/database/push/PushExporterDatabaseException.hxx"
#include "exporter/client/push/PushGatewayClient.hxx"
#include "shared/model/ModelException.hxx"
#include "exporter/model/push/PushNotificationContext.hxx"
#include "exporter/pc/MedicationExporterFactories.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "shared/audit/AuditDataCollector.hxx"
#include "shared/util/Configuration.hxx"
#include "test/exporter/mock/MainDatabaseFrontendMock.hxx"
#include "test/exporter/mock/MedicationExporterDatabaseFrontendMock.hxx"
#include "test/exporter/mock/PushNotificationClientMock.hxx"
#include "test/exporter/util/MedicationExporterStaticData.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"

#include <gtest/gtest.h>

// Tests with Database mocks and client mock
class MainDatabaseFrontendMock;
class PushEventProcessorTest : public testing::Test
{
public:
    PushEventProcessorTest()
    {
        auto fact = MedicationExporterStaticData::makeMockMedicationExporterFactories(true);
        fact.exporterDatabaseFactory =
            [this](KeyDerivation&,
                   TransactionMode) mutable -> std::unique_ptr<MedicationExporterDatabaseFrontendInterface> {
            return std::make_unique<MedicationExporterDatabaseFrontendProxy>(&exporterDbMock);
        };
        fact.erpDatabaseFactory =
            [this](HsmPool&, KeyDerivation&) mutable -> std::unique_ptr<exporter::MainDatabaseFrontendInterface> {
            return std::make_unique<MainDatabaseFrontendMockProxy>(&mainDbMock);
        };

        ioThreadPool.setUp(1, "medication-exporter-io");
        auto& ioContext = ioThreadPool.ioContext();
        serviceContext = std::make_shared<MedicationExporterServiceContext>(ioContext, Configuration::instance(), fact);
        client = std::make_unique<PushNotificationClientMock>();
    }
    model::PushNotificationContext makePushNotificationContext(date::year_month_day ymd, int64_t eventId,
                                                               int retryCount = 0,
                                                               model::Timestamp created = model::Timestamp::now())
    {
        auto prescriptionId =
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1);
        const model::PushNotification pushNotification("erp.task.activate", prescriptionId.toString(), "TaskId");
        const model::PusherData pusherData("https://push-gateway.location.here/push/v1/", std::nullopt);
        const model::PushNotificationDevice pushNotificationDevice(
            "app-id", "Xp/MzCt8/9DcSNE9cuiaoT5Ac55job3TdLSSmtmYl4A=",
            std::chrono::duration_cast<std::chrono::seconds>(
                model::Timestamp::now().toChronoTimePoint().time_since_epoch()),
            pusherData, "displayName");
        const model::EncryptionKey encryptionKey{
            SafeString("shared-secret1111111111111111111"), SafeString("message-key111111111111111111111"),
            fmt::format("{}-{}", static_cast<int>(ymd.year()), static_cast<unsigned>(ymd.month()))};
        db_model::HashedKvnr hashedKvnr;
        hashedKvnr.append("c8c5d1e7de0a204e56970d3c1c32c3cccf9c71e08425fa114b507c381f48cfff");
        const model::PushNotificationContext context(pushNotification, pushNotificationDevice, encryptionKey,
                                                     "key-identifier", created, db_model::Blob{}, BlobId{}, eventId,
                                                     hashedKvnr, retryCount, "auditId");
        return context;
    }
    testing::NiceMock<MedicationExporterDatabaseFrontendMock> exporterDbMock;
    testing::NiceMock<MainDatabaseFrontendMock> mainDbMock;
    ThreadPool ioThreadPool;
    std::shared_ptr<MedicationExporterServiceContext> serviceContext;
    std::unique_ptr<PushGatewayClient> client;
};

TEST_F(PushEventProcessorTest, fetchNextEvent_NoEvent)
{
    PushEventProcessor processor{serviceContext, client.get()};

    ON_CALL(exporterDbMock, processNextPushNotification()).WillByDefault(testing::Return(std::nullopt));
    std::vector<model::PushNotificationContext> events;
    ASSERT_NO_THROW(events = processor.fetchNextEvent());
    ASSERT_TRUE(events.empty());
}

TEST_F(PushEventProcessorTest, fetchNextEvent_NoRegistration)
{
    PushEventProcessor processor{serviceContext, client.get()};
    // event, but no registration
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1);
    const model::Kvnr kvnr("X12345678");
    db_model::HashedKvnr hashedKvnr;
    hashedKvnr.append("c8c5d1e7de0a204e56970d3c1c32c3cccf9c71e08425fa114b507c381f48cfff");
    const model::PushNotificationEvent event{
        123, "erp.task.activate", prescriptionId.toString(), "TaskId", hashedKvnr, 0, model::Timestamp::now(), "auditId"};
    ON_CALL(exporterDbMock, processNextPushNotification()).WillByDefault(testing::Return(event));
    ON_CALL(mainDbMock, retrievePushNotificationEventData(testing::_))
        .WillByDefault(testing::Return(std::vector<model::PushNotificationContext>{}));
    EXPECT_CALL(exporterDbMock, deletePushNotification(testing::_, testing::Eq(123))).Times(1);
    std::vector<model::PushNotificationContext> events;
    ASSERT_NO_THROW(events = processor.fetchNextEvent());
    ASSERT_TRUE(events.empty());
}

TEST_F(PushEventProcessorTest, fetchNextEvent_Good)
{
    PushEventProcessor processor{serviceContext, client.get()};
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1);
    const model::Kvnr kvnr("X12345678");
    db_model::HashedKvnr hashedKvnr;
    hashedKvnr.append("c8c5d1e7de0a204e56970d3c1c32c3cccf9c71e08425fa114b507c381f48cfff");
    const model::PushNotificationEvent event{
        0, "erp.task.activate", prescriptionId.toString(), "TaskId", hashedKvnr, 0, model::Timestamp::now(), "auditId"};
    ON_CALL(exporterDbMock, processNextPushNotification()).WillByDefault(testing::Return(event));

    const date::year_month_day ymd{model::Timestamp::now().localDay()};
    ON_CALL(mainDbMock, retrievePushNotificationEventData(testing::_))
        .WillByDefault(
            testing::Return(std::vector<model::PushNotificationContext>{makePushNotificationContext(ymd, 0)}));

    std::vector<model::PushNotificationContext> events;
    ASSERT_NO_THROW(events = processor.fetchNextEvent());
    ASSERT_FALSE(events.empty());
}

TEST_F(PushEventProcessorTest, fetchNextEvent_EncryptionNeedsUpdate)
{
    PushEventProcessor processor{serviceContext, client.get()};
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1);
    const model::Kvnr kvnr("X12345678");
    db_model::HashedKvnr hashedKvnr;
    hashedKvnr.append("c8c5d1e7de0a204e56970d3c1c32c3cccf9c71e08425fa114b507c381f48cfff");
    const model::PushNotificationEvent event{
        0, "erp.task.activate", prescriptionId.toString(), "TaskId", hashedKvnr, 0, model::Timestamp::now(), "auditId"};
    ON_CALL(exporterDbMock, processNextPushNotification()).WillByDefault(testing::Return(event));


    const date::year_month_day ymd{model::Timestamp::now().localDay()};
    const auto ymdOld = ymd - date::months{1};

    ON_CALL(mainDbMock, retrievePushNotificationEventData(testing::_))
        .WillByDefault(
            testing::Return(std::vector<model::PushNotificationContext>{makePushNotificationContext(ymdOld, 0)}));

    EXPECT_CALL(mainDbMock, updateEncryptionKey).Times(1);

    std::vector<model::PushNotificationContext> events;
    ASSERT_NO_THROW(events = processor.fetchNextEvent());
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.front().encryptionKey().monthKeyCreated(), date::year_month(ymd.year(), ymd.month()));
}

TEST_F(PushEventProcessorTest, processEvent_Good)
{
    PushEventProcessor processor{serviceContext, client.get()};
    PushEventProcessor::ResultType result{PushEventProcessor::ResultType::FailureRetry};
    const date::year_month_day ymd{model::Timestamp::now().localDay()};

    auto* clientMock = dynamic_cast<PushNotificationClientMock*>(client.get());
    ASSERT_TRUE(clientMock);
    EXPECT_CALL(*clientMock, sendPushNotification(testing::_, testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(ClientResponse{Header{HttpStatus::OK}, ""}));

    ASSERT_NO_THROW(result = processor.processEvent(makePushNotificationContext(ymd, 0)));
    EXPECT_EQ(result, PushEventProcessor::ResultType::Success);
}

TEST_F(PushEventProcessorTest, process_Good)
{
    PushEventProcessor processor{serviceContext, client.get()};
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1);
    const model::Kvnr kvnr("X12345678");
    db_model::HashedKvnr hashedKvnr;
    hashedKvnr.append("c8c5d1e7de0a204e56970d3c1c32c3cccf9c71e08425fa114b507c381f48cfff");
    const model::PushNotificationEvent event{
        123,      "erp.task.activate", prescriptionId.toString(), "TaskId", hashedKvnr, 0, model::Timestamp::now(),
        "auditId"};
    ON_CALL(exporterDbMock, processNextPushNotification()).WillByDefault(testing::Return(event));

    const date::year_month_day ymd{model::Timestamp::now().localDay()};
    ON_CALL(mainDbMock, retrievePushNotificationEventData(testing::_))
        .WillByDefault(
            testing::Return(std::vector<model::PushNotificationContext>{makePushNotificationContext(ymd, 123)}));

    auto* clientMock = dynamic_cast<PushNotificationClientMock*>(client.get());
    ASSERT_TRUE(clientMock);
    EXPECT_CALL(*clientMock, sendPushNotification(testing::_, testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(ClientResponse{Header{HttpStatus::OK}, ""}));
    EXPECT_CALL(exporterDbMock, deletePushNotification(testing::_, testing::Eq(123))).Times(1);

    PushEventProcessor::ResultType result{PushEventProcessor::ResultType::FailureRetry};
    ASSERT_NO_THROW(result = processor.process());
    EXPECT_EQ(result, PushEventProcessor::ResultType::Success);
}

TEST_F(PushEventProcessorTest, process_Idle)
{
    PushEventProcessor processor{serviceContext, client.get()};
    ON_CALL(exporterDbMock, processNextPushNotification()).WillByDefault(testing::Return(std::nullopt));
    PushEventProcessor::ResultType result{PushEventProcessor::ResultType::FailureRetry};
    ASSERT_NO_THROW(result = processor.process());
    EXPECT_EQ(result, PushEventProcessor::ResultType::Idle);
}

TEST_F(PushEventProcessorTest, process_ThrowInDelete)
{
    PushEventProcessor processor{serviceContext, client.get()};
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1);
    const model::Kvnr kvnr("X12345678");
    db_model::HashedKvnr hashedKvnr;
    hashedKvnr.append("c8c5d1e7de0a204e56970d3c1c32c3cccf9c71e08425fa114b507c381f48cfff");
    const model::PushNotificationEvent event{
        123,      "erp.task.activate", prescriptionId.toString(), "TaskId", hashedKvnr, 0, model::Timestamp::now(),
        "auditId"};
    ON_CALL(exporterDbMock, processNextPushNotification()).WillByDefault(testing::Return(event));

    const date::year_month_day ymd{model::Timestamp::now().localDay()};
    ON_CALL(mainDbMock, retrievePushNotificationEventData(testing::_))
        .WillByDefault(
            testing::Return(std::vector<model::PushNotificationContext>{makePushNotificationContext(ymd, 123)}));

    auto* clientMock = dynamic_cast<PushNotificationClientMock*>(client.get());
    ASSERT_TRUE(clientMock);
    EXPECT_CALL(*clientMock, sendPushNotification(testing::_, testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(ClientResponse{Header{HttpStatus::OK}, ""}));

    // throws during delete
    ON_CALL(exporterDbMock, deletePushNotification(testing::_, testing::Eq(123)))
        .WillByDefault(testing::Throw(std::runtime_error("error")));

    PushEventProcessor::ResultType result{PushEventProcessor::ResultType::FailureRetry};
    ASSERT_NO_THROW(result = processor.process());
    // still Success, the event was transmitted.
    EXPECT_EQ(result, PushEventProcessor::ResultType::Success);
}

struct ExponentialBackoffParam {
    int retry;
    std::chrono::seconds expectedDelay;
};
class PushEventProcessorExponentialBackoffTest : public PushEventProcessorTest,
                                                 public testing::WithParamInterface<ExponentialBackoffParam>
{
};

TEST_P(PushEventProcessorExponentialBackoffTest, exponentialBackoff)
{
    PushEventProcessor processor{serviceContext, client.get()};
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1);
    const model::Kvnr kvnr("X12345678");
    db_model::HashedKvnr hashedKvnr;
    hashedKvnr.append("c8c5d1e7de0a204e56970d3c1c32c3cccf9c71e08425fa114b507c381f48cfff");
    const model::PushNotificationEvent event{123,        "erp.task.activate", prescriptionId.toString(), "TaskId",
                                             hashedKvnr, GetParam().retry,    model::Timestamp::now(),   "auditId"};
    ON_CALL(exporterDbMock, processNextPushNotification()).WillByDefault(testing::Return(event));

    const date::year_month_day ymd{model::Timestamp::now().localDay()};
    ON_CALL(mainDbMock, retrievePushNotificationEventData(testing::_))
        .WillByDefault(testing::Return(
            std::vector<model::PushNotificationContext>{makePushNotificationContext(ymd, 123, GetParam().retry)}));

    auto* clientMock = dynamic_cast<PushNotificationClientMock*>(client.get());
    ASSERT_TRUE(clientMock);
    EXPECT_CALL(*clientMock, sendPushNotification(testing::_, testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(ClientResponse{Header{HttpStatus::InternalServerError}, ""}));

    EXPECT_CALL(exporterDbMock,
                updatePushProcessingDelay(testing::Eq(GetParam().retry + 1), testing::Eq(GetParam().expectedDelay),
                                          testing::_, testing::Eq(123)))
        .Times(1);

    PushEventProcessor::ResultType result{PushEventProcessor::ResultType::FailureRetry};
    ASSERT_NO_THROW(result = processor.process());
    EXPECT_EQ(result, PushEventProcessor::ResultType::FailureRetry);
}
INSTANTIATE_TEST_SUITE_P(
    exponentialBackoff, PushEventProcessorExponentialBackoffTest,
    testing::Values(
        ExponentialBackoffParam{.retry = 0, .expectedDelay = std::chrono::minutes{static_cast<int>(std::pow(2, 1))}},
        ExponentialBackoffParam{.retry = 1, .expectedDelay = std::chrono::minutes{static_cast<int>(std::pow(2, 2))}},
        ExponentialBackoffParam{.retry = 2, .expectedDelay = std::chrono::minutes{static_cast<int>(std::pow(2, 3))}},
        ExponentialBackoffParam{.retry = 3, .expectedDelay = std::chrono::minutes{static_cast<int>(std::pow(2, 4))}},
        ExponentialBackoffParam{.retry = 4, .expectedDelay = std::chrono::minutes{static_cast<int>(std::pow(2, 5))}},
        ExponentialBackoffParam{.retry = 5, .expectedDelay = std::chrono::minutes{static_cast<int>(std::pow(2, 6))}},
        ExponentialBackoffParam{.retry = 6, .expectedDelay = std::chrono::minutes{static_cast<int>(std::pow(2, 7))}},
        ExponentialBackoffParam{.retry = 7, .expectedDelay = std::chrono::minutes{static_cast<int>(std::pow(2, 8))}},
        ExponentialBackoffParam{.retry = 8, .expectedDelay = std::chrono::minutes{static_cast<int>(std::pow(2, 9))}},
        ExponentialBackoffParam{.retry = 9, .expectedDelay = std::chrono::minutes{static_cast<int>(std::pow(2, 10))}}));

TEST_F(PushEventProcessorTest, exponentialBackoffMaxRetries)
{
    const EnvironmentVariableGuard varGuard{ConfigurationKey::MEDICATION_EXPORTER_PUSH_MAX_RETRY_COUNT, "10"};
    PushEventProcessor processor{serviceContext, client.get()};
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1);
    const model::Kvnr kvnr("X12345678");
    db_model::HashedKvnr hashedKvnr;
    hashedKvnr.append("c8c5d1e7de0a204e56970d3c1c32c3cccf9c71e08425fa114b507c381f48cfff");
    const model::PushNotificationEvent event{
        123,      "erp.task.activate", prescriptionId.toString(), "TaskId", hashedKvnr, 10, model::Timestamp::now(),
        "auditId"};
    ON_CALL(exporterDbMock, processNextPushNotification()).WillByDefault(testing::Return(event));

    const date::year_month_day ymd{model::Timestamp::now().localDay()};
    ON_CALL(mainDbMock, retrievePushNotificationEventData(testing::_))
        .WillByDefault(
            testing::Return(std::vector<model::PushNotificationContext>{makePushNotificationContext(ymd, 123, 10)}));

    auto* clientMock = dynamic_cast<PushNotificationClientMock*>(client.get());
    ASSERT_TRUE(clientMock);
    EXPECT_CALL(*clientMock, sendPushNotification(testing::_, testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(ClientResponse{Header{HttpStatus::BadRequest}, ""}));

    EXPECT_CALL(exporterDbMock, deletePushNotification(testing::_, testing::Eq(123))).Times(1);

    PushEventProcessor::ResultType result{PushEventProcessor::ResultType::FailureRetry};
    ASSERT_NO_THROW(result = processor.process());
    EXPECT_EQ(result, PushEventProcessor::ResultType::FailureRetry);
}

TEST_F(PushEventProcessorTest, eventTooOld)
{
    PushEventProcessor processor{serviceContext, client.get()};
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1);
    const model::Kvnr kvnr("X12345678");
    db_model::HashedKvnr hashedKvnr;
    hashedKvnr.append("c8c5d1e7de0a204e56970d3c1c32c3cccf9c71e08425fa114b507c381f48cfff");
    using namespace std::chrono_literals;
    const model::PushNotificationEvent event{123,
                                             "erp.task.activate",
                                             prescriptionId.toString(),
                                             "TaskId",
                                             hashedKvnr,
                                             0,
                                             model::Timestamp::now() - 13h,
                                             "auditId"};
    ON_CALL(exporterDbMock, processNextPushNotification()).WillByDefault(testing::Return(event));
    const date::year_month_day ymd{model::Timestamp::now().localDay()};
    ON_CALL(mainDbMock, retrievePushNotificationEventData(testing::_))
        .WillByDefault(testing::Return(std::vector<model::PushNotificationContext>{
            makePushNotificationContext(ymd, 123, 0, model::Timestamp::now() - 13h)}));
    auto* clientMock = dynamic_cast<PushNotificationClientMock*>(client.get());
    ASSERT_TRUE(clientMock);
    EXPECT_CALL(*clientMock, sendPushNotification(testing::_, testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(ClientResponse{Header{HttpStatus::InternalServerError}, ""}));

    EXPECT_CALL(exporterDbMock, deletePushNotification(testing::_, testing::Eq(123))).Times(1);

    PushEventProcessor::ResultType result{PushEventProcessor::ResultType::FailureRetry};
    ASSERT_NO_THROW(result = processor.process());
    EXPECT_EQ(result, PushEventProcessor::ResultType::FailureRetry);
}

TEST_F(PushEventProcessorTest, pushGatewayFailing)
{
    const EnvironmentVariableGuard varGuard{
        ConfigurationKey::MEDICATION_EXPORTER_PUSH_GATEWAY_COOLDOWN_AFTER_ERROR_SECONDS, "10"};
    PushEventProcessor processor{serviceContext, client.get()};
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1);
    const model::Kvnr kvnr("X12345678");
    db_model::HashedKvnr hashedKvnr;
    hashedKvnr.append("c8c5d1e7de0a204e56970d3c1c32c3cccf9c71e08425fa114b507c381f48cfff");
    const model::PushNotificationEvent event{
        123,      "erp.task.activate", prescriptionId.toString(), "TaskId", hashedKvnr, 0, model::Timestamp::now(),
        "auditId"};
    ON_CALL(exporterDbMock, processNextPushNotification()).WillByDefault(testing::Return(event));

    const date::year_month_day ymd{model::Timestamp::now().localDay()};
    ON_CALL(mainDbMock, retrievePushNotificationEventData(testing::_))
        .WillByDefault(
            testing::Return(std::vector<model::PushNotificationContext>{makePushNotificationContext(ymd, 123, 0)}));

    auto* clientMock = dynamic_cast<PushNotificationClientMock*>(client.get());
    ASSERT_TRUE(clientMock);
    EXPECT_CALL(*clientMock, sendPushNotification(testing::_, testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(ClientResponse{Header{HttpStatus::InternalServerError}, ""}));

    // 5xx puts push gateway into cooldown
    EXPECT_FALSE(serviceContext->isPushGatewayFailing("https://push-gateway.location.here/push/v1/"));
    PushEventProcessor::ResultType result{PushEventProcessor::ResultType::FailureRetry};
    ASSERT_NO_THROW(result = processor.process());
    EXPECT_EQ(result, PushEventProcessor::ResultType::FailureRetry);
    EXPECT_TRUE(serviceContext->isPushGatewayFailing("https://push-gateway.location.here/push/v1/"));

    // restore after 10s cooldown
    std::this_thread::sleep_for(std::chrono::seconds{10});
    EXPECT_CALL(*clientMock, sendPushNotification(testing::_, testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(ClientResponse{Header{HttpStatus::OK}, ""}));
    ASSERT_NO_THROW(result = processor.process());
    EXPECT_EQ(result, PushEventProcessor::ResultType::Success);
    EXPECT_FALSE(serviceContext->isPushGatewayFailing("https://push-gateway.location.here/push/v1/"));
}

TEST_F(PushEventProcessorTest, pushKeyRejected)
{
    PushEventProcessor processor{serviceContext, client.get()};
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1);
    const model::Kvnr kvnr("X12345678");
    db_model::HashedKvnr hashedKvnr;
    hashedKvnr.append("c8c5d1e7de0a204e56970d3c1c32c3cccf9c71e08425fa114b507c381f48cfff");
    const model::PushNotificationEvent event{
        123,      "erp.task.activate", prescriptionId.toString(), "TaskId", hashedKvnr, 0, model::Timestamp::now(),
        "auditId"};
    ON_CALL(exporterDbMock, processNextPushNotification()).WillByDefault(testing::Return(event));

    const date::year_month_day ymd{model::Timestamp::now().localDay()};
    const auto pushNotificationContext = makePushNotificationContext(ymd, 123);
    ON_CALL(mainDbMock, retrievePushNotificationEventData(testing::_))
        .WillByDefault(testing::Return(std::vector{pushNotificationContext}));
    EXPECT_CALL(mainDbMock, deletePushKey)
        .WillOnce([&](const model::HashedKvnr& hashedKvnr_, const model::PushKey& pushKey) {
            EXPECT_EQ(hashedKvnr_.getLoggingId(), pushNotificationContext.hashedKvnr().getLoggingId());
            EXPECT_EQ(pushKey.value, pushNotificationContext.device().pushkey());
        });
    EXPECT_CALL(mainDbMock, storeAuditEventData).WillOnce([&](const AuditDataCollector& auditDataCollector) {
        std::optional<model::AuditData> auditData;
        EXPECT_NO_THROW(auditData.emplace(auditDataCollector.createData()));
        EXPECT_TRUE(auditData);
        EXPECT_EQ(auditData->action(), model::AuditEvent::Action::del);
        EXPECT_EQ(auditData->eventId(), model::AuditEventId::POST_PUSHERS_SET_UNREGISTER_IN_EXPORTER);
        EXPECT_FALSE(auditData->prescriptionId().has_value());
        EXPECT_TRUE(auditData->variables().contains("device_display_name"));
        return "";
    });

    auto* clientMock = dynamic_cast<PushNotificationClientMock*>(client.get());
    std::string body =
        R"({"results":[{"id":"9ef1ea9f-3afd-4c56-a838-3ee6efe8fac6","status":"success","rejected":["##PUSHKEY##"],"error":null}],"summary":{"total":1,"successful":1,"failed":0,"partial":0}})";
    body = String::replaceAll(body, "##PUSHKEY##", pushNotificationContext.device().pushkey());
    ASSERT_TRUE(clientMock);
    EXPECT_CALL(*clientMock, sendPushNotification(testing::_, testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(ClientResponse{Header{HttpStatus::OK}, body}));
    EXPECT_CALL(exporterDbMock, deletePushNotification(testing::_, testing::Eq(123))).Times(1);

    PushEventProcessor::ResultType result{PushEventProcessor::ResultType::FailureRetry};
    ASSERT_NO_THROW(result = processor.process());
    EXPECT_EQ(result, PushEventProcessor::ResultType::FailureRejected);
}

TEST_F(PushEventProcessorTest, okFailed)
{
    PushEventProcessor processor{serviceContext, client.get()};
    auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 1);
    const model::Kvnr kvnr("X12345678");
    db_model::HashedKvnr hashedKvnr;
    hashedKvnr.append("c8c5d1e7de0a204e56970d3c1c32c3cccf9c71e08425fa114b507c381f48cfff");
    const model::PushNotificationEvent event{123,        "erp.task.activate", prescriptionId.toString(), "TaskId",
                                             hashedKvnr, 0,    model::Timestamp::now(),   "auditId"};
    ON_CALL(exporterDbMock, processNextPushNotification()).WillByDefault(testing::Return(event));

    const date::year_month_day ymd{model::Timestamp::now().localDay()};
    ON_CALL(mainDbMock, retrievePushNotificationEventData(testing::_))
        .WillByDefault(testing::Return(
            std::vector<model::PushNotificationContext>{makePushNotificationContext(ymd, 123, 0)}));

    auto* clientMock = dynamic_cast<PushNotificationClientMock*>(client.get());
    ASSERT_TRUE(clientMock);
    EXPECT_CALL(*clientMock, sendPushNotification(testing::_, testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Return(ClientResponse{Header{HttpStatus::OK}, R"({"results": [{"id": "9ef1ea9f-3afd-4c56-a838-3ee6efe8fac6", "status": "failed", "rejected": [], "error": "Could not derive push provider from appId=app_id"}], "summary": {"total": 1, "successful": 0, "failed": 1, "partial": 0}})"}));

    EXPECT_CALL(exporterDbMock,
                updatePushProcessingDelay(testing::Eq(1), testing::Eq(std::chrono::minutes{2}),
                                          testing::_, testing::Eq(123)))
        .Times(1);

    PushEventProcessor::ResultType result{PushEventProcessor::ResultType::FailureRetry};
    ASSERT_NO_THROW(result = processor.process());
    EXPECT_EQ(result, PushEventProcessor::ResultType::FailureRetry);
}

TEST_F(PushEventProcessorTest, processEvents_ModelException)
{
    PushEventProcessor processor{serviceContext, client.get()};
    const date::year_month_day ymd{model::Timestamp::now().localDay()};

    auto* clientMock = dynamic_cast<PushNotificationClientMock*>(client.get());
    ASSERT_TRUE(clientMock);
    EXPECT_CALL(*clientMock, sendPushNotification(testing::_, testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Throw(model::ModelException{"some client exception"}));

    EXPECT_CALL(exporterDbMock, updatePushProcessingDelay(testing::_, testing::_, testing::_, testing::_)).Times(1);

    const std::vector<model::PushNotificationContext> events{makePushNotificationContext(ymd, 123)};
    PushEventProcessor::ResultType result{PushEventProcessor::ResultType::Success};
    ASSERT_NO_THROW(result = processor.processEvents(events));
    EXPECT_EQ(result, PushEventProcessor::ResultType::FailureRetry);
}

TEST_F(PushEventProcessorTest, processEvents_StdException)
{
    PushEventProcessor processor{serviceContext, client.get()};
    const date::year_month_day ymd{model::Timestamp::now().localDay()};

    auto* clientMock = dynamic_cast<PushNotificationClientMock*>(client.get());
    ASSERT_TRUE(clientMock);
    EXPECT_CALL(*clientMock, sendPushNotification(testing::_, testing::_, testing::_))
        .Times(1)
        .WillOnce(testing::Throw(std::runtime_error{"some client exception"}));

    EXPECT_CALL(exporterDbMock, updatePushProcessingDelay(testing::_, testing::_, testing::_, testing::_)).Times(1);

    const std::vector<model::PushNotificationContext> events{makePushNotificationContext(ymd, 123)};
    PushEventProcessor::ResultType result{PushEventProcessor::ResultType::Success};
    ASSERT_NO_THROW(result = processor.processEvents(events));
    EXPECT_EQ(result, PushEventProcessor::ResultType::FailureRetry);
}

TEST_F(PushEventProcessorTest, processEvents_MultiEvent_SuccessAndRetry)
{
    PushEventProcessor processor{serviceContext, client.get()};
    const date::year_month_day ymd{model::Timestamp::now().localDay()};

    auto* clientMock = dynamic_cast<PushNotificationClientMock*>(client.get());
    ASSERT_TRUE(clientMock);
    EXPECT_CALL(*clientMock, sendPushNotification(testing::_, testing::_, testing::_))
        .Times(2)
        .WillOnce(testing::Return(ClientResponse{Header{HttpStatus::OK}, ""}))
        .WillOnce(testing::Return(ClientResponse{Header{HttpStatus::InternalServerError}, ""}));

    EXPECT_CALL(exporterDbMock, deletePushNotification(testing::_, testing::Eq(123))).Times(1);
    EXPECT_CALL(exporterDbMock, updatePushProcessingDelay(testing::_, testing::_, testing::_, testing::_)).Times(0);

    const std::vector<model::PushNotificationContext> events{makePushNotificationContext(ymd, 123),
                                                             makePushNotificationContext(ymd, 123)};
    PushEventProcessor::ResultType result{PushEventProcessor::ResultType::FailureRetry};
    ASSERT_NO_THROW(result = processor.processEvents(events));
    EXPECT_EQ(result, PushEventProcessor::ResultType::Success);
}

TEST_F(PushEventProcessorTest, processEvents_MultiEvent_RejectedAndRetry)
{
    PushEventProcessor processor{serviceContext, client.get()};
    const date::year_month_day ymd{model::Timestamp::now().localDay()};

    auto* clientMock = dynamic_cast<PushNotificationClientMock*>(client.get());
    ASSERT_TRUE(clientMock);

    const auto context0 = makePushNotificationContext(ymd, 123);
    const auto context1 = makePushNotificationContext(ymd, 123);

    std::string rejectedBody =
        R"({"results":[{"id":"9ef1ea9f-3afd-4c56-a838-3ee6efe8fac6","status":"success","rejected":["##PUSHKEY##"],"error":null}],"summary":{"total":1,"successful":1,"failed":0,"partial":0}})";
    rejectedBody = String::replaceAll(rejectedBody, "##PUSHKEY##", context0.device().pushkey());

    EXPECT_CALL(*clientMock, sendPushNotification(testing::_, testing::_, testing::_))
        .Times(2)
        .WillOnce(testing::Return(ClientResponse{Header{HttpStatus::OK}, rejectedBody}))
        .WillOnce(testing::Return(ClientResponse{Header{HttpStatus::InternalServerError}, ""}));

    EXPECT_CALL(mainDbMock, deletePushKey).Times(1);
    EXPECT_CALL(exporterDbMock, updatePushProcessingDelay(testing::_, testing::_, testing::_, testing::_)).Times(1);
    EXPECT_CALL(exporterDbMock, deletePushNotification(testing::_, testing::_)).Times(0);

    const std::vector<model::PushNotificationContext> events{context0, context1};
    PushEventProcessor::ResultType result{PushEventProcessor::ResultType::Success};
    ASSERT_NO_THROW(result = processor.processEvents(events));
    EXPECT_EQ(result, PushEventProcessor::ResultType::FailureRetry);
}
