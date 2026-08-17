/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/push/Channels.hxx"
#include "fhirtools/model/NumberAsStringParserWriter.hxx"
#include "shared/database/DatabaseModel.hxx"
#include "shared/util/Hash.hxx"
#include "test/erp/database/PostgresDatabaseTestFixture.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/mock/PushErpMockBackend.hxx"
#include "test/util/JsonTestUtils.hxx"

#include <gmock/gmock.h>

class ErpAppRegistrationTableTest : public PostgresDatabaseTest
{
public:
    void SetUp() override
    {
        if (! usePostgres())
        {
            GTEST_SKIP() << "Postgres only test";
        }
        PostgresDatabaseTest::SetUp();
    }

    void cleanup() override
    {
        clearTables();
    }

    void comparePushers(const model::Pusher& expected, const model::Pusher& actual)
    {
        EXPECT_EQ(expected.pushKey().value, actual.pushKey().value);
        EXPECT_EQ(expected.appId().value, actual.appId().value);
        EXPECT_EQ(expected.appDisplayName(), actual.appDisplayName());
        EXPECT_EQ(expected.deviceDisplayName(), actual.deviceDisplayName());
        EXPECT_EQ(expected.lang().value, actual.lang().value);
        EXPECT_EQ(expected.pusherData().mUrl, actual.pusherData().mUrl);
        EXPECT_EQ(expected.pusherData().mUserData == nullptr, actual.pusherData().mUserData == nullptr);
        if (expected.pusherData().mUserData)
        {
            EXPECT_EQ(jsonToString(*expected.pusherData().mUserData), jsonToString(*actual.pusherData().mUserData));
        }
        // Pusher.encryption is not read back from database
    }
};

TEST_F(ErpAppRegistrationTableTest, crud)
{
    const model::Kvnr kvnr{InsurantA};
    const model::PushKey pushkey{"pushkey"};
    const model::AppId appId{"app_id"};

    const model::Pusher pusher{
        pushkey,
        appId,
        "appDisplayName",
        "deviceDisplayName",
        model::Lang{"de"},
        model::PusherData{"https://push-gateway.location.here/push/v1/", "format"},
        model::Encryption{"2026-05", SafeString{"iss11111111111111111111111111111"}, "keyIdentifier"}};

    // CREATE
    pushErpDatabase().createOrUpdateRegistration(kvnr, pusher);
    pushErpDatabase().commitTransaction();

    {
        // READ
        auto foundReg = pushErpDatabase().findRegistration(pushkey, appId, kvnr);
        pushErpDatabase().commitTransaction();
        ASSERT_TRUE(foundReg.has_value());
        comparePushers(pusher, *foundReg);

        // READ
        auto getRegs = pushErpDatabase().getRegistrations(kvnr);
        pushErpDatabase().commitTransaction();
        ASSERT_EQ(getRegs.size(), 1);
        comparePushers(pusher, getRegs.at(0));
    }

    const model::Pusher updatePusher{
        pushkey,
        appId,
        "Updated appDisplayName",
        "Updated deviceDisplayName",
        model::Lang{"en"},
        model::PusherData{"https://push-gateway.location.here/push/v1/", "format"},
        model::Encryption{"2026-05", SafeString{"iss11111111111111111111111111112"}, "updated keyIdentifier"}};
    // UPDATE
    pushErpDatabase().createOrUpdateRegistration(kvnr, updatePusher);
    pushErpDatabase().commitTransaction();

    {
        // READ
        auto foundReg = pushErpDatabase().findRegistration(pushkey, appId, kvnr);
        pushErpDatabase().commitTransaction();
        ASSERT_TRUE(foundReg.has_value());
        comparePushers(updatePusher, *foundReg);

        // READ
        auto getRegs = pushErpDatabase().getRegistrations(kvnr);
        pushErpDatabase().commitTransaction();
        ASSERT_EQ(getRegs.size(), 1);
        comparePushers(updatePusher, getRegs.at(0));
    }

    // DELETE
    pushErpDatabase().deleteRegistration(kvnr, pushkey, appId);
    pushErpDatabase().commitTransaction();

    {
        // READ
        auto foundReg = pushErpDatabase().findRegistration(pushkey, appId, kvnr);
        pushErpDatabase().commitTransaction();
        ASSERT_FALSE(foundReg.has_value());

        // READ
        auto getRegs = pushErpDatabase().getRegistrations(kvnr);
        pushErpDatabase().commitTransaction();
        ASSERT_EQ(getRegs.size(), 0);
    }
}

