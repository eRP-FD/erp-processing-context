/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/PostgresBackend.hxx"
#include "erp/database/push/PushErpDatabase.hxx"
#include "erp/database/push/PushErpPostgresBackend.hxx"
#include "erp/database/push/PushExporterPostgresBackend.hxx"
#include "erp/model/push/Channels.hxx"
#include "shared/util/ByteHelper.hxx"
#include "shared/util/Hash.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <pqxx/result>
#include <pqxx/transaction>

class ErpWorkflowPushEventsTest : public ErpWorkflowTestTemplate<::testing::Test>
{
public:
    EnvironmentVariableGuard featureToggleGuard{"ERP_FEATURE_EU", "true"};

    static void SetUpTestSuite()
    {
        (void) Fhir::instance();
    }


    void SetUp() override;
    void TearDown() override;
    void subscribe(const std::string& kvnrStr);

    std::set<model::ChannelId> channelsFromPushEvents();

    std::string mKvnr;
    db_model::HashedKvnr mHashedKvnr;
    std::unique_ptr<PushExporterPostgresBackend> mPushDb;
};

void ErpWorkflowPushEventsTest::SetUp()
{
    mPushDb = std::make_unique<PushExporterPostgresBackend>(PushExporterPostgresBackend::mainConnection());

    if (! mPushDb->transaction() && runsInErpTest())
    {
        GTEST_SKIP() << "Push DB not available";
    }
    generateNewRandomKVNR(mKvnr);
    subscribe(mKvnr);
    KeyDerivation keyDerivation{client->getContext()->getHsmPool()};
    mHashedKvnr = keyDerivation.hashKvnr(model::Kvnr(mKvnr));
}

void ErpWorkflowPushEventsTest::TearDown()
{
    mPushDb = nullptr;
}

void ErpWorkflowPushEventsTest::subscribe(const std::string& kvnrStr)
{
    KeyDerivation keyDerivation{client->getContext()->getHsmPool()};
    std::unique_ptr<PushErpDatabase> mPushErpDatabase;
    mPushErpDatabase =
        std::make_unique<PushErpDatabase>(std::make_unique<PushErpPostgresBackend>(PostgresBackend::mainConnection()),
                                          client->getContext()->getHsmPool(), keyDerivation);
    const model::PushKey pushkey{"pushkey"};
    const model::AppId appId{"app_id"};
    const model::Pusher pusher{
        pushkey,
        appId,
        +"appDisplayName",
        +"deviceDisplayName",
        model::Lang{"de"},
        model::PusherData{"https://push-gateway.location.here/push/v1/", "format"},
        model::Encryption{"2026-05", SafeString{"iss01234567890123456789012345678"}, "keyIdentifier"}};
    mPushErpDatabase->createOrUpdateRegistration(model::Kvnr(kvnrStr), pusher);
    model::Channels channels{};

    channels.add(model::ChannelId::erp_communication_new);

    channels.add(model::ChannelId::erp_task_activate);
    channels.add(model::ChannelId::erp_task_accept);
    channels.add(model::ChannelId::erp_task_close);
    channels.add(model::ChannelId::erp_task_abort);
    channels.add(model::ChannelId::erp_task_reject);
    channels.add(model::ChannelId::erp_task_dispense);

    channels.add(model::ChannelId::erp_task_vertreter);

    channels.add(model::ChannelId::erp_chargeitem_create);
    channels.add(model::ChannelId::erp_chargeitem_update);

    channels.add(model::ChannelId::erp_eu_prescription_redeem);
    channels.add(model::ChannelId::erp_eu_prescription_get);
    channels.add(model::ChannelId::erp_eu_prescription_close);

    mPushErpDatabase->updateChannels(model::Kvnr{kvnrStr}, pushkey, db_model::HashedId::fromString(appId.value), channels);
    mPushErpDatabase->commitTransaction();
}

