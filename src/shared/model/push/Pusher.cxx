/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */


#include "Pusher.hxx"
#include "fhirtools/model/NumberAsStringParserWriter.hxx"
#include "shared/util/ByteHelper.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/String.hxx"

#include <charconv>
#include <utility>

namespace
{
const rapidjson::Value& getMember(const rapidjson::Value& json, const std::string& memberName)
{
    const auto it = json.FindMember(memberName);
    Expect3(it != json.MemberEnd(), "member " + std::string{memberName} + " is missing",
            model::MissingParameterException);
    return it->value;
}
std::string_view getStringMember(const rapidjson::Value& json, const std::string& memberName)
{
    const auto& memb = getMember(json, memberName);
    Expect3(memb.IsString(), "member " + std::string{memberName} + " is not a string",
            model::InvalidParameterException);
    return memb.GetString();
}

// GEMREQ-start A_27158
void validateYearMonth(const std::string& yearMonthString)
{
    date::year_month result{};
    std::istringstream iss(yearMonthString);
    iss >> date::parse("%Y-%m", result);
    Expect3(! iss.fail() && iss.peek() == std::char_traits<char>::eof(),
            "Invalid format for " + yearMonthString + " expected yyyy-MM", model::InvalidParameterException);
}
// GEMREQ-end A_27158
}

