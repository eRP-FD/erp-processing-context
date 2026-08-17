/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "shared/util/SafeString.hxx"
#include "shared/util/UrlHelper.hxx"

#include <date/date.h>
#include <rapidjson/rapidjson.h>
#include <string>

namespace model
{

struct PushKey {
    explicit PushKey(std::string_view value);
    std::string value;
};

enum class PushKind
{
    http,
    null
};

struct AppId {
    explicit AppId(std::string_view value);
    std::string value;
};

struct Lang {
    explicit Lang(std::string_view value);
    Lang() = default;
    std::string value{};
};

struct PusherData {
    explicit PusherData(const rapidjson::Value& json, bool validate=true);
    explicit PusherData(std::string url, std::optional<std::string> userData);
    PusherData() = default;
    [[nodiscard]] rapidjson::Value toJson(rapidjson::Document::AllocatorType& alloc) const;
    [[nodiscard]] std::string fqdn() const;
    [[nodiscard]] std::string path() const;
    std::string mUrl{};
    std::shared_ptr<rapidjson::Document> mUserData{};
};

struct Encryption {
    explicit Encryption(const rapidjson::Value& json);
    Encryption() = default;
    Encryption(std::string timeIssCreated, SafeString iss, std::string keyIdentifier);
    std::string method{};
    std::string timeIssCreated{};
    SafeString iss{};
    std::string keyIdentifier{};
    [[nodiscard]] rapidjson::Value toJson(rapidjson::Document::AllocatorType& allocator) const;
    [[nodiscard]] bool isEmpty() const;
};

class Pusher
{
    // schema: https://raw.githubusercontent.com/gematik/gem-push-notifications-concept/refs/tags/1.2.0/docs_sources/definitions/pusher_post_put_delete.yaml#/schema
    // Pushers.append is ignored, see https://service.gematik.de/servicedesk/customer/portal/11/ANFERP-3740
public:
    static PushKind parsePushKind(const rapidjson::Value& jsonDocument);
    static Pusher parse(const rapidjson::Value& jsonDocument);
    static Pusher fromDb(const std::string& dbPayloadJson);

    Pusher(PushKey pushKey, AppId appId, std::string appDisplayName, std::string deviceDisplayName,
           Lang lang, PusherData pusherData, Encryption encryption);

    [[nodiscard]] const PushKey& pushKey() const;
    [[nodiscard]] const AppId& appId() const;
    [[nodiscard]] const std::string& appDisplayName() const;
    [[nodiscard]] const std::string& deviceDisplayName() const;
    [[nodiscard]] const Lang& lang() const;
    [[nodiscard]] const PusherData& pusherData() const;
    [[nodiscard]] const Encryption& encryption() const;

    [[nodiscard]] std::string serializeToJsonString() const;
    [[nodiscard]] std::string dbPayloadJson() const;

    void addTo(rapidjson::Value& object, rapidjson::Document::AllocatorType& allocator) const;

private:
    explicit Pusher(const rapidjson::Value& json);

    PushKey mPushKey;
    AppId mAppId;
    std::string mAppDisplayName{};
    std::string mDeviceDisplayName{};
    Lang mLang{};
    PusherData mPusherData{};
    Encryption mEncryption{};
};

class DeletePusher
{
    // schema: https://raw.githubusercontent.com/gematik/gem-push-notifications-concept/refs/tags/1.2.0/docs_sources/definitions/pusher_post_put_delete.yaml#/schema
public:
    static DeletePusher parse(const rapidjson::Value& jsonDocument);
    DeletePusher(PushKey pushKey, AppId appId);

    [[nodiscard]] const PushKey& pushKey() const;
    [[nodiscard]] const AppId& appId() const;

    [[nodiscard]] std::string serializeToJsonString() const;

private:
    explicit DeletePusher(const rapidjson::Value& json);
    PushKey mPushKey;
    AppId mAppId;
};

}// model