std::set<model::ChannelId> ErpWorkflowPushEventsTest::channelsFromPushEvents()
{
    auto connection = std::make_unique<pqxx::connection>(PushExporterPostgresBackend::defaultConnectParameters().str());
    auto txn = pqxx::work{*connection};
    auto rows = txn.exec("SELECT channel_id FROM erp_event.push_notification_event WHERE kvnr_hashed = $1",
                         pqxx::params{mHashedKvnr});
    std::set<model::ChannelId> values;
    for (const auto& row : rows)
    {
        values.insert( model::toChannelId( row[0].as<std::string>()) );
    }
    return values;
}

TEST_F(ErpWorkflowPushEventsTest, taskPushEventsGeneration_DispenseTaskUpdateOwner)
{
    const model::PrescriptionType workflowType = model::PrescriptionType::apothekenpflichigeArzneimittel;

    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, workflowType));

    std::string qesBundle;
    std::vector<model::Communication> communications;

    ASSERT_NO_THROW(qesBundle =
                        std::get<0>(makeQESBundle(mKvnr, *prescriptionId, model::Timestamp::now(), std::nullopt, false)));
    auto taskOrOperationOutcome = taskActivate(*prescriptionId, accessCode, qesBundle, HttpStatus::OK);

    std::string secret;
    std::optional<model::Timestamp> lastModifiedDate;
    ASSERT_NO_FATAL_FAILURE(checkTaskAccept(secret, lastModifiedDate, *prescriptionId, mKvnr, accessCode, qesBundle));
    mDispenseTaskRequestArgs.overrideTelematikId = "3-SMC-B-Testkarte-883110000120999";
    ASSERT_NO_FATAL_FAILURE(taskDispense(*prescriptionId, secret, mKvnr));

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    const std::set<model::ChannelId> pushEventChannels = channelsFromPushEvents();
    EXPECT_EQ(pushEventChannels.size(), 3);
    EXPECT_TRUE(pushEventChannels.contains(model::ChannelId::erp_task_activate));
    EXPECT_TRUE(pushEventChannels.contains(model::ChannelId::erp_task_accept));
    EXPECT_TRUE(pushEventChannels.contains(model::ChannelId::erp_task_dispense));
}

TEST_F(ErpWorkflowPushEventsTest, taskPushEventsGeneration_TaskLifecycleReject)// NOLINT
{
    const model::PrescriptionType workflowType = model::PrescriptionType::apothekenpflichigeArzneimittel;

    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(workflowType));
    ASSERT_TRUE(task);

    const std::string accessCode(task->accessCode());

    std::string qesBundle;
    ASSERT_NO_THROW(qesBundle =
                        std::get<0>(makeQESBundle(mKvnr, task->prescriptionId(), model::Timestamp::now(), std::nullopt, false)));
    auto taskOrOperationOutcome = taskActivate(task->prescriptionId(), accessCode, qesBundle, HttpStatus::OK);

    std::string secret;
    std::optional<model::Timestamp> lastModifiedDate;
    ASSERT_NO_FATAL_FAILURE(
        checkTaskAccept(secret, lastModifiedDate, task->prescriptionId(), mKvnr, accessCode, qesBundle));

    ASSERT_NO_FATAL_FAILURE(checkTaskReject(task->prescriptionId(), mKvnr, accessCode, secret));

    const std::set<model::ChannelId> pushEventChannels = channelsFromPushEvents();
    EXPECT_TRUE(pushEventChannels.contains(model::ChannelId::erp_task_accept));
    EXPECT_TRUE(pushEventChannels.contains(model::ChannelId::erp_task_activate));
    EXPECT_TRUE(pushEventChannels.contains(model::ChannelId::erp_task_reject));
}