TEST_F(ErpAppRegistrationTableTest, channels)
{
    const model::Kvnr kvnr{InsurantA};
    const model::PushKey pushkey{"pushkey"};
    const model::AppId appId{"app_id"};

    {
        const auto [_, channels] = pushErpDatabase().getChannels(kvnr, pushkey);
        pushErpDatabase().commitTransaction();
        ASSERT_FALSE(channels.has_value());
    }

    const model::Pusher pusher{
        pushkey,
        appId,
        "appDisplayName",
        "deviceDisplayName",
        model::Lang{"de"},
        model::PusherData{"https://push-gateway.location.here/push/v1/", "format"},
        model::Encryption{"2026-05", SafeString{"iss11111111111111111111111111111"}, "keyIdentifier"}};

    pushErpDatabase().createOrUpdateRegistration(kvnr, pusher);
    pushErpDatabase().commitTransaction();

    // one channel
    {
        model::Channels channels{};
        channels.add(model::ChannelId::erp_communication_new);
        pushErpDatabase().updateChannels(kvnr, pushkey, db_model::HashedId::fromString(appId.value), channels);
        pushErpDatabase().commitTransaction();
    }
    {
        const auto [_, channels] = pushErpDatabase().getChannels(kvnr, pushkey);
        pushErpDatabase().commitTransaction();
        ASSERT_TRUE(channels.has_value());
        ASSERT_EQ(channels->activeChannels().size(), 1);
        EXPECT_EQ(*channels->activeChannels().begin(), model::ChannelId::erp_communication_new);
    }

    // all channels:
    {
        model::Channels channels{};
        for (auto [channelId, _] : magic_enum::enum_entries<model::ChannelId>())
        {
            channels.add(channelId);
        }
        pushErpDatabase().updateChannels(kvnr, pushkey, db_model::HashedId::fromString(appId.value), channels);
        pushErpDatabase().commitTransaction();
    }
    {
        const auto [_, channels] = pushErpDatabase().getChannels(kvnr, pushkey);
        pushErpDatabase().commitTransaction();
        ASSERT_TRUE(channels.has_value());
        ASSERT_EQ(channels->activeChannels().size(), magic_enum::enum_count<model::ChannelId>());
        for (const auto& activeChannel : channels->activeChannels())
        {
            EXPECT_TRUE(magic_enum::enum_contains<model::ChannelId>(activeChannel));
        }
    }

    // three channels:
    {
        model::Channels channels{};
        channels.add(model::ChannelId::erp_task_accept);
        channels.add(model::ChannelId::erp_chargeitem_update);
        channels.add(model::ChannelId::erp_eu_prescription_close);
        pushErpDatabase().updateChannels(kvnr, pushkey, db_model::HashedId::fromString(appId.value), channels);
        pushErpDatabase().commitTransaction();
    }
    {
        const auto [_, channels] = pushErpDatabase().getChannels(kvnr, pushkey);
        pushErpDatabase().commitTransaction();
        ASSERT_TRUE(channels.has_value());
        ASSERT_EQ(channels->activeChannels().size(), 3);
        EXPECT_TRUE(channels->activeChannels().contains(model::ChannelId::erp_task_accept));
        EXPECT_TRUE(
            channels->activeChannels().contains(model::ChannelId::erp_chargeitem_update));
        EXPECT_TRUE(
            channels->activeChannels().contains(model::ChannelId::erp_eu_prescription_close));
    }
}

