/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/push/Pusher.hxx"
#include "shared/util/ByteHelper.hxx"
#include "test/util/ErpMacros.hxx"

#undef Expect

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

class PusherTest : public testing::Test
{
protected:
    void SetUp() override
    {
        document = doc();
    }

    rapidjson::Document doc()
    {

        const std::string pusherJson = R"({
    "lang": "en",
    "kind": "http",
    "app_display_name": "Mat Rix",
    "device_display_name": "iPhone 9",
    "app_id": "com.example.app.ios",
    "pushkey": "<APNS/GCM TOKEN>",
    "data": {
        "url": "https://push-gateway.location.here/push/v1/",
        "format": "format"
    },
    "encryption": {
        "method": "aes-hmac-sha256",
        "time_iss_created": "2023-10",
        "iss": "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
        "key_identifier": "f47ac10b-58cc-4372-a567-0e02b2c3d479"
    },
    "append": false
})";
        rapidjson::Document doc;
        doc.Parse(pusherJson);
        return doc;
    }

    rapidjson::Document document;
};

TEST_F(PusherTest, parse)
{
    std::optional<model::PushKind> pushKind;
    ASSERT_NO_THROW(pushKind.emplace(model::Pusher::parsePushKind(document)));
    ASSERT_TRUE(pushKind.has_value());
    EXPECT_EQ(*pushKind, model::PushKind::http);


    std::optional<model::Pusher> pusher;
    ASSERT_NO_THROW(pusher.emplace(model::Pusher::parse(document)));
    ASSERT_TRUE(pusher.has_value());

    EXPECT_EQ(pusher->lang().value, "en");
    EXPECT_EQ(pusher->appDisplayName(), "Mat Rix");
    EXPECT_EQ(pusher->deviceDisplayName(), "iPhone 9");
    EXPECT_EQ(pusher->appId().value, "com.example.app.ios");
    EXPECT_EQ(pusher->pushKey().value, "<APNS/GCM TOKEN>");
    EXPECT_EQ(pusher->pusherData().mUrl, "https://push-gateway.location.here/push/v1/");
    EXPECT_EQ(pusher->encryption().timeIssCreated, "2023-10");
    const std::string expectedIss =
        ByteHelper::fromHex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    EXPECT_EQ(std::string{std::string_view{pusher->encryption().iss}}, expectedIss);
    EXPECT_EQ(pusher->encryption().keyIdentifier, "f47ac10b-58cc-4372-a567-0e02b2c3d479");
}

TEST_F(PusherTest, parseDelete)
{
    const std::string pusherJson = R"({
    "app_id": "com.example.app.ios",
    "pushkey": "<APNS/GCM TOKEN>",
    "kind": null
})";

    rapidjson::Document document;
    document.Parse(pusherJson);

    std::optional<model::PushKind> pushKind;
    ASSERT_NO_THROW(pushKind.emplace(model::Pusher::parsePushKind(document)));
    ASSERT_TRUE(pushKind.has_value());
    EXPECT_EQ(*pushKind, model::PushKind::null);

    std::optional<model::DeletePusher> pusher;
    ASSERT_NO_THROW(pusher.emplace(model::DeletePusher::parse(document)));
    ASSERT_TRUE(pusher.has_value());

    EXPECT_EQ(pusher->appId().value, "com.example.app.ios");
    EXPECT_EQ(pusher->pushKey().value, "<APNS/GCM TOKEN>");
}

TEST_F(PusherTest, missingMandatory)
{
    auto docWithRemoved = [this](const std::string& member) {
        auto document = doc();
        rapidjson::Pointer{member}.Erase(document);
        return document;
    };
    EXPECT_EXCEPTION_WITH_MESSAGE(model::Pusher::parsePushKind(docWithRemoved("/kind")),
                                  model::MissingParameterException, "Pusher.kind is missing");

    EXPECT_EXCEPTION_WITH_MESSAGE(model::Pusher::parse(docWithRemoved("/lang")), model::MissingParameterException,
                                  "member lang is missing");
    EXPECT_EXCEPTION_WITH_MESSAGE(model::Pusher::parse(docWithRemoved("/app_display_name")),
                                  model::MissingParameterException, "member app_display_name is missing");
    EXPECT_EXCEPTION_WITH_MESSAGE(model::Pusher::parse(docWithRemoved("/device_display_name")),
                                  model::MissingParameterException, "member device_display_name is missing");
    EXPECT_EXCEPTION_WITH_MESSAGE(model::Pusher::parse(docWithRemoved("/app_id")), model::MissingParameterException,
                                  "member app_id is missing");
    EXPECT_EXCEPTION_WITH_MESSAGE(model::Pusher::parse(docWithRemoved("/pushkey")), model::MissingParameterException,
                                  "member pushkey is missing");
    EXPECT_EXCEPTION_WITH_MESSAGE(model::Pusher::parse(docWithRemoved("/data")), model::MissingParameterException,
                                  "member data is missing");
    EXPECT_EXCEPTION_WITH_MESSAGE(model::Pusher::parse(docWithRemoved("/data/url")), model::MissingParameterException,
                                  "member url is missing");
    EXPECT_NO_THROW(model::Pusher::parse(docWithRemoved("/data/format")));
    EXPECT_EXCEPTION_WITH_MESSAGE(model::Pusher::parse(docWithRemoved("/encryption")), model::MissingParameterException,
                                  "member encryption is missing");
    EXPECT_EXCEPTION_WITH_MESSAGE(model::Pusher::parse(docWithRemoved("/encryption/method")),
                                  model::MissingParameterException, "member method is missing");
    EXPECT_EXCEPTION_WITH_MESSAGE(model::Pusher::parse(docWithRemoved("/encryption/time_iss_created")),
                                  model::MissingParameterException, "member time_iss_created is missing");
    EXPECT_EXCEPTION_WITH_MESSAGE(model::Pusher::parse(docWithRemoved("/encryption/iss")),
                                  model::MissingParameterException, "member iss is missing");
    EXPECT_EXCEPTION_WITH_MESSAGE(model::Pusher::parse(docWithRemoved("/encryption/key_identifier")),
                                  model::MissingParameterException, "member key_identifier is missing");
    EXPECT_NO_THROW(model::Pusher::parse(docWithRemoved("/append")));
}