TEST_F(ErpWorkflowPushEventsTest, taskPushEventsGeneration_TaskAbort_NewlyCreated)// NOLINT
{
    const model::PrescriptionType workflowType = model::PrescriptionType::apothekenpflichigeArzneimittel;

    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(workflowType));
    ASSERT_TRUE(task);

    const std::string accessCode(task->accessCode());

    std::string qesBundle;
    ASSERT_NO_THROW(qesBundle =
                        std::get<0>(makeQESBundle(mKvnr, task->prescriptionId(), model::Timestamp::now(), std::nullopt, false)));
    auto taskOrOperationOutcome = taskActivate(task->prescriptionId(), accessCode, qesBundle, HttpStatus::OK);

    ASSERT_NO_FATAL_FAILURE(taskAbort(task->prescriptionId(), JwtBuilder::testBuilder().makeJwtArzt(), accessCode, {},
                                      HttpStatus::NoContent, model::OperationOutcome::Issue::Type::forbidden));



    std::this_thread::sleep_for(std::chrono::milliseconds(500));// Abort operation requires some processing time.
    const std::set<model::ChannelId> pushEventChannels = channelsFromPushEvents();
    EXPECT_EQ(pushEventChannels.size(), 2);
    EXPECT_TRUE(pushEventChannels.contains(model::ChannelId::erp_task_activate));
    EXPECT_TRUE(pushEventChannels.contains(model::ChannelId::erp_task_abort));
}

TEST_F(ErpWorkflowPushEventsTest, taskPushEventsGeneration_TaskLifecycleNormal)// NOLINT
{
    const model::PrescriptionType workflowType = model::PrescriptionType::digitaleGesundheitsanwendungen;

    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, workflowType));

    std::string qesBundle;
    ASSERT_NO_THROW(qesBundle =
                        std::get<0>(makeQESBundle(mKvnr, *prescriptionId, model::Timestamp::now(), std::nullopt, false)));
    auto taskOrOperationOutcome = taskActivate(*prescriptionId, accessCode, qesBundle, HttpStatus::OK);

    std::string secret;
    std::optional<model::Timestamp> lastModifiedDate;
    ASSERT_NO_FATAL_FAILURE(checkTaskAccept(secret, lastModifiedDate, *prescriptionId, mKvnr, accessCode, qesBundle));

    taskClose(*prescriptionId, secret, mKvnr);

    const std::set<model::ChannelId> pushEventChannels = channelsFromPushEvents();
    EXPECT_EQ(pushEventChannels.size(), 3);
    EXPECT_TRUE(pushEventChannels.contains(model::ChannelId::erp_task_activate));
    EXPECT_TRUE(pushEventChannels.contains(model::ChannelId::erp_task_accept));
    EXPECT_TRUE(pushEventChannels.contains(model::ChannelId::erp_task_close));
}

TEST_F(ErpWorkflowPushEventsTest, taskByIdPushEventsGeneration_GetTaskById)
{
    const model::PrescriptionType workflowType = model::PrescriptionType::apothekenpflichigeArzneimittel;

    model::Kvnr kvnrRepresentative{"X234567891"};
    subscribe(kvnrRepresentative.id());

    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, workflowType));

    std::string qesBundle;
    std::vector<model::Communication> communications;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, mKvnr, accessCode));

    testing::internal::CaptureStderr();
    std::optional<model::Bundle> taskBundle;
    ASSERT_NO_FATAL_FAILURE(taskBundle = taskGetId(*prescriptionId, kvnrRepresentative.id(), accessCode));

    std::string output = testing::internal::GetCapturedStderr();
    TVLOG(1)<<"~~~~~~~~~~~~~~~~~~~~~";
    TVLOG(1) << output;// Write the captured log, for completeness.
    TVLOG(1)<<"~~~~~~~~~~~~~~~~~~~~~";
    EXPECT_TRUE(
        output.find(fmt::format(
            R"("event":"Create Push Event","channel_id":"erp.task.vertreter","prescription_id":"{}","kvnr":"{}")",
            prescriptionId->toString(), mHashedKvnr.toHex())) != std::string::npos)
        << output;

    const std::set<model::ChannelId> pushEventChannels = channelsFromPushEvents();
    EXPECT_EQ(pushEventChannels.size(), 3);
    EXPECT_TRUE(pushEventChannels.contains(model::ChannelId::erp_task_activate));
    EXPECT_TRUE(pushEventChannels.contains(model::ChannelId::erp_task_vertreter));
    EXPECT_TRUE(pushEventChannels.contains(model::ChannelId::erp_communication_new));
}