TEST_F(ErpAppRegistrationTableTest, multiplePushkeys)
{
    const model::Kvnr kvnr{InsurantA};
    const model::PushKey pushkey1{"pushkey1"};
    const model::AppId appId1{"app_id1"};
    const model::PushKey pushkey2{"pushkey2"};
    const model::AppId appId2{"app_id2"};

    const model::Pusher pusher1{
        pushkey1,
        appId1,
        "appDisplayName",
        "deviceDisplayName",
        model::Lang{"de"},
        model::PusherData{"https://push-gateway.location.here/push/v1/", "format"},
        model::Encryption{"2026-05", SafeString{"iss11111111111111111111111111111"}, "keyIdentifier"}};
    const model::Pusher pusher2{
        pushkey2,
        appId2,
        "appDisplayName",
        "deviceDisplayName",
        model::Lang{"de"},
        model::PusherData{"https://push-gateway.location.here/push/v1/", "format"},
        model::Encryption{"2026-05", SafeString{"iss11111111111111111111111111111"}, "keyIdentifier"}};

    // CREATE
    pushErpDatabase().createOrUpdateRegistration(kvnr, pusher1);
    pushErpDatabase().createOrUpdateRegistration(kvnr, pusher2);
    pushErpDatabase().commitTransaction();

    {
        // READ
        auto foundReg = pushErpDatabase().findRegistration(pushkey1, appId1, kvnr);
        pushErpDatabase().commitTransaction();
        ASSERT_TRUE(foundReg.has_value());
        comparePushers(pusher1, *foundReg);

        // READ
        auto getRegs = pushErpDatabase().getRegistrations(kvnr);
        pushErpDatabase().commitTransaction();
        ASSERT_EQ(getRegs.size(), 2);
        comparePushers(pusher1, getRegs.at(0));
        comparePushers(pusher2, getRegs.at(1));
    }

    {
        model::Channels channels{};
        channels.add(model::ChannelId::erp_communication_new);
        pushErpDatabase().updateChannels(kvnr, pushkey1, db_model::HashedId::fromString(appId1.value), channels);
        pushErpDatabase().commitTransaction();
    }
    {
        model::Channels channels{};
        channels.add(model::ChannelId::erp_task_accept);
        pushErpDatabase().updateChannels(kvnr, pushkey2, db_model::HashedId::fromString(appId2.value), channels);
        pushErpDatabase().commitTransaction();
    }

    {
        const auto [_, channels] = pushErpDatabase().getChannels(kvnr, pushkey1);
        pushErpDatabase().commitTransaction();
        ASSERT_TRUE(channels.has_value());
        ASSERT_EQ(channels->activeChannels().size(), 1);
        EXPECT_EQ(*channels->activeChannels().begin(), model::ChannelId::erp_communication_new);
    }

    {
        const auto [_, channels] = pushErpDatabase().getChannels(kvnr, pushkey2);
        pushErpDatabase().commitTransaction();
        ASSERT_TRUE(channels.has_value());
        ASSERT_EQ(channels->activeChannels().size(), 1);
        EXPECT_EQ(*channels->activeChannels().begin(), model::ChannelId::erp_task_accept);
    }

    {
        const auto [_, channels] = pushErpDatabase().getChannels(kvnr, model::PushKey{"pushkey3"});
        pushErpDatabase().commitTransaction();
        ASSERT_FALSE(channels.has_value());
    }
}

TEST_F(ErpAppRegistrationTableTest, multipleRegistrationsSamePushkey)
{
    const model::Kvnr kvnr{InsurantA};
    const model::PushKey pushkey{"pushkey"};
    const model::AppId appId1{"app_id1"};
    const std::string appId1Hashed = Hash::sha256(appId1.value);
    const model::AppId appId2{"app_id2"};

    const model::Pusher pusher1{
        pushkey,
        appId1,
        "appDisplayName",
        "deviceDisplayName",
        model::Lang{"de"},
        model::PusherData{"https://push-gateway.location.here/push/v1/", "format"},
        model::Encryption{"2026-05", SafeString{"iss11111111111111111111111111111"}, "keyIdentifier"}};
    pushErpDatabase().createOrUpdateRegistration(kvnr, pusher1);
    pushErpDatabase().commitTransaction();
    {
        model::Channels channels{};
        channels.add(model::ChannelId::erp_task_accept);
        pushErpDatabase().updateChannels(kvnr, pushkey, db_model::HashedId::fromString(appId1.value), channels);
        pushErpDatabase().commitTransaction();
    }

    const model::Pusher pusher2{
        pushkey,
        appId2,
        "appDisplayName",
        "deviceDisplayName",
        model::Lang{"de"},
        model::PusherData{"https://push-gateway.location.here/push/v1/", "format"},
        model::Encryption{"2026-05", SafeString{"iss11111111111111111111111111111"}, "keyIdentifier"}};
    pushErpDatabase().createOrUpdateRegistration(kvnr, pusher2);
    pushErpDatabase().commitTransaction();

    auto [_, channels] = pushErpDatabase().getChannels(kvnr, pushkey);
    pushErpDatabase().commitTransaction();
    ASSERT_TRUE(channels.has_value());
    ASSERT_EQ(channels->activeChannels().size(), 0);
}

