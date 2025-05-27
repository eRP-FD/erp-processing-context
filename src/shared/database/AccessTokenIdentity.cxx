/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/crypto/Jwt.hxx"
#include "shared/database/AccessTokenIdentity.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/JsonLog.hxx"
#include "shared/util/String.hxx"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include <regex>
#include <tuple>
#include <stdexcept>
#include <rapidjson/error/en.h>

namespace db_model
{

AccessTokenIdentity::AccessTokenIdentity(const JWT& jwt)
    : mId(jwt.stringForClaim(JWT::idNumberClaim).value_or(""))
    , mName(jwt.stringForClaim(JWT::organizationNameClaim).value_or("unbekannt"))
    , mOid(jwt.stringForClaim(JWT::professionOIDClaim).value_or(""))
{
    ModelExpect(! mId.id().empty() && ! mOid.empty(), "incomplete JWT, idNumber or professionOID missing");
}

AccessTokenIdentity::AccessTokenIdentity(model::TelematikId id, std::string_view name, std::string_view oid)
    : mId(std::move(id))
    , mName(name)
    , mOid(oid)
{
}

std::string AccessTokenIdentity::getJson() const
{
    rapidjson::Document d;
    d.SetObject();
    d.AddMember("id", mId.id(), d.GetAllocator());
    d.AddMember("name", mName, d.GetAllocator());
    d.AddMember("oid", mOid, d.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    d.Accept(writer);

    return buffer.GetString();
}

const model::TelematikId& AccessTokenIdentity::getId() const
{
    return mId;
}

const std::string& AccessTokenIdentity::getName() const
{
    return mName;
}

const std::string& AccessTokenIdentity::getOid() const
{
    return mOid;
}

std::optional<std::tuple<std::string, std::string, std::string>> extractStrangeJson(const std::string& input) {

    static const std::regex re(
       R"#(\{"id": "(.+)", "name": "(.+)", "oid": "([0-9.]+)"\})#"
   );

    std::smatch m;
    auto ret = std::regex_match(input, m, re);
    auto s = m.size();
    if (ret && s == 4) {
        return { {m[1].str(), m[2].str(), m[3].str()} };
    }
    return std::nullopt;
}

AccessTokenIdentity AccessTokenIdentity::fromJson(const std::string& json)
{
    rapidjson::Document doc;
    doc.Parse(json);
    if (doc.HasParseError())
    {
        const auto rescued_json = extractStrangeJson(json);
        if (rescued_json.has_value()) {
            const auto [id, name, oid] = *rescued_json;

            JsonLog(LogId::INFO, JsonLog::makeInfoLogReceiver(), false)
                .keyValue("event", "Extracted information from invalid custom JSON representation of JWT.")
                .keyValue("reason", "Invalid JSON");

            return {model::TelematikId(id), name, oid};
        }
        const auto error = doc.GetParseError();
        std::stringstream ss;
        ss << "invalid custom json representation of JWT - ";
        ss << rapidjson::GetParseError_En(error);
        ss << " at offset ";
        ss << doc.GetErrorOffset();
        Fail2(ss.str(), Exception);
    }
    return {model::TelematikId(std::string(doc["id"].GetString())),
                               std::string(doc["name"].GetString()), std::string(doc["oid"].GetString())};
}

AccessTokenIdentity::Exception::Exception(const std::string& what)
: std::runtime_error(what)
{
}

}// namespace db_model
