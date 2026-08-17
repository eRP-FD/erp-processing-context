/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "ErpWorkflowTestFixture.hxx"

#undef Expect
#include "erp/model/push/Channels.hxx"
#include "fhirtools/model/NumberAsStringParserWriter.hxx"
#include "shared/ErpRequirements.hxx"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

class TestPusher : public model::Pusher
{
public:
    // initialize base class with empty model::Encryption
    TestPusher(const rapidjson::Value& json)
        : Pusher(model::PushKey{json["pushkey"].GetString()}, model::AppId{json["app_id"].GetString()},
                 json["app_display_name"].GetString(), json["device_display_name"].GetString(),
                 model::Lang{json["lang"].GetString()}, model::PusherData{json["data"]}, model::Encryption{})
    {
    }
};


class ErpWorkflowPushTest : public ErpWorkflowTestBase, public testing::Test
{
    static constexpr auto testsA_28111 = A_28111.tests("OpenApi_Notification_Fachdienst");
    static constexpr auto testsA_27104 = A_27104.tests("OpenApi_Notification_Fachdienst");
    static constexpr auto testsA_28117 = A_28117.tests("Channels - OpenApi_Notification_Fachdienst");

protected:
    void SetUp() override
    {
        if (! TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false) &&
            runsInErpTest())
        {
            GTEST_SKIP_("Push tests require postgres");
        }
    }

public:
    void pushRegister(const model::Pusher& pusher, ClientResponse& outInnerResponse,
                      const std::string& kvnr = "X123456788");
    void pushDelete(const model::DeletePusher& deletePusher, ClientResponse& outInnerResponse,
                    const std::string& kvnr = "X123456788");
    void pushGet(std::vector<TestPusher>& outPusher, ClientResponse& outInnerResponse);
    void comparePushers(const model::Pusher& expected, const model::Pusher& actual);
    void compareUserData(std::shared_ptr<rapidjson::Document> lhs, std::shared_ptr<rapidjson::Document> rhs);
    void getChannels(const std::optional<model::PushKey>& pushkey, model::Channels& outChannels,
                     ClientResponse& outInnerResponse);
    void updateChannels(const model::PushKey& pushkey, const model::Channels& channels,
                        ClientResponse& outInnerResponse);
    void updateChannels(const model::PushKey& pushkey, const std::string& body, ClientResponse& outInnerResponse);

    std::string pushKey{"Xp/MzCt8/9DcSNE9cuiaoT5Ac55job3TdLSSmtmYl4A="};
};