TEST_F(ErpAppRegistrationTableTest, pushKeyMismatch_Throws)
{
    HsmPool hsmPool{
        std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(),
                                         MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm)),
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>()};

    const model::Kvnr kvnr{InsurantA};
    const model::PushKey pushkey{"pushkey"};
    const model::AppId appId{"app_id"};
    const db_model::HashedKvnr hashedKvnr = getKeyDerivation().hashKvnr(kvnr);

    const model::Pusher storedPusher{
        model::PushKey{"pushkey"},
        appId,
        "appDisplayName",
        "deviceDisplayName",
        model::Lang{"de"},
        model::PusherData{"https://push-gateway.location.here/push/v1/", "format"},
        model::Encryption{"2026-05", SafeString{"iss11111111111111111111111111111"}, "keyIdentifier"}};

    const model::Pusher storedPusher2{
        model::PushKey{"pushkey_collision"},
        appId,
        "appDisplayName",
        "deviceDisplayName",
        model::Lang{"de"},
        model::PusherData{"https://push-gateway.location.here/push/v1/", "format"},
        model::Encryption{"2026-05", SafeString{"iss11111111111111111111111111111"}, "keyIdentifier"}};

    const auto hashedPushKey = db_model::HashedId::fromString(pushkey.value);
    const auto hashedPushKey1 = db_model::HashedId::fromString(storedPusher.pushKey().value);
    const auto hashedAppId = db_model::HashedId::fromString(storedPusher.appId().value);;

    const auto [key, derivationData] = getKeyDerivation().initialAppRegistrationKey(hashedPushKey, hashedKvnr);
    const auto now = model::Timestamp::now();
    std::optional<db_model::Pusher> storedPusherDb(
        {.hashedPushKey = hashedPushKey1,
         .hashedAppId = hashedAppId,
         .kvnr = hashedKvnr,
         .blobId = derivationData.blobId,
         .salt = db_model::Blob{derivationData.salt},
         .payload = getDBCodec().encode(storedPusher2.dbPayloadJson(), key, Compression::DictionaryUse::Default_json),
         .url = storedPusher.pusherData().mUrl,
         .lastModified = now});

    auto db = std::make_unique<PushErpDatabase>(std::make_unique<PushErpMockBackend>(), hsmPool,
                                                getKeyDerivation());

    EXPECT_CALL(dynamic_cast<PushErpMockBackend&>(db->getBackend()), findRegistration(db_model::HashedId::fromString(pushkey.value), db_model::HashedId::fromString(appId.value), hashedKvnr))
        .WillOnce(testing::Return(storedPusherDb));

    try
    {
        db->findRegistration(pushkey, appId, kvnr);
        FAIL() << "Expected ErpException";
    }
    catch (const ErpException& exc)
    {
        EXPECT_EQ(exc.status(), HttpStatus::BadRequest);
        EXPECT_TRUE(std::string(exc.what()).find("Mismatch in") != std::string::npos);
        EXPECT_TRUE(std::string(exc.what()).find("kvnr: ") != std::string::npos);
        EXPECT_TRUE(std::string(exc.what()).find("pushkey: ") != std::string::npos);
        EXPECT_FALSE(std::string(exc.what()).find("appId: ") != std::string::npos);
    }
}