TEST_F(ErpWorkflowPushEventsTest,
       chargeItemPushEventsGeneration_PkvChargeItemPostandPut)//NOLINT(readability-function-cognitive-complexity)
{
    const model::PrescriptionType wfType = model::PrescriptionType::apothekenpflichtigeArzneimittelPkv;

    model::Timestamp startTime = model::Timestamp::now();

    std::optional<model::Consent> consent;
    ASSERT_NO_FATAL_FAILURE(consent = consentPost(model::ConsentType::CHARGCONS, mKvnr, startTime));
    ASSERT_TRUE(consent.has_value());

    const std::size_t numOfTasks = 4;
    std::vector<std::optional<model::PrescriptionId>> prescriptionIds(numOfTasks);
    std::vector<std::optional<model::KbvBundle>> kbvBundles(numOfTasks);
    std::vector<std::optional<model::ErxReceipt>> closeReceipts(numOfTasks);
    std::vector<std::string> accessCodes(numOfTasks);
    std::vector<std::string> secrets(numOfTasks);
    for (std::size_t i = 0; i < numOfTasks; ++i)
    {
        ASSERT_NO_FATAL_FAILURE(createClosedTask(prescriptionIds[i], kbvBundles[i], closeReceipts[i], accessCodes[i],
                                                 secrets[i], wfType, mKvnr, "PKV"));
    }

    const auto telematicIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    std::vector<std::optional<model::ChargeItem>> createdChargeItems(numOfTasks);
    for (std::size_t i = 0; i < numOfTasks; ++i)
    {
        ASSERT_NO_FATAL_FAILURE(createdChargeItems[i] =
                                    chargeItemPost(*prescriptionIds[i], mKvnr, telematicIdPharmacy, secrets[i]));
        EXPECT_FALSE(createdChargeItems[i]->accessCode().has_value());
        EXPECT_FALSE(createdChargeItems[i]->containedBinary());
    }

    const auto jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(mKvnr);

    std::optional<model::Bundle> chargeItemsBundle;
    ASSERT_NO_FATAL_FAILURE(chargeItemsBundle = chargeItemsGet(
                                jwtInsurant, ContentMimeType::fhirJsonUtf8,
                                "entered-date=ge" + startTime.toXsDateTimeWithoutFractionalSeconds().substr(0, 19) +
                                    "Z&_sort=entered-date"));
    auto chargeItems = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
    EXPECT_EQ(chargeItems.size(), numOfTasks);

    model::PatchChargeItemParameters::MarkingFlag markingFlag{};
    markingFlag.taxOffice.value = true;
    markingFlag.insuranceProvider.value = true;
    markingFlag.subsidy.value = true;
    std::optional<model::ChargeItem> chargeItem2Changed;
    ASSERT_NO_FATAL_FAILURE(chargeItem2Changed =
                                chargeItemPatch(*createdChargeItems[1]->id(), jwtInsurant, markingFlag));

    const auto dispenseBundleString =
        ResourceTemplates::davDispenseItemXml({.prescriptionId = prescriptionIds.at(1).value()});
    const Uuid uuid;
    auto dispenseBundle = model::Bundle::fromXmlNoValidation(dispenseBundleString);
    dispenseBundle.setId(uuid);

    std::variant<model::ChargeItem, model::OperationOutcome> chargeItem2Changed2;
    chargeItem2Changed->deleteSupportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle);
    ASSERT_NO_FATAL_FAILURE(chargeItem2Changed2 = chargeItemPut(
                                jwtApotheke(), ContentMimeType::fhirXmlUtf8, *chargeItem2Changed,
                                dispenseBundle.serializeToXmlString(), chargeItem2Changed->accessCode().value()));
    const std::set<model::ChannelId> pushEventChannels = channelsFromPushEvents();
    EXPECT_EQ(pushEventChannels.size(), 4);
    EXPECT_TRUE(pushEventChannels.contains(model::ChannelId::erp_chargeitem_create));
    EXPECT_TRUE(pushEventChannels.contains(model::ChannelId::erp_chargeitem_update));
    EXPECT_TRUE(pushEventChannels.contains(model::ChannelId::erp_task_activate));
    EXPECT_TRUE(pushEventChannels.contains(model::ChannelId::erp_task_accept));
    // No close event, due to workflow type.
}