void ErpWorkflowPushTest::pushRegister(const model::Pusher& pusher, ClientResponse& outInnerResponse,
                                       const std::string& kvnr)
{
    auto body = pusher.serializeToJsonString();
    RequestArguments requestArguments{HttpMethod::POST, "/pushers/v1/set", body, ContentMimeType::jsonUtf8};
    requestArguments.headerFields.emplace(Header::Accept, "application/json");
    requestArguments.jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    auto [outerResponse, innerResponse] = send(requestArguments);
    EXPECT_THAT(innerResponse.getHeader().status(),
                ::testing::AnyOf(HttpStatus::OK, HttpStatus::BadRequest, HttpStatus::Unauthorized,
                                 HttpStatus::Forbidden, HttpStatus::MethodNotAllowed, HttpStatus::NotAcceptable,
                                 HttpStatus::RequestTimeout, HttpStatus::TooManyRequests));
    if (innerResponse.getHeader().status() == HttpStatus::OK)
    {
        EXPECT_EQ(innerResponse.getBody(), "{}") << innerResponse.getBody();
    }
    else if (innerResponse.getHeader().status() == HttpStatus::BadRequest)
    {
        rapidjson::Document document;
        ASSERT_NO_THROW(document.Parse(innerResponse.getBody())) << innerResponse.getBody();
        ASSERT_FALSE(document.HasParseError()) << innerResponse.getBody();
        ASSERT_TRUE(document["errorCode"].IsString()) << innerResponse.getBody();
    }
    outInnerResponse = innerResponse;
}
void ErpWorkflowPushTest::pushDelete(const model::DeletePusher& deletePusher, ClientResponse& outInnerResponse,
                                     const std::string& kvnr)
{
    auto body = deletePusher.serializeToJsonString();
    RequestArguments requestArguments{HttpMethod::POST, "/pushers/v1/set", body, ContentMimeType::jsonUtf8};
    requestArguments.headerFields.emplace(Header::Accept, "application/json");
    requestArguments.jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    auto [outerResponse, innerResponse] = send(requestArguments);
    EXPECT_THAT(innerResponse.getHeader().status(),
                ::testing::AnyOf(HttpStatus::OK, HttpStatus::BadRequest, HttpStatus::Unauthorized,
                                 HttpStatus::Forbidden, HttpStatus::MethodNotAllowed, HttpStatus::NotAcceptable,
                                 HttpStatus::RequestTimeout, HttpStatus::TooManyRequests));
    if (innerResponse.getHeader().status() == HttpStatus::OK)
    {
        EXPECT_EQ(innerResponse.getBody(), "{}") << innerResponse.getBody();
    }
    else if (innerResponse.getHeader().status() == HttpStatus::BadRequest)
    {
        rapidjson::Document document;
        ASSERT_NO_THROW(document.Parse(innerResponse.getBody()));
        ASSERT_FALSE(document.HasParseError()) << innerResponse.getBody();
        ASSERT_TRUE(document["errorCode"].IsString());
    }
    outInnerResponse = innerResponse;
}
void ErpWorkflowPushTest::pushGet(std::vector<TestPusher>& outPusher, ClientResponse& outInnerResponse)
{
    A_28530.test("App-Registrierungen abrufen - Filter auf KVNR des Versicherten");
    RequestArguments requestArguments{HttpMethod::GET, "/pushers/v1", ""};
    requestArguments.headerFields.emplace(Header::Accept, "application/json");
    auto [outerResponse, innerResponse] = send(requestArguments);
    EXPECT_THAT(innerResponse.getHeader().status(),
                ::testing::AnyOf(HttpStatus::OK, HttpStatus::BadRequest, HttpStatus::Unauthorized,
                                 HttpStatus::Forbidden, HttpStatus::MethodNotAllowed, HttpStatus::NotAcceptable,
                                 HttpStatus::RequestTimeout, HttpStatus::TooManyRequests));
    rapidjson::Document document;
    ASSERT_NO_THROW(document.Parse(innerResponse.getBody()));
    ASSERT_FALSE(document.HasParseError()) << innerResponse.getBody();
    if (innerResponse.getHeader().status() == HttpStatus::OK)
    {
        // must return empty array if none is present.
        ASSERT_TRUE(document.HasMember("pushers")) << innerResponse.getBody();
        const auto& pushersJson = document["pushers"];
        ASSERT_TRUE(pushersJson.IsArray());
        for (auto& pusherJson : pushersJson.GetArray())
        {
            outPusher.emplace_back(pusherJson);
        }
    }
    else if (innerResponse.getHeader().status() == HttpStatus::BadRequest)
    {
        ASSERT_TRUE(document["errorCode"].IsString()) << innerResponse.getBody();
    }
    outInnerResponse = innerResponse;
}
void ErpWorkflowPushTest::comparePushers(const model::Pusher& expected, const model::Pusher& actual)
{
    EXPECT_EQ(expected.pushKey().value, actual.pushKey().value);
    EXPECT_EQ(expected.appId().value, actual.appId().value);
    EXPECT_EQ(expected.appDisplayName(), actual.appDisplayName());
    EXPECT_EQ(expected.deviceDisplayName(), actual.deviceDisplayName());
    EXPECT_EQ(expected.lang().value, actual.lang().value);
    EXPECT_EQ(expected.pusherData().mUrl, actual.pusherData().mUrl);
    // Pusher.encryption does not come back from the server
    compareUserData(expected.pusherData().mUserData, actual.pusherData().mUserData);
}
void ErpWorkflowPushTest::compareUserData(std::shared_ptr<rapidjson::Document> lhs,
                                          std::shared_ptr<rapidjson::Document> rhs)
{
    EXPECT_EQ(lhs == nullptr, rhs == nullptr);
    if (lhs != nullptr)
    {
        EXPECT_EQ(jsonToString(*lhs), jsonToString(*rhs));
    }
}
void ErpWorkflowPushTest::getChannels(const std::optional<model::PushKey>& pushkey, model::Channels& outChannels,
                                      ClientResponse& outInnerResponse)
{
    const RequestArguments requestArguments{HttpMethod::GET,
                                            "/channels/v1" + (pushkey.has_value() ? "/" + UrlHelper::escapeUrl(pushkey->value) : ""), ""};
    auto [outerResponse, innerResponse] = send(requestArguments);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::OK);
    outChannels.processUpdate(innerResponse.getBody());
    outInnerResponse = innerResponse;

    rapidjson::Document document;
    ASSERT_NO_THROW(document.Parse(innerResponse.getBody()));
    ASSERT_FALSE(document.HasParseError()) << innerResponse.getBody();
    ASSERT_TRUE(document.HasMember("channels"));
    const auto& channelsJson = document["channels"];
    ASSERT_TRUE(channelsJson.IsArray());
    ASSERT_EQ(channelsJson.Size(), 13) << innerResponse.getBody();
}
void ErpWorkflowPushTest::updateChannels(const model::PushKey& pushkey, const model::Channels& channels,
                                         ClientResponse& outInnerResponse)
{
    auto body = channels.serializeToJsonString();
    updateChannels(pushkey, body, outInnerResponse);
}
void ErpWorkflowPushTest::updateChannels(const model::PushKey& pushkey, const std::string& body,
                                         ClientResponse& outInnerResponse)
{
    const RequestArguments requestArguments{HttpMethod::POST, "/channels/v1/" + UrlHelper::escapeUrl(pushkey.value), body,
                                            ContentMimeType::jsonUtf8};
    auto [outerResponse, innerResponse] = send(requestArguments);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::OK);
    outInnerResponse = innerResponse;
}