namespace model
{

PushKey::PushKey(std::string_view value)
    : value(value)
{
    Expect3(value.size() <= 512, "PushKey must not exceed 512 bytes", model::InvalidParameterException);
}

AppId::AppId(std::string_view value)
    : value(value)
{
    Expect3(String::utf8Length(value) <= 64, "AppId must not exceed 64 characters", model::InvalidParameterException);
}

Lang::Lang(std::string_view value)
    : value(value)
{
}

PusherData::PusherData(const rapidjson::Value& json, bool validate)
    : mUrl(getStringMember(json, "url"))
    , mUserData(std::make_shared<rapidjson::Document>())
{
    mUserData->SetObject();
    if (validate)
    {
        auto urlParts = UrlHelper::parseUrl(mUrl);
        Expect3(urlParts.isHttpsProtocol() && urlParts.mPath.ends_with("/push/v1/"),
                "data.url MUST be an HTTPS URL with a path ending on /push/v1/", model::InvalidParameterException);
        Expect3(Configuration::instance().pushGatewayFQDNs().contains({urlParts.mHost, urlParts.mPort}),
                fmt::format("Push Gateway {}:{} is not configured", urlParts.mHost, urlParts.mPort),
                model::InvalidParameterException);
    }
    // if given, the user data must be forwarded to the push gateway.
    mUserData->CopyFrom(json, mUserData->GetAllocator());
    mUserData->RemoveMember("url");
}

PusherData::PusherData(std::string url, std::optional<std::string> userData)
    : mUrl(std::move(url))
    , mUserData(std::make_shared<rapidjson::Document>())
{
    mUserData->SetObject();
    if (userData)
    {
        mUserData->Parse(*userData);
    }
}

rapidjson::Value PusherData::toJson(rapidjson::Document::AllocatorType& alloc) const
{
    rapidjson::Value result(rapidjson::kObjectType);
    if (! mUserData->ObjectEmpty())
    {
        result.CopyFrom(*mUserData, alloc);
    }
    result.AddMember("url", rapidjson::Value(mUrl, alloc), alloc);
    return result;
}

std::string PusherData::fqdn() const
{
    auto urlParts = UrlHelper::parseUrl(mUrl);
    return urlParts.mHost;
}

std::string PusherData::path() const
{
    auto urlParts = UrlHelper::parseUrl(mUrl);
    return urlParts.mPath;
}

Encryption::Encryption(const rapidjson::Value& json)
    : method(getStringMember(json, "method"))
    , timeIssCreated(getStringMember(json, "time_iss_created"))
    , keyIdentifier(getStringMember(json, "key_identifier"))
{
    SafeString issHex{std::string{getStringMember(json, "iss")}};
    validateYearMonth(timeIssCreated);
    // https://github.com/gematik/gem-push-notifications-concept/blob/1.2/docs_sources/definitions/pusher_post_put_delete.yaml
    // The encryption method to use. As currently only AES/GCM with a key generated using HKDF is supported, this value is static and should be 'aes-hmac-sha256'.
    Expect3(method == "aes-hmac-sha256", "encryption.method must be aes-hmac-sha256", model::InvalidParameterException);
    Expect3(issHex.size() == 64 && std::ranges::all_of(std::string_view{issHex.c_str()},
                                                       [](const unsigned char c) {
                                                           return std::isxdigit(c) != 0;
                                                       }),
            "encryption.iss must be a string containing the hex representation of the 256 bit initial shared secret.",
            model::InvalidParameterException);
    iss = ByteHelper::fromHex(issHex);
}

Encryption::Encryption(std::string timeIssCreated, SafeString iss, std::string keyIdentifier)
    : method("aes-hmac-sha256")
    , timeIssCreated(std::move(timeIssCreated))
    , iss(std::move(iss))
    , keyIdentifier(std::move(keyIdentifier))
{
}

rapidjson::Value Encryption::toJson(rapidjson::Document::AllocatorType& allocator) const
{
    rapidjson::Value result(rapidjson::kObjectType);
    result.AddMember("method", rapidjson::Value(method, allocator), allocator);
    result.AddMember("time_iss_created", rapidjson::Value(timeIssCreated, allocator), allocator);
    auto issHex = String::toHexString(iss);
    result.AddMember("iss", rapidjson::Value(issHex, allocator), allocator);
    result.AddMember("key_identifier", rapidjson::Value(keyIdentifier, allocator), allocator);
    return result;
}

bool Encryption::isEmpty() const
{
    return method.empty();
}

PushKind Pusher::parsePushKind(const rapidjson::Value& json)
{
    const auto it = json.FindMember("kind");
    Expect3(it != json.MemberEnd(), "Pusher.kind is missing", model::MissingParameterException);
    if (it->value.IsNull())
    {
        return PushKind::null;
    }
    Expect3(it->value.IsString(), "Pusher.kind is not a string", model::InvalidParameterException);
    const auto opt = magic_enum::enum_cast<PushKind>(it->value.GetString());
    Expect3(opt.has_value(), "Could not parse Pusher.kind", model::InvalidParameterException);
    return opt.value();
}

Pusher Pusher::parse(const rapidjson::Value& json)
{
    try
    {
        return Pusher{json};
    }
    catch (const ModelException&)
    {
        throw;
    }
    catch (const std::exception& ex)
    {
        ModelFail(ex.what());
    }
}

Pusher Pusher::fromDb(const std::string& dbPayloadJson)
{
    rapidjson::Document doc;
    doc.Parse(dbPayloadJson);
    Expect3(doc.IsObject(), "Could not parse db payload json", model::InvalidParameterException);
    auto lang = getStringMember(doc, "lang");
    auto appDisplayName = getStringMember(doc, "app_display_name");
    auto deviceDisplayName = getStringMember(doc, "device_display_name");
    auto appId = getStringMember(doc, "app_id");
    auto pushKey = getStringMember(doc, "pushkey");

    return Pusher{model::PushKey{pushKey},
                  model::AppId{appId},
                  std::string{appDisplayName},
                  std::string{deviceDisplayName},
                  Lang{lang},
                  PusherData{getMember(doc, "data"), false},
                  {}};
}

Pusher::Pusher(PushKey pushKey, AppId appId, std::string appDisplayName, std::string deviceDisplayName, Lang lang,
               PusherData pusherData, Encryption encryption)
    : mPushKey(std::move(pushKey))
    , mAppId(std::move(appId))
    , mAppDisplayName(std::move(appDisplayName))
    , mDeviceDisplayName(std::move(deviceDisplayName))
    , mLang(std::move(lang))
    , mPusherData(std::move(pusherData))
    , mEncryption(std::move(encryption))
{
}

const PushKey& Pusher::pushKey() const
{
    return mPushKey;
}
const AppId& Pusher::appId() const
{
    return mAppId;
}
const std::string& Pusher::appDisplayName() const
{
    return mAppDisplayName;
}
const std::string& Pusher::deviceDisplayName() const
{
    return mDeviceDisplayName;
}
const Lang& Pusher::lang() const
{
    return mLang;
}
const PusherData& Pusher::pusherData() const
{
    return mPusherData;
}
const Encryption& Pusher::encryption() const
{
    return mEncryption;
}
std::string Pusher::serializeToJsonString() const
{
    rapidjson::Document doc;
    doc.SetObject();
    addTo(doc, doc.GetAllocator());
    rapidjson::StringBuffer buffer;
    rapidjson::Writer writer(buffer);
    ModelExpect(doc.Accept(writer), "Could not serialize Pusher to json");
    return {buffer.GetString()};
}

std::string Pusher::dbPayloadJson() const
{
    rapidjson::Document doc;
    doc.SetObject();
    doc.AddMember("lang", rapidjson::StringRef(mLang.value), doc.GetAllocator());
    doc.AddMember("kind", rapidjson::StringRef("http"), doc.GetAllocator());
    doc.AddMember("app_display_name", rapidjson::StringRef(mAppDisplayName), doc.GetAllocator());
    doc.AddMember("device_display_name", rapidjson::StringRef(mDeviceDisplayName), doc.GetAllocator());
    doc.AddMember("data", mPusherData.toJson(doc.GetAllocator()), doc.GetAllocator());
    doc.AddMember("pushkey", rapidjson::StringRef(mPushKey.value), doc.GetAllocator());
    doc.AddMember("app_id", rapidjson::StringRef(mAppId.value), doc.GetAllocator());
    rapidjson::StringBuffer buffer;
    rapidjson::Writer writer(buffer);
    ModelExpect(doc.Accept(writer), "Could not serialize Pusher to payload json");
    return {buffer.GetString()};
}

void Pusher::addTo(rapidjson::Value& object, rapidjson::Document::AllocatorType& allocator) const
{
    object.AddMember("pushkey", rapidjson::StringRef(mPushKey.value), allocator);
    object.AddMember("kind", rapidjson::StringRef("http"), allocator);
    object.AddMember("app_id", rapidjson::StringRef(mAppId.value), allocator);
    object.AddMember("app_display_name", rapidjson::StringRef(mAppDisplayName), allocator);
    object.AddMember("device_display_name", rapidjson::StringRef(mDeviceDisplayName), allocator);
    object.AddMember("lang", rapidjson::StringRef(mLang.value), allocator);
    object.AddMember("data", mPusherData.toJson(allocator), allocator);
    // encryption is not rendered and not available when read from DB (write-only)
    if (! encryption().isEmpty())
    {
        object.AddMember("encryption", encryption().toJson(allocator), allocator);
    }
}

Pusher::Pusher(const rapidjson::Value& json)
    : mPushKey(getStringMember(json, "pushkey"))
    , mAppId(getStringMember(json, "app_id"))
    , mAppDisplayName(getStringMember(json, "app_display_name"))
    , mDeviceDisplayName(getStringMember(json, "device_display_name"))
    , mLang(getStringMember(json, "lang"))
    , mPusherData(getMember(json, "data"))
    , mEncryption(getMember(json, "encryption"))
{
}

DeletePusher DeletePusher::parse(const rapidjson::Value& jsonDocument)
{
    try
    {
        return DeletePusher{jsonDocument};
    }
    catch (const ModelException&)
    {
        throw;
    }
    catch (const std::exception& ex)
    {
        ModelFail(ex.what());
    }
}

DeletePusher::DeletePusher(PushKey pushKey, AppId appId)
    : mPushKey(std::move(pushKey))
    , mAppId(std::move(appId))
{
}

const PushKey& DeletePusher::pushKey() const
{
    return mPushKey;
}

const AppId& DeletePusher::appId() const
{
    return mAppId;
}

std::string DeletePusher::serializeToJsonString() const
{
    rapidjson::Document doc;
    doc.SetObject();
    doc.AddMember("app_id", rapidjson::StringRef(mAppId.value), doc.GetAllocator());
    doc.AddMember("pushkey", rapidjson::StringRef(mPushKey.value), doc.GetAllocator());
    doc.AddMember("kind", rapidjson::Value{rapidjson::kNullType}, doc.GetAllocator());
    rapidjson::StringBuffer buffer;
    rapidjson::Writer writer(buffer);
    ModelExpect(doc.Accept(writer), "Could not serialize DeletePusher to json");
    return buffer.GetString();
}

DeletePusher::DeletePusher(const rapidjson::Value& json)
    : mPushKey(getStringMember(json, "pushkey"))
    , mAppId(getStringMember(json, "app_id"))
{
}

}// model