TEST_F(PusherTest, missingMandatoryDelete)
{
    auto docWithRemoved = [](const std::string& member) {
        static const std::string pusherJson = R"({
    "app_id": "com.example.app.ios",
    "pushkey": "<APNS/GCM TOKEN>",
    "kind": null
})";
        rapidjson::Document document;
        document.Parse(pusherJson);
        rapidjson::Pointer{member}.Erase(document);
        return document;
    };
    EXPECT_EXCEPTION_WITH_MESSAGE(model::DeletePusher::parse(docWithRemoved("/app_id")),
                                  model::MissingParameterException, "member app_id is missing");
    EXPECT_EXCEPTION_WITH_MESSAGE(model::DeletePusher::parse(docWithRemoved("/pushkey")),
                                  model::MissingParameterException, "member pushkey is missing");
}

TEST_F(PusherTest, invalidMembers)
{
    auto docWithModified = [this](const std::string& member, const std::string& value) {
        auto document = doc();
        rapidjson::Pointer{member}.Set(document, rapidjson::StringRef(value));
        return document;
    };
    auto utf8Str = [](const std::string& c, const size_t count) {
        std::string result;
        result.reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            result.append(c);
        }
        return result;
    };

    EXPECT_EXCEPTION_WITH_MESSAGE(model::Pusher::parsePushKind(docWithModified("/kind", "email")),
                                  model::InvalidParameterException, "Could not parse Pusher.kind");

    EXPECT_NO_THROW(model::Pusher::parse(docWithModified("/pushkey", utf8Str("ö", 256))));
    EXPECT_EXCEPTION_WITH_MESSAGE(model::Pusher::parse(docWithModified("/pushkey", utf8Str("ö", 257))),
                                  model::InvalidParameterException, "PushKey must not exceed 512 bytes");
    EXPECT_EXCEPTION_WITH_MESSAGE(model::Pusher::parse(docWithModified("/pushkey", std::string(513, 'x'))),
                                  model::InvalidParameterException, "PushKey must not exceed 512 bytes");

    EXPECT_NO_THROW(model::Pusher::parse(docWithModified("/app_id", utf8Str("ö", 64))));
    EXPECT_EXCEPTION_WITH_MESSAGE(model::Pusher::parse(docWithModified("/app_id", utf8Str("ö", 65))),
                                  model::InvalidParameterException, "AppId must not exceed 64 characters");
    EXPECT_EXCEPTION_WITH_MESSAGE(model::Pusher::parse(docWithModified("/app_id", std::string(65, 'x'))),
                                  model::InvalidParameterException, "AppId must not exceed 64 characters");

    EXPECT_EXCEPTION_WITH_MESSAGE(
        model::Pusher::parse(docWithModified("/data/url", "http://push-gateway.location.here/push/v1/")),
        model::InvalidParameterException, "data.url MUST be an HTTPS URL with a path ending on /push/v1/");
    EXPECT_EXCEPTION_WITH_MESSAGE(
        model::Pusher::parse(docWithModified("/data/url", "https://push-gateway.location.here/push/v1")),
        model::InvalidParameterException, "data.url MUST be an HTTPS URL with a path ending on /push/v1/");
    EXPECT_EXCEPTION_WITH_MESSAGE(
        model::Pusher::parse(docWithModified("/data/url", "https://push-gateway.location.there/push/v1/")),
        model::InvalidParameterException, "Push Gateway push-gateway.location.there:443 is not configured");
    EXPECT_NO_THROW(
        model::Pusher::parse(docWithModified("/data/url", "https://push-gateway.location.here/_matrix/push/v1/")));

    EXPECT_EXCEPTION_WITH_MESSAGE(model::Pusher::parse(docWithModified("/encryption/method", "aes-hmac-sha512")),
                                  model::InvalidParameterException, "encryption.method must be aes-hmac-sha256");

    EXPECT_EXCEPTION_WITH_MESSAGE(model::Pusher::parse(docWithModified("/encryption/time_iss_created", "2026-05-01")),
                                  model::InvalidParameterException, "Invalid format for 2026-05-01 expected yyyy-MM");

    EXPECT_EXCEPTION_WITH_MESSAGE(
        model::Pusher::parse(
            docWithModified("/encryption/iss", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f1")),
        model::InvalidParameterException,
        "encryption.iss must be a string containing the hex representation of the 256 bit initial shared secret.");
    EXPECT_EXCEPTION_WITH_MESSAGE(
        model::Pusher::parse(
            docWithModified("/encryption/iss", "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1x1")),
        model::InvalidParameterException,
        "encryption.iss must be a string containing the hex representation of the 256 bit initial shared secret.");
}