TEST_F(ErpWorkflowPushTest, registerUpdateDelete)
{
    // register
    A_27154.test("App-Registrierung anlegen");
    const model::Pusher pusher{
        model::PushKey{pushKey},
        model::AppId{"appid"},
        "app display name",
        "device display name",
        model::Lang{"de"},
        model::PusherData{"https://push-gateway.location.here/push/v1/", "format"},
        model::Encryption{"2026-06", SafeString("iss11111111111111111111111111111"), "keyIdentifier"}};
    ClientResponse innerResponse;
    ASSERT_NO_THROW(pushRegister(pusher, innerResponse));
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::OK);

    std::vector<TestPusher> pushers;
    ASSERT_NO_THROW(pushGet(pushers, innerResponse));
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::OK);
    EXPECT_EQ(pushers.size(), 1);
    comparePushers(pusher, pushers.at(0));

    // update
    A_27155.test("App-Registrierung aktualisieren");
    const model::Pusher updatedPusher{
        model::PushKey{pushKey},
        model::AppId{"appid"},
        "updated app display name",
        "updated device display name",
        model::Lang{"en"},
        model::PusherData{"https://push-gateway.location.here/push/v1/", "format"},
        model::Encryption{"2026-07", SafeString("iss11111111111111111111111111112"), "updated-keyIdentifier"}};
    ASSERT_NO_THROW(pushRegister(updatedPusher, innerResponse));
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::OK);

    pushers.clear();
    ASSERT_NO_THROW(pushGet(pushers, innerResponse));
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::OK);
    EXPECT_EQ(pushers.size(), 1);
    comparePushers(updatedPusher, pushers.at(0));

    // delete
    A_27156.test("App-Registrierung löschen");
    A_27197_01.test("App-Registrierung löschen");
    pushDelete(model::DeletePusher{model::PushKey{pushKey}, model::AppId{"appid"}}, innerResponse);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::OK);

    pushers.clear();
    ASSERT_NO_THROW(pushGet(pushers, innerResponse));
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::OK);
    EXPECT_EQ(pushers.size(), 0);
}

TEST_F(ErpWorkflowPushTest, notAcceptable)
{
    {
        RequestArguments requestArguments{HttpMethod::GET, "/pushers/v1", ""};
        requestArguments.headerFields.emplace(Header::Accept, "application/xml");
        auto [outerResponse, innerResponse] = send(requestArguments);
        EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::NotAcceptable);
    }

    {
        RequestArguments requestArguments{HttpMethod::GET, "/pushers/v1", ""};
        requestArguments.headerFields.emplace(Header::Accept, "application/fhir+json");
        auto [outerResponse, innerResponse] = send(requestArguments);
        EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::NotAcceptable);
    }
}

TEST_F(ErpWorkflowPushTest, badRequest)
{
    const model::Pusher pusher{
        model::PushKey{pushKey},
        model::AppId{"appid"},
        "app display name",
        "device display name",
        model::Lang{"de"},
        model::PusherData{"https://push-gateway.location.here/push/v1/", "format"},
        model::Encryption{"2026-06-01", SafeString("iss11111111111111111111111111111"), "keyIdentifier"}};
    ClientResponse innerResponse;
    ASSERT_NO_THROW(pushRegister(pusher, innerResponse));
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::BadRequest);
    std::cout << innerResponse.getBody() << std::endl;
}

TEST_F(ErpWorkflowPushTest, forbidden)
{
    RequestArguments requestArguments{HttpMethod::GET, "/pushers/v1", ""};
    requestArguments.headerFields.emplace(Header::Accept, "application/json");
    requestArguments.jwt = JwtBuilder::testBuilder().makeJwtArzt();
    auto [outerResponse, innerResponse] = send(requestArguments);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::Forbidden);
    rapidjson::Document document;
    ASSERT_NO_THROW(document.Parse(innerResponse.getBody()));
    ASSERT_FALSE(document.HasParseError()) << innerResponse.getBody();
    ASSERT_TRUE(document["errorCode"].IsString()) << innerResponse.getBody();
}

TEST_F(ErpWorkflowPushTest, getChannels)
{
    model::Channels channels;
    ClientResponse innerResponse;
    getChannels(std::nullopt, channels, innerResponse);
    ASSERT_EQ(channels.activeChannels().size(), 0);
}

TEST_F(ErpWorkflowPushTest, updateChannels)
{
    A_27193.test("Liste der channel_ids des Geräts anlegen");
    const model::PushKey pushkey{pushKey};
    const model::Pusher pusher{
        pushkey,
        model::AppId{"appid"},
        "app display name",
        "device display name",
        model::Lang{"de"},
        model::PusherData{"https://push-gateway.location.here/push/v1/", "format"},
        model::Encryption{"2026-06", SafeString("iss11111111111111111111111111111"), "keyIdentifier"}};
    ClientResponse innerResponse;
    ASSERT_NO_THROW(pushRegister(pusher, innerResponse));
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::OK);

    {
        model::Channels channels;
        getChannels(pushkey, channels, innerResponse);
        ASSERT_EQ(channels.activeChannels().size(), 0);
    }

    {
        model::Channels channels;
        channels.add(model::ChannelId::erp_task_abort);
        channels.add(model::ChannelId::erp_task_vertreter);
        channels.add(model::ChannelId::erp_chargeitem_create);
        channels.add(model::ChannelId::erp_eu_prescription_close);
        updateChannels(pushkey, channels, innerResponse);
    }
    {
        model::Channels channels;
        getChannels(pushkey, channels, innerResponse);
        ASSERT_EQ(channels.activeChannels().size(), 4);
        EXPECT_TRUE(channels.activeChannels().contains(model::ChannelId::erp_task_abort));
        EXPECT_TRUE(channels.activeChannels().contains(model::ChannelId::erp_task_vertreter));
        EXPECT_TRUE(
            channels.activeChannels().contains(model::ChannelId::erp_chargeitem_create));
        EXPECT_TRUE(
            channels.activeChannels().contains(model::ChannelId::erp_eu_prescription_close));
    }
    {
        std::string body =
            R"({"channels":[{"id":"erp.task.vertreter","status":"disabled" },{ "id":"erp.task.abort","status":"enabled"},{"id":"erp.eu.prescription.redeem","status":"enabled"}]})";
        updateChannels(pushkey, body, innerResponse);
    }
    {
        model::Channels channels;
        getChannels(pushkey, channels, innerResponse);
        ASSERT_EQ(channels.activeChannels().size(), 4);
        EXPECT_TRUE(channels.activeChannels().contains(model::ChannelId::erp_task_abort));
        EXPECT_TRUE(
            channels.activeChannels().contains(model::ChannelId::erp_eu_prescription_redeem));
        EXPECT_TRUE(
            channels.activeChannels().contains(model::ChannelId::erp_chargeitem_create));
        EXPECT_TRUE(
            channels.activeChannels().contains(model::ChannelId::erp_eu_prescription_close));
    }

    {
        model::Channels channels;
        updateChannels(pushkey, channels, innerResponse);
    }
    {
        model::Channels channels;
        getChannels(pushkey, channels, innerResponse);
        ASSERT_EQ(channels.activeChannels().size(), 0);
    }
}

TEST_F(ErpWorkflowPushTest, noAuditEventNothingDeleted)
{
    auto kvnr = generateNewRandomKVNR();
    model::Timestamp startTime = model::Timestamp::now();
    ClientResponse innerResponse;
    pushDelete(model::DeletePusher{model::PushKey{pushKey}, model::AppId{"appid"}}, innerResponse);
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::OK);
    std::optional<model::Bundle> auditEventBundle;
    ASSERT_NO_FATAL_FAILURE(
        auditEventBundle =
            auditEventGet(kvnr.id(), "de",
                          "date=ge" + startTime.toXsDateTimeWithoutFractionalSeconds().substr(0, 19) + "Z&_sort=date"));
    EXPECT_TRUE(auditEventBundle.has_value());
    EXPECT_EQ(0, auditEventBundle->getResourceCount()) << auditEventBundle->serializeToJsonString();
}

TEST_F(ErpWorkflowPushTest, auditEventRegisterUnregister)
{
    auto kvnr = generateNewRandomKVNR();
    model::Timestamp startTime = model::Timestamp::now();
    const model::Pusher pusher{
        model::PushKey{pushKey},
        model::AppId{"appid"},
        "app display name",
        "device display name",
        model::Lang{"de"},
        model::PusherData{"https://push-gateway.location.here/push/v1/", "format"},
        model::Encryption{"2026-06", SafeString("iss11111111111111111111111111111"), "keyIdentifier"}};
    ClientResponse innerResponse;
    ASSERT_NO_THROW(pushRegister(pusher, innerResponse, kvnr.id()));
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::OK);
    checkAuditEvents(std::vector<std::optional<std::string>>{"device display name"}, kvnr.id(), "de", startTime,
                     std::vector<std::string>{{kvnr.id()}}, {},
                     std::vector<model::AuditEvent::SubType>{model::AuditEvent::SubType::create}, true);

    pushDelete(model::DeletePusher{model::PushKey{pushKey}, model::AppId{"appid"}}, innerResponse, kvnr.id());
    EXPECT_EQ(innerResponse.getHeader().status(), HttpStatus::OK);
    checkAuditEvents(
        std::vector<std::optional<std::string>>{"device display name", "device display name"}, kvnr.id(), "de",
        startTime, std::vector<std::string>{kvnr.id(), kvnr.id()}, {},
        std::vector<model::AuditEvent::SubType>{model::AuditEvent::SubType::create, model::AuditEvent::SubType::del},
        true);
}